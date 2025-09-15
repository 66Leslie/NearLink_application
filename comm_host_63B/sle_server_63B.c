/*
 * Copyright (c) 2024 HiSilicon Technologies CO., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sle_server_63B.h"
#include "securec.h"
#include "soc_osal.h"
#include "sle_errcode.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_ssap_server.h"
#include "cmsis_os2.h"
#include "common_def.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// 星闪服务配置
#define SLE_SERVER_NAME "sle_test"
#define SLE_MTU_SIZE_DEFAULT 512
#define SLE_ADV_HANDLE_DEFAULT 1

// UUID定义 - 使用官方标准UUID  
#define SLE_UUID_SERVER_SERVICE 0xABCD
#define SLE_UUID_SERVER_NTF_REPORT 0x1122

// 全局变量
static cargo_info_t g_cargo_info = {0};
static osMutexId_t g_cargo_mutex = NULL;
static uint16_t g_sle_conn_hdl = 0;
static uint8_t g_server_id = 0;
static uint16_t g_service_handle = 0;
static uint16_t g_property_handle = 0;
static bool g_sle_connected = false;

// 基础UUID设置
static uint8_t g_sle_base[] = {0x73, 0x6C, 0x65, 0x5F, 0x74, 0x65, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// UUID编码函数
static void encode2byte_little(uint8_t *ptr, uint16_t data)
{
    *(uint8_t *)((ptr) + 1) = (uint8_t)((data) >> 0x8);
    *(uint8_t *)(ptr) = (uint8_t)(data);
}

static void sle_uuid_set_base(sle_uuid_t *out)
{
    errcode_t ret = memcpy_s(out->uuid, SLE_UUID_LEN, g_sle_base, SLE_UUID_LEN);
    if (ret != EOK) {
        printf("[sle_server_63B] memcpy uuid fail\n");
        out->len = 0;
        return;
    }
    out->len = 2;
}

static void sle_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    sle_uuid_set_base(out);
    out->len = 2;
    encode2byte_little(&out->uuid[14], u2);
}

// 解析接收到的货物数据
static bool parse_cargo_data(const char *data, uint16_t len, cargo_info_t *cargo)
{
    if (data == NULL || cargo == NULL || len == 0) {
        return false;
    }
    
    // 拷贝数据到本地缓冲区确保字符串结束
    char buffer[256] = {0};
    if (len >= sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }
    memcpy_s(buffer, sizeof(buffer), data, len);
    buffer[len] = '\0';
    
    printf("[sle_server_63B] parsing cargo data: %s\r\n", buffer);
    
    // 解析格式: "J:xxx,Z:xxx,S:xxx,T:timestamp"
    uint32_t jiangsu = 0, zhejiang = 0, shanghai = 0;
    uint64_t timestamp = 0;
    
    char *token = strtok(buffer, ",");
    int parsed_count = 0;
    
    while (token != NULL && parsed_count < 4) {
        if (strncmp(token, "J:", 2) == 0) {
            jiangsu = (uint32_t)atoi(token + 2);
            parsed_count++;
        } else if (strncmp(token, "Z:", 2) == 0) {
            zhejiang = (uint32_t)atoi(token + 2);
            parsed_count++;
        } else if (strncmp(token, "S:", 2) == 0) {
            shanghai = (uint32_t)atoi(token + 2);
            parsed_count++;
        } else if (strncmp(token, "T:", 2) == 0) {
            timestamp = (uint64_t)atoll(token + 2);
            parsed_count++;
        }
        token = strtok(NULL, ",");
    }
    
    if (parsed_count >= 3) { // 至少要有J、Z、S三个数据
        cargo->jiangsu = jiangsu;
        cargo->zhejiang = zhejiang;
        cargo->shanghai = shanghai;
        cargo->timestamp = timestamp;
        cargo->valid = true;
        printf("[sle_server_63B] parsed cargo: J=%u, Z=%u, S=%u, T=%llu\r\n", 
               jiangsu, zhejiang, shanghai, timestamp);
        return true;
    }
    
    printf("[sle_server_63B] parse failed, parsed_count=%d\r\n", parsed_count);
    return false;
}

// 写入回调 - 接收客户端发送的货物数据
static void ssaps_write_request_cbk(uint8_t server_id, uint16_t conn_id, 
                                    ssaps_req_write_cb_t *write_cb_para, errcode_t status)
{
    printf("[sle_server_63B] ===== 写请求回调被触发 =====\r\n");
    printf("[sle_server_63B] server_id=%d, conn_id=%d, status=0x%x\r\n", 
           server_id, conn_id, status);
    
    if (status != ERRCODE_SUCC) {
        printf("[sle_server_63B] ❌ 写请求失败，状态码=0x%x\r\n", status);
        return;
    }
    
    if (write_cb_para == NULL) {
        printf("[sle_server_63B] write_cb_para is NULL\r\n");
        return;
    }
    
    printf("[sle_server_63B] write request: handle=0x%04x, type=%d, length=%d\r\n",
           write_cb_para->handle, write_cb_para->type, write_cb_para->length);
    
    if (write_cb_para->value == NULL || write_cb_para->length == 0) {
        printf("[sle_server_63B] invalid data: value=%p, length=%d\r\n", 
               write_cb_para->value, write_cb_para->length);
        return;
    }
    
    // 打印接收到的原始数据（用于调试）
    printf("[sle_server_63B] received raw data: ");
    for (uint16_t i = 0; i < write_cb_para->length && i < 64; i++) {
        printf("%c", write_cb_para->value[i]);
    }
    printf("\r\n");
    
    // 解析货物数据
    cargo_info_t new_cargo = {0};
    if (parse_cargo_data((const char *)write_cb_para->value, write_cb_para->length, &new_cargo)) {
        // 更新全局货物信息
        if (g_cargo_mutex != NULL) {
            osMutexAcquire(g_cargo_mutex, osWaitForever);
            g_cargo_info = new_cargo;
            osMutexRelease(g_cargo_mutex);
            
            printf("[sle_server_63B] ✓ Cargo data updated: J=%u, Z=%u, S=%u\r\n",
                   new_cargo.jiangsu, new_cargo.zhejiang, new_cargo.shanghai);
        } else {
            printf("[sle_server_63B] cargo mutex is NULL\r\n");
        }
    } else {
        printf("[sle_server_63B] ✗ Failed to parse cargo data\r\n");
    }
}

// 其他必要的回调函数
static void ssaps_add_service_cbk(uint8_t server_id, sle_uuid_t *uuid, uint16_t handle, errcode_t status)
{
    printf("[sle_server_63B] add service: server_id=%x, handle=%x, status=%x\r\n", server_id, handle, status);
    unused(uuid);
}

static void ssaps_add_property_cbk(uint8_t server_id, sle_uuid_t *uuid, uint16_t service_handle, 
                                   uint16_t handle, errcode_t status)
{
    printf("[sle_server_63B] add property: server_id=%x, service_handle=%x, handle=%x, status=%x\r\n", 
           server_id, service_handle, handle, status);
    unused(uuid);
}

static void ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    printf("[sle_server_63B] start service: server_id=%d, handle=%x, status=%x\r\n", server_id, handle, status);
}

// 连接状态变化回调
static void sle_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
                                          sle_acb_state_t conn_state, sle_pair_state_t pair_state,
                                          sle_disc_reason_t disc_reason)
{
    printf("[sle_server_63B] ===== 连接状态变化 =====\r\n");
    printf("[sle_server_63B] conn_id:0x%02x, state:0x%x, pair_state:0x%x, reason:0x%x\r\n", 
           conn_id, conn_state, pair_state, disc_reason);
    printf("[sle_server_63B] 客户端地址: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           addr->addr[0], addr->addr[1], addr->addr[2], addr->addr[3], addr->addr[4], addr->addr[5]);
    
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        g_sle_conn_hdl = conn_id;
        g_sle_connected = true;
        printf("[sle_server_63B] ✅ SLE连接成功，conn_id=0x%04x\r\n", conn_id);
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_sle_conn_hdl = 0;
        g_sle_connected = false;
        printf("[sle_server_63B] ❌ SLE连接断开，原因=0x%02x\r\n", disc_reason);
        
        // 重新开始广播
        printf("[sle_server_63B] 重新启动广播...\r\n");
        errcode_t ret = sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
        if (ret != ERRCODE_SUCC) {
            printf("[sle_server_63B] 重启广播失败:0x%x\r\n", ret);
        } else {
            printf("[sle_server_63B] 重启广播成功\r\n");
        }
    }
    unused(addr);
}

// 注册回调函数
static errcode_t sle_ssaps_register_cbks(void)
{
    ssaps_callbacks_t ssaps_cbk = {0};
    ssaps_cbk.add_service_cb = ssaps_add_service_cbk;
    ssaps_cbk.add_property_cb = ssaps_add_property_cbk;
    ssaps_cbk.start_service_cb = ssaps_start_service_cbk;
    ssaps_cbk.write_request_cb = ssaps_write_request_cbk;
    
    errcode_t ret = ssaps_register_callbacks(&ssaps_cbk);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] register callbacks fail:%x\r\n", ret);
        return ret;
    }
    return ERRCODE_SUCC;
}

static errcode_t sle_conn_register_cbks(void)
{
    sle_connection_callbacks_t conn_cbks = {0};
    conn_cbks.connect_state_changed_cb = sle_connect_state_changed_cbk;
    
    errcode_t ret = sle_connection_register_callbacks(&conn_cbks);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] conn register callbacks fail:%x\r\n", ret);
        return ret;
    }
    return ERRCODE_SUCC;
}

// 添加服务
static errcode_t sle_uuid_server_service_add(void)
{
    sle_uuid_t service_uuid = {0};
    sle_uuid_setu2(SLE_UUID_SERVER_SERVICE, &service_uuid);
    
    printf("[sle_server_63B] 正在添加服务，UUID=0x%04x\r\n", SLE_UUID_SERVER_SERVICE);
    errcode_t ret = ssaps_add_service_sync(g_server_id, &service_uuid, true, &g_service_handle);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] ❌ 添加服务失败, ret:0x%x\r\n", ret);
        return ERRCODE_FAIL;
    }
    
    printf("[sle_server_63B] ✅ 服务添加成功，句柄=0x%04x\r\n", g_service_handle);
    return ERRCODE_SUCC;
}

// 添加特征
static errcode_t sle_uuid_server_property_add(void)
{
    ssaps_property_info_t property = {0};
    
    printf("[sle_server_63B] 正在添加特征到服务句柄=0x%04x\r\n", g_service_handle);
    
    property.permissions = SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE;
    property.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &property.uuid);
    
    printf("[sle_server_63B] 特征权限: 读写=0x%02x, 操作指示=0x%02x, UUID=0x%04x\r\n",
           property.permissions, property.operate_indication, SLE_UUID_SERVER_NTF_REPORT);
    
    // 不设置初始值，避免内存问题
    property.value = NULL;
    property.value_len = 0;
    
    errcode_t ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property, &g_property_handle);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] ❌ 添加特征失败, ret:0x%x\r\n", ret);
        return ERRCODE_FAIL;
    }
    
    printf("[sle_server_63B] ✅ 特征添加成功，句柄=0x%04x\r\n", g_property_handle);
    return ERRCODE_SUCC;
}

// 创建服务器
static errcode_t sle_server_add(void)
{
    sle_uuid_t app_uuid = {0};
    app_uuid.len = 2;
    
    errcode_t ret = ssaps_register_server(&app_uuid, &g_server_id);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] register server fail, ret:%x\r\n", ret);
        return ERRCODE_FAIL;
    }
    
    if (sle_uuid_server_service_add() != ERRCODE_SUCC) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_FAIL;
    }
    
    if (sle_uuid_server_property_add() != ERRCODE_SUCC) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_FAIL;
    }
    
    printf("[sle_server_63B] server_id:%x, service_handle:%x, property_handle:%x\r\n", 
           g_server_id, g_service_handle, g_property_handle);
    
    ret = ssaps_start_service(g_server_id, g_service_handle);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] start service fail, ret:%x\r\n", ret);
        return ERRCODE_FAIL;
    }
    
    return ERRCODE_SUCC;
}

// 广播参数设置 - 参考官方教程
static errcode_t sle_server_set_announce_param(void)
{
    sle_announce_param_t param = {0};
    uint8_t mac[SLE_ADDR_LEN] = {0x04, 0x01, 0x06, 0x08, 0x06, 0x03};
    
    printf("[sle_server_63B] ===== 设置广播参数 =====\r\n");
    printf("[sle_server_63B] 服务器地址: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    param.announce_mode = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE;
    param.announce_handle = SLE_ADV_HANDLE_DEFAULT;
    param.announce_gt_role = SLE_ANNOUNCE_ROLE_T_CAN_NEGO;
    param.announce_level = SLE_ANNOUNCE_LEVEL_NORMAL;
    param.announce_channel_map = 0x07; // SLE_ADV_CHANNEL_MAP_DEFAULT
    param.announce_interval_min = 0xC8;  // 25ms
    param.announce_interval_max = 0xC8;  // 25ms
    param.conn_interval_min = 0x64;      // 12.5ms - 按官方demo标准
    param.conn_interval_max = 0x64;      // 12.5ms - 按官方demo标准
    param.conn_max_latency = 0x1F3;      // 按官方demo标准
    param.conn_supervision_timeout = 0x1F4; // 5000ms
    param.announce_tx_power = 20;
    param.own_addr.type = 0;
    memcpy_s(param.own_addr.addr, SLE_ADDR_LEN, mac, SLE_ADDR_LEN);
    
    printf("[sle_server_63B] 连接参数: interval=0x%x, latency=0x%x, timeout=0x%x\r\n",
           param.conn_interval_min, param.conn_max_latency, param.conn_supervision_timeout);
    
    errcode_t ret = sle_set_announce_param(param.announce_handle, &param);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] ❌ 设置广播参数失败:0x%x\r\n", ret);
        return ret;
    }
    
    printf("[sle_server_63B] ✅ 广播参数设置成功\r\n");
    return ERRCODE_SUCC;
}

// 设置广播数据 - 参考官方教程
static errcode_t sle_server_set_announce_data(void)
{
    sle_announce_data_t data = {0};
    uint8_t announce_data[32] = {0};
    uint8_t seek_rsp_data[32] = {0};
    uint16_t announce_idx = 0;
    uint16_t seek_idx = 0;
    
    // 设置广播数据
    announce_data[announce_idx++] = 2;  // length
    announce_data[announce_idx++] = 0x01; // SLE_ADV_DATA_TYPE_DISCOVERY_LEVEL
    announce_data[announce_idx++] = SLE_ANNOUNCE_LEVEL_NORMAL;
    
    announce_data[announce_idx++] = 2;  // length
    announce_data[announce_idx++] = 0x02; // SLE_ADV_DATA_TYPE_ACCESS_MODE
    announce_data[announce_idx++] = 0;
    
    // 设置扫描响应数据 - 设备名称
    seek_rsp_data[seek_idx++] = 16;  // length
    seek_rsp_data[seek_idx++] = 0x0B; // SLE_ADV_DATA_TYPE_COMPLETE_LOCAL_NAME
    memcpy_s(&seek_rsp_data[seek_idx], 32 - seek_idx, "CARGO_SERVER_63B", 16);
    seek_idx += 16;
    
    data.announce_data = announce_data;
    data.announce_data_len = announce_idx;
    data.seek_rsp_data = seek_rsp_data;
    data.seek_rsp_data_len = seek_idx;
    
    errcode_t ret = sle_set_announce_data(SLE_ADV_HANDLE_DEFAULT, &data);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] set announce data fail:%x\r\n", ret);
        return ret;
    }
    
    printf("[sle_server_63B] set announce data success\r\n");
    return ERRCODE_SUCC;
}

// 广播回调函数
static void sle_announce_enable_cbk(uint32_t announce_id, errcode_t status)
{
    printf("[sle_server_63B] announce enable id:%02x, status:%02x\r\n", announce_id, status);
}

static void sle_announce_disable_cbk(uint32_t announce_id, errcode_t status)
{
    printf("[sle_server_63B] announce disable id:%02x, status:%02x\r\n", announce_id, status);
}

static void sle_enable_cbk(errcode_t status)
{
    printf("[sle_server_63B] sle enable status:%02x\r\n", status);
}

// 注册广播回调
static errcode_t sle_server_announce_register_cbks(void)
{
    sle_announce_seek_callbacks_t seek_cbks = {0};
    seek_cbks.announce_enable_cb = sle_announce_enable_cbk;
    seek_cbks.announce_disable_cb = sle_announce_disable_cbk;
    seek_cbks.sle_enable_cb = sle_enable_cbk;
    
    errcode_t ret = sle_announce_seek_register_callbacks(&seek_cbks);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] announce register callbacks fail:%x\r\n", ret);
        return ret;
    }
    
    printf("[sle_server_63B] announce register callbacks success\r\n");
    return ERRCODE_SUCC;
}

// 广播初始化 - 按照官方教程流程
static errcode_t sle_server_adv_init(void)
{
    printf("[sle_server_63B] adv init start\r\n");
    
    // 注册广播回调
    errcode_t ret = sle_server_announce_register_cbks();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    
    // 设置广播参数
    ret = sle_server_set_announce_param();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    
    // 设置广播数据
    ret = sle_server_set_announce_data();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    
    // 开始广播
    ret = sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] start announce fail:%x\r\n", ret);
        return ret;
    }
    
    printf("[sle_server_63B] adv init success\r\n");
    return ERRCODE_SUCC;
}

// 设置SSAP信息
static errcode_t sle_server_set_ssap_info(void)
{
    ssap_exchange_info_t info = {0};
    info.mtu_size = SLE_MTU_SIZE_DEFAULT;
    info.version = 1;
    
    errcode_t ret = ssaps_set_info(g_server_id, &info);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] set ssap info fail:%x\r\n", ret);
        return ret;
    }
    
    printf("[sle_server_63B] set ssap info success\r\n");
    return ERRCODE_SUCC;
}

// 星闪服务器初始化 - 按照官方教程流程
errcode_t sle_server_63B_init(void)
{
    printf("[sle_server_63B] ===== 63B服务器初始化开始 =====\r\n");
    
    // 创建互斥锁
    g_cargo_mutex = osMutexNew(NULL);
    if (g_cargo_mutex == NULL) {
        printf("[sle_server_63B] ❌ 创建互斥锁失败\r\n");
        return ERRCODE_FAIL;
    }
    printf("[sle_server_63B] ✅ 互斥锁创建成功\r\n");
    
    // 1. 启用SLE
    printf("[sle_server_63B] 正在启用SLE协议栈...\r\n");
    errcode_t ret = enable_sle();
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] ❌ SLE启用失败:0x%x\r\n", ret);
        return ret;
    }
    printf("[sle_server_63B] ✅ SLE协议栈启用成功\r\n");
    
    // 2. 注册连接回调
    ret = sle_conn_register_cbks();
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] conn register callbacks fail:%x\r\n", ret);
        return ret;
    }
    
    // 3. 注册SSAP服务器回调
    ret = sle_ssaps_register_cbks();
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] ssaps register callbacks fail:%x\r\n", ret);
        return ret;
    }
    
    // 4. 添加服务器和服务
    ret = sle_server_add();
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] server add fail:%x\r\n", ret);
        return ret;
    }
    
    // 5. 初始化广播
    ret = sle_server_adv_init();
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] adv init fail:%x\r\n", ret);
        return ret;
    }
    
    // 6. 设置SSAP信息
    ret = sle_server_set_ssap_info();
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] set ssap info fail:%x\r\n", ret);
        return ret;
    }
    
    printf("[sle_server_63B] init success\r\n");
    return ERRCODE_SUCC;
}

// 获取货物信息
bool sle_server_get_cargo_info(cargo_info_t *cargo_info)
{
    if (cargo_info == NULL) {
        return false;
    }
    
    if (g_cargo_mutex == NULL) {
        return false;
    }
    
    osMutexAcquire(g_cargo_mutex, osWaitForever);
    *cargo_info = g_cargo_info;
    osMutexRelease(g_cargo_mutex);
    
    return g_cargo_info.valid;
}

// 发送货物数据到客户端
errcode_t sle_server_send_cargo_data(uint32_t jiangsu, uint32_t zhejiang, uint32_t shanghai)
{
    if (!g_sle_connected || g_sle_conn_hdl == 0) {
        printf("[sle_server_63B] not connected, cannot send data\r\n");
        return ERRCODE_FAIL;
    }
    
    // 构建货物数据包格式: "J:xxx,Z:xxx,S:xxx,T:timestamp"
    char msg[128] = {0};
    uint64_t timestamp = (uint64_t)osKernelGetTickCount();
    snprintf(msg, sizeof(msg), "J:%u,Z:%u,S:%u,T:%llu", 
             jiangsu, zhejiang, shanghai, timestamp);
    
    // 通过notify发送数据
    ssaps_ntf_ind_t param = {0};
    param.handle = g_property_handle;
    param.type = 0; // notification
    param.value = (uint8_t *)msg;
    param.value_len = (uint16_t)strlen(msg);
    
    errcode_t ret = ssaps_notify_indicate(g_server_id, g_sle_conn_hdl, &param);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_server_63B] send notify failed:0x%x\r\n", ret);
        return ret;
    }
    
    printf("[sle_server_63B] sent cargo data: %s\r\n", msg);
    return ERRCODE_SUCC;
}

// 获取连接状态
bool sle_server_is_connected(void)
{
    return g_sle_connected;
}
