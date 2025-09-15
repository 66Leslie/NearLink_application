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

#include "sle_client.h"
#include "common_def.h"
#include "sle_device_discovery.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"
#include "securec.h"
#include "cmsis_os2.h"
#include "soc_osal.h"
#include "uart.h"

// 官方星闪客户端实现 - 基于sle_02_trans_client

#define SLE_UART_BAUDRATE                   115200
#define SLE_UART_DATA_BITS                  UART_DATA_BIT_8
#define SLE_UART_STOP_BITS                  UART_STOP_BIT_1
#define SLE_UART_PARITY                     UART_PARITY_NONE

#define SLE_UART_TX_PIN                     16
#define SLE_UART_RX_PIN                     15
#define SLE_UART_BUS                        1

// 使用头文件中的定义，避免重复定义
// #define SLE_SEEK_INTERVAL_DEFAULT           0x100
// #define SLE_SEEK_WINDOW_DEFAULT             0x100
#define SLE_CONN_INTV_MIN_DEFAULT           0x64  // 12.5ms - 按官方demo标准
#define SLE_CONN_INTV_MAX_DEFAULT           0x64  // 12.5ms - 按官方demo标准  
#define SLE_CONN_MAX_LATENCY                0x1F3 // 按官方demo标准
#define SLE_CONN_SUPERVISION_TIMEOUT        0x1f4

#define SLE_MTU_SIZE_DEFAULT                512
#define SLE_TASK_DELAY_MS                   2000

// UUID定义 - 使用官方标准UUID
#define SLE_UUID_SERVER_SERVICE             0xABCD
#define SLE_UUID_SERVER_NTF_REPORT          0x1122

// 前向声明
static void sle_start_scan(void);
static void sle_client_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param, errcode_t status);
static void sle_client_find_property_cbk(uint8_t client_id, uint16_t conn_id, ssapc_find_property_result_t *property, errcode_t status);
static void sle_client_write_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_write_result_t *write_result, errcode_t status);

// 全局变量
static uint16_t g_sle_client_conn_id = 0;
static sle_acb_state_t g_sle_client_conn_state = SLE_ACB_STATE_NONE;
static uint16_t g_sle_client_ntf_id __attribute__((unused)) = 0;
static uint16_t g_sle_client_write_id = 0;
static uint16_t g_sle_client_server_id = 0;
static sle_addr_t g_sle_remote_addr = {0};
static ssapc_write_param_t g_sle_send_param = {0};

// 期望连接的服务器地址 - 需要与服务器端保持一致
static uint8_t g_sle_expected_addr[SLE_ADDR_LEN] = {0x04, 0x01, 0x06, 0x08, 0x06, 0x03};

// 星闪扫描结果回调
static void sle_seek_result_cb(sle_seek_result_info_t *seek_result_data)
{
    if (seek_result_data == NULL) {
        printf("[sle_client] seek result data is NULL\r\n");
        return;
    }
    
    printf("[sle_client] found device addr: %02x:%02x:%02x:%02x:%02x:%02x, rssi: %d\r\n",
           seek_result_data->addr.addr[0], seek_result_data->addr.addr[1], seek_result_data->addr.addr[2],
           seek_result_data->addr.addr[3], seek_result_data->addr.addr[4], seek_result_data->addr.addr[5],
           seek_result_data->rssi);
    
    printf("[sle_client] expected addr: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           g_sle_expected_addr[0], g_sle_expected_addr[1], g_sle_expected_addr[2],
           g_sle_expected_addr[3], g_sle_expected_addr[4], g_sle_expected_addr[5]);

    // 只连接期望的服务器地址
    if (memcmp((void *)seek_result_data->addr.addr, (void *)g_sle_expected_addr, SLE_ADDR_LEN) == 0) {
        printf("[sle_client] ✓ FOUND TARGET CARGO_SERVER_63B! Connecting...\r\n");
        
        // 停止扫描
        errcode_t ret = sle_stop_seek();
        if (ret != ERRCODE_SUCC) {
            printf("[sle_client] stop seek failed:0x%x\r\n", ret);
        }
        
        // 保存远程设备地址
        memcpy_s(&g_sle_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));
        
        // 连接到目标设备
        ret = sle_connect_remote_device(&seek_result_data->addr);
        if (ret != ERRCODE_SUCC) {
            printf("[sle_client] connect failed:0x%x, will retry scan\r\n", ret);
            osDelay(1000);
            sle_start_scan();
        } else {
            printf("[sle_client] connection request sent\r\n");
        }
    } else {
        printf("[sle_client] not target server (addr mismatch), continue scanning...\r\n");
    }
}

// 星闪连接状态变化回调
static void sle_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr, sle_acb_state_t conn_state,
                                          sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    printf("[sle_client] conn state changed: conn_id=0x%02x, state=0x%x, pair_state=0x%x, reason=0x%x\r\n",
           conn_id, conn_state, pair_state, disc_reason);
    printf("[sle_client] addr: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           addr->addr[0], addr->addr[1], addr->addr[2], addr->addr[3], addr->addr[4], addr->addr[5]);
    
    g_sle_client_conn_id = conn_id;
    
    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        printf("[sle_client] SLE connected successfully\r\n");
        g_sle_client_conn_state = SLE_ACB_STATE_CONNECTED;
        
        // 如果还没有配对，启动配对
        if (pair_state == SLE_PAIR_NONE) {
            printf("[sle_client] starting pairing...\r\n");
            sle_pair_remote_device(&g_sle_remote_addr);
        }
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        printf("[sle_client] SLE disconnected, reason:0x%02x\r\n", disc_reason);
        g_sle_client_conn_state = SLE_ACB_STATE_NONE;
        g_sle_client_write_id = 0; // 重置写句柄
        
        // 延迟后重新开始扫描
        printf("[sle_client] will restart scanning in 2 seconds...\r\n");
        osDelay(2000); // 延迟2秒后重新扫描
        sle_start_scan();
    }
}

// 配对完成回调
static void sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    printf("[sle_client] pair complete: conn_id=0x%02x, status=0x%x\r\n", conn_id, status);
    printf("[sle_client] pair addr: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           addr->addr[0], addr->addr[1], addr->addr[2], addr->addr[3], addr->addr[4], addr->addr[5]);
    
    if (status == ERRCODE_SUCC) {
        printf("[sle_client] pairing successful, starting MTU exchange...\r\n");
        // 发起MTU交换
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
        info.version = 1;
        ssapc_exchange_info_req(0, conn_id, &info); // 使用默认client_id 0
    } else {
        printf("[sle_client] pairing failed\r\n");
    }
}

// 解析来自服务器的货物数据
static bool parse_server_cargo_data(const char *data, uint16_t len, uint32_t *jiangsu, uint32_t *zhejiang, uint32_t *shanghai, uint64_t *timestamp)
{
    if (data == NULL || len == 0) {
        return false;
    }
    
    // 拷贝数据到本地缓冲区确保字符串结束
    char buffer[256] = {0};
    if (len >= sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }
    memcpy_s(buffer, sizeof(buffer), data, len);
    buffer[len] = '\0';
    
    printf("[sle_client] parsing server cargo data: %s\r\n", buffer);
    
    // 解析格式: "J:xxx,Z:xxx,S:xxx,T:timestamp"
    uint32_t js = 0, zj = 0, sh = 0;
    uint64_t ts = 0;
    
    char *token = strtok(buffer, ",");
    int parsed_count = 0;
    
    while (token != NULL && parsed_count < 4) {
        if (strncmp(token, "J:", 2) == 0) {
            js = (uint32_t)atoi(token + 2);
            parsed_count++;
        } else if (strncmp(token, "Z:", 2) == 0) {
            zj = (uint32_t)atoi(token + 2);
            parsed_count++;
        } else if (strncmp(token, "S:", 2) == 0) {
            sh = (uint32_t)atoi(token + 2);
            parsed_count++;
        } else if (strncmp(token, "T:", 2) == 0) {
            ts = (uint64_t)atoll(token + 2);
            parsed_count++;
        }
        token = strtok(NULL, ",");
    }
    
    if (parsed_count >= 3) { // 至少要有J、Z、S三个数据
        *jiangsu = js;
        *zhejiang = zj;
        *shanghai = sh;
        *timestamp = ts;
        printf("[sle_client] parsed server cargo: J=%u, Z=%u, S=%u, T=%llu\r\n", js, zj, sh, ts);
        return true;
    }
    
    printf("[sle_client] parse failed, parsed_count=%d\r\n", parsed_count);
    return false;
}

// 星闪数据接收回调
static void sle_ssapc_data_received_cbk(uint8_t client_id, uint16_t conn_id, ssapc_handle_value_t *data,
                                        errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    
    if (status != ERRCODE_SUCC) {
        printf("[sle_client] data received with error: 0x%x\r\n", status);
        return;
    }
    
    if (data != NULL && data->data_len > 0) {
        printf("[sle_client] received data len:%d\r\n", data->data_len);
        
        // 解析接收到的货物数据
        uint32_t jiangsu, zhejiang, shanghai;
        uint64_t timestamp;
        if (parse_server_cargo_data((const char *)data->data, data->data_len, 
                                   &jiangsu, &zhejiang, &shanghai, &timestamp)) {
            printf("[sle_client] received cargo data from 63B: J=%u, Z=%u, S=%u\r\n", 
                   jiangsu, zhejiang, shanghai);
            
            // 这里可以添加处理逻辑，例如更新本地数据或同步到其他系统
            // 可以调用外部函数来更新WS63的本地货物数据
        }
    }
}

// 开始扫描
static void sle_start_scan(void)
{
    sle_seek_param_t param = {0};
    param.own_addr_type = 0;
    param.filter_duplicates = 0; // 不过滤重复设备，确保能扫描到目标
    param.seek_filter_policy = 0;
    param.seek_phys = 1;
    param.seek_type[0] = 0; // 被动扫描
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;
    
    errcode_t ret = sle_set_seek_param(&param);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_client] set seek param failed:0x%x\r\n", ret);
        return;
    }
    
    ret = sle_start_seek();
    if (ret != ERRCODE_SUCC) {
        printf("[sle_client] start seek failed:0x%x\r\n", ret);
        return;
    }
    
    printf("[sle_client] start scan success, searching for CARGO_SERVER_63B...\r\n");
}

// 发送货物数据到服务器
void sle_client_send_cargo_data(uint32_t jiangsu, uint32_t zhejiang, uint32_t shanghai)
{
    if (g_sle_client_conn_state != SLE_ACB_STATE_CONNECTED) {
        printf("[sle_client] not connected, cannot send cargo data\r\n");
        return;
    }

    // 检查写句柄是否已设置
    if (g_sle_client_write_id == 0) {
        printf("[sle_client] 错误：写句柄未设置，服务发现可能未完成\r\n");
        return;
    }

    // 构建货物数据包格式: "J:xxx,Z:xxx,S:xxx,T:timestamp"
    char msg[128] = {0};
    uint64_t timestamp = (uint64_t)osKernelGetTickCount();
    snprintf(msg, sizeof(msg), "J:%u,Z:%u,S:%u,T:%llu", 
             jiangsu, zhejiang, shanghai, timestamp);

    // 添加详细调试信息
    printf("[sle_client] 准备发送数据：\r\n");
    printf("  连接ID: 0x%04x\r\n", g_sle_client_conn_id);
    printf("  写句柄: 0x%04x\r\n", g_sle_client_write_id);
    printf("  数据长度: %d\r\n", (int)strlen(msg));
    printf("  数据内容: %s\r\n", msg);

    // 使用全局发送参数，句柄由特征发现回调设置
    g_sle_send_param.handle = g_sle_client_write_id;
    g_sle_send_param.type = SSAP_PROPERTY_TYPE_VALUE;
    g_sle_send_param.data_len = (uint16_t)strlen(msg);
    g_sle_send_param.data = (uint8_t *)msg; // 注意：API会拷贝数据

    errcode_t ret = ssapc_write_req(0, g_sle_client_conn_id, &g_sle_send_param);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_client] 发送失败，错误代码:0x%x\r\n", ret);
    } else {
        printf("[sle_client] 发送请求已提交: %s\r\n", msg);
    }
}

// 服务发现完成回调
static void sle_ssapc_find_structure_cbk(uint8_t client_id, uint16_t conn_id,
                                          ssapc_find_service_result_t *service, errcode_t status)
{
    printf("[sle_client] find structure cbk: status=%d\r\n", status);
    
    if (status != ERRCODE_SUCC || service == NULL) {
        printf("[sle_client] service discovery failed\r\n");
        return;
    }
    
    printf("[sle_client] found service: start_hdl=0x%04x, end_hdl=0x%04x, uuid_len=%d\r\n",
           service->start_hdl, service->end_hdl, service->uuid.len);
    
    // 检查是否是我们期望的服务UUID  
    printf("[sle_client] 检查服务UUID，长度=%d\r\n", service->uuid.len);
    
    // 支持16字节完整UUID和2字节短UUID
    uint16_t service_uuid = 0;
    bool uuid_match = false;
    
    if (service->uuid.len == 2) {
        // 2字节短UUID
        service_uuid = (service->uuid.uuid[15] << 8) | service->uuid.uuid[14];
        printf("[sle_client] 短UUID: 0x%04x\r\n", service_uuid);
        uuid_match = (service_uuid == SLE_UUID_SERVER_SERVICE);
    } else if (service->uuid.len == 16) {
        // 16字节完整UUID，检查最后2字节
        service_uuid = (service->uuid.uuid[15] << 8) | service->uuid.uuid[14];
        printf("[sle_client] 完整UUID最后2字节: 0x%04x\r\n", service_uuid);
        uuid_match = (service_uuid == SLE_UUID_SERVER_SERVICE);
    } else {
        printf("[sle_client] UUID长度不支持: %d\r\n", service->uuid.len);
    }
    
    if (uuid_match) {
        printf("[sle_client] ✅ 找到货物服务，开始发现特征...\r\n");
        g_sle_client_server_id = service->start_hdl;
        
        // 发现特征
        ssapc_find_structure_param_t find_param = {0};
        find_param.type = SSAP_FIND_TYPE_PROPERTY;
        find_param.start_hdl = service->start_hdl;
        find_param.end_hdl = service->end_hdl;
        
        printf("[sle_client] 发起特征发现: start_hdl=0x%04x, end_hdl=0x%04x\r\n", 
               find_param.start_hdl, find_param.end_hdl);
        
        errcode_t ret = ssapc_find_structure(client_id, conn_id, &find_param);
        if (ret != ERRCODE_SUCC) {
            printf("[sle_client] ❌ 特征发现请求失败:0x%x\r\n", ret);
        } else {
            printf("[sle_client] ✅ 特征发现请求已发送\r\n");
        }
    } else {
        printf("[sle_client] 不是目标服务 (UUID=0x%04x, 期望=0x%04x)\r\n", 
               service_uuid, SLE_UUID_SERVER_SERVICE);
    }
}

// 设置连接参数 - 参考官方教程
static errcode_t sle_client_connect_param_init(void)
{
    sle_default_connect_param_t param = {0};
    param.enable_filter_policy = 0;
    param.gt_negotiate = 0;
    param.initiate_phys = 1;
    param.max_interval = SLE_CONN_INTV_MAX_DEFAULT;  // 按官方demo标准
    param.min_interval = SLE_CONN_INTV_MIN_DEFAULT;  // 按官方demo标准
    param.scan_interval = 400;     // 扫描间隔
    param.scan_window = 20;        // 扫描窗口
    param.timeout = 0x1F4;         // 超时时间
    
    errcode_t ret = sle_default_connection_param_set(&param);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_client] set connect param fail:%x\r\n", ret);
        return ret;
    }
    
    printf("[sle_client] connect param init success\r\n");
    return ERRCODE_SUCC;
}

// 注册扫描回调
static errcode_t sle_client_seek_cbk_register(void)
{
    sle_announce_seek_callbacks_t seek_cbks = {0};
    seek_cbks.seek_result_cb = sle_seek_result_cb;
    
    errcode_t ret = sle_announce_seek_register_callbacks(&seek_cbks);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_client] seek register callbacks fail:%x\r\n", ret);
        return ret;
    }
    
    printf("[sle_client] seek callbacks registered\r\n");
    return ERRCODE_SUCC;
}

// 注册连接回调
static errcode_t sle_client_connect_cbk_register(void)
{
    sle_connection_callbacks_t conn_cbks = {0};
    conn_cbks.connect_state_changed_cb = sle_connect_state_changed_cbk;
    conn_cbks.pair_complete_cb = sle_pair_complete_cbk;
    
    errcode_t ret = sle_connection_register_callbacks(&conn_cbks);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_client] connect register callbacks fail:%x\r\n", ret);
        return ret;
    }
    
    printf("[sle_client] connect callbacks registered\r\n");
    return ERRCODE_SUCC;
}

// 注册SSAPC回调
static errcode_t sle_client_ssapc_cbk_register(void)
{
    ssapc_callbacks_t ssapc_cbks = {0};
    ssapc_cbks.exchange_info_cb = sle_client_exchange_info_cbk;
    ssapc_cbks.find_structure_cb = sle_ssapc_find_structure_cbk;
    ssapc_cbks.ssapc_find_property_cbk = sle_client_find_property_cbk;
    ssapc_cbks.write_cfm_cb = sle_client_write_cfm_cbk;
    ssapc_cbks.notification_cb = sle_ssapc_data_received_cbk;
    ssapc_cbks.indication_cb = sle_ssapc_data_received_cbk;
    
    errcode_t ret = ssapc_register_callbacks(&ssapc_cbks);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_client] ssapc register callbacks fail:%x\r\n", ret);
        return ret;
    }
    
    printf("[sle_client] ssapc callbacks registered\r\n");
    return ERRCODE_SUCC;
}

// 设置本地地址
static errcode_t sle_client_set_local_addr(void)
{
    uint8_t local_addr[SLE_ADDR_LEN] = {0x13, 0x67, 0x5c, 0x07, 0x00, 0x51};
    sle_addr_t local_address = {0};
    local_address.type = 0;
    memcpy_s(local_address.addr, SLE_ADDR_LEN, local_addr, SLE_ADDR_LEN);
    
    errcode_t ret = sle_set_local_addr(&local_address);
    if (ret != ERRCODE_SUCC) {
        printf("[sle_client] set local addr fail:%x\r\n", ret);
        return ret;
    }
    
    printf("[sle_client] local addr set: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           local_addr[0], local_addr[1], local_addr[2], local_addr[3], local_addr[4], local_addr[5]);
    return ERRCODE_SUCC;
}

// 星闪客户端初始化 - 按照官方教程流程
errcode_t sle_client_init(void)
{
    printf("[sle_client] init start\r\n");
    
    // 1. 注册扫描回调
    errcode_t ret = sle_client_seek_cbk_register();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    
    // 2. 初始化连接参数
    ret = sle_client_connect_param_init();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    
    // 3. 注册连接回调
    ret = sle_client_connect_cbk_register();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    
    // 4. 注册SSAPC回调
    ret = sle_client_ssapc_cbk_register();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    
    // 5. 启用星闪协议栈
    ret = enable_sle();
    if (ret != ERRCODE_SUCC) {
        printf("[sle_client] enable sle fail:%x\r\n", ret);
        return ret;
    }
    printf("[sle_client] sle enabled\r\n");
    
    // 6. 设置本地地址
    ret = sle_client_set_local_addr();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    
    printf("[sle_client] init success\r\n");
    return ERRCODE_SUCC;
}

// 星闪客户端任务
static void sle_client_sample_task(void)
{
    printf("[sle_client] sample task started\r\n");
    
    // 延迟一下确保初始化完成
    osDelay(1000);
    
    // 启动扫描
    sle_start_scan();
    
    while (true) {
        // 保持任务存活，其他动作在回调中驱动
        osDelay(SLE_TASK_DELAY_MS);
        
        // 如果连接断开，定期重新扫描
        if (g_sle_client_conn_state != SLE_ACB_STATE_CONNECTED) {
            printf("[sle_client] not connected, check scan status\r\n");
        }
    }
}

// MTU交换完成回调后，发起服务发现
static void sle_client_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param, errcode_t status)
{
    printf("[sle_client] exchange info: mtu=%u ver=%u status=%d\r\n", param->mtu_size, param->version, status);
    
    if (status == ERRCODE_SUCC) {
        printf("[sle_client] MTU exchange successful, starting service discovery...\r\n");
        // 首先发现服务
        ssapc_find_structure_param_t find_param = {0};
        find_param.type = SSAP_FIND_TYPE_PRIMARY_SERVICE;
        find_param.start_hdl = 1;
        find_param.end_hdl = 0xFFFF;
        ssapc_find_structure(client_id, conn_id, &find_param);
    } else {
        printf("[sle_client] MTU exchange failed\r\n");
    }
}

// 发现特征回调：记录可写特征的句柄
static void sle_client_find_property_cbk(uint8_t client_id, uint16_t conn_id, ssapc_find_property_result_t *property, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    
    printf("[sle_client] ===== 特征发现回调 =====\r\n");
    printf("[sle_client] 客户端ID=%d, 连接ID=0x%04x, 状态=0x%02x\r\n", client_id, conn_id, status);
    
    if (status != ERRCODE_SUCC) {
        printf("[sle_client] ❌ 特征发现失败，状态=0x%02x\r\n", status);
        return;
    }
    
    if (property == NULL) {
        printf("[sle_client] ❌ 特征指针为空\r\n");
        return;
    }
    
    printf("[sle_client] 发现特征: 句柄=0x%04x, 操作指示=0x%02x\r\n",
           property->handle, property->operate_indication);
    
    // 检查是否是我们期望的特征UUID
    printf("[sle_client] 检查特征UUID，长度=%d\r\n", property->uuid.len);
    
    // 支持16字节完整UUID和2字节短UUID
    uint16_t property_uuid = 0;
    bool uuid_match = false;
    
    if (property->uuid.len == 2) {
        // 2字节短UUID
        property_uuid = (property->uuid.uuid[15] << 8) | property->uuid.uuid[14];
        printf("[sle_client] 短UUID: 0x%04x\r\n", property_uuid);
        uuid_match = (property_uuid == SLE_UUID_SERVER_NTF_REPORT);
    } else if (property->uuid.len == 16) {
        // 16字节完整UUID，检查最后2字节
        property_uuid = (property->uuid.uuid[15] << 8) | property->uuid.uuid[14];
        printf("[sle_client] 完整UUID最后2字节: 0x%04x\r\n", property_uuid);
        uuid_match = (property_uuid == SLE_UUID_SERVER_NTF_REPORT);
    } else {
        printf("[sle_client] UUID长度不支持: %d\r\n", property->uuid.len);
    }
    
    printf("[sle_client] 特征UUID: 0x%04x (期望: 0x%04x) 匹配=%s\r\n", 
           property_uuid, SLE_UUID_SERVER_NTF_REPORT, uuid_match ? "是" : "否");
    
    if (uuid_match) {
        printf("[sle_client] ✅ 找到目标货物特征！\r\n");
        // 检查是否支持写操作
        if (property->operate_indication & SSAP_OPERATE_INDICATION_BIT_WRITE) {
            g_sle_client_write_id = property->handle;
            printf("[sle_client] ✅ 特征支持写操作，句柄=0x%04x\r\n", g_sle_client_write_id);
            printf("[sle_client] ✅ SLE服务发现完成，准备发送数据\r\n");
            
            // 立即发送一次当前货物数据，确保63B能看到初始状态
            extern void get_current_cargo_counts(uint32_t *js, uint32_t *zj, uint32_t *sh);
            uint32_t js, zj, sh;
            get_current_cargo_counts(&js, &zj, &sh);
            
            // 延迟一下确保连接稳定，然后发送初始数据
            osDelay(100);
            sle_client_send_cargo_data(js, zj, sh);
            printf("[sle_client] 发送初始货物数据: J=%u, Z=%u, S=%u\r\n", js, zj, sh);
        } else {
            printf("[sle_client] ❌ 货物特征不支持写操作 (0x%02x)\r\n", property->operate_indication);
        }
    } else {
        printf("[sle_client] 不是目标特征，继续搜索...\r\n");
    }
}

// 写确认回调
static void sle_client_write_cfm_cbk(uint8_t client_id, uint16_t conn_id, ssapc_write_result_t *write_result, errcode_t status)
{
    unused(client_id);
    unused(conn_id);
    
    printf("[sle_client] 写操作确认回调：\r\n");
    printf("  客户端ID: %d\r\n", client_id);
    printf("  连接ID: 0x%04x\r\n", conn_id);
    printf("  状态码: 0x%02x (%s)\r\n", status, (status == ERRCODE_SUCC) ? "成功" : "失败");
    
    if (write_result != NULL) {
        printf("  句柄: 0x%04x\r\n", write_result->handle);
        printf("  类型: 0x%02x\r\n", write_result->type);
    } else {
        printf("  写结果为空\r\n");
    }
    
    if (status != ERRCODE_SUCC) {
        printf("[sle_client] ❌ 货物数据发送失败！\r\n");
    } else {
        printf("[sle_client] ✅ 货物数据发送成功！\r\n");
    }
}

// 获取连接状态
bool sle_client_is_connected(void)
{
    return (g_sle_client_conn_state == SLE_ACB_STATE_CONNECTED);
}

// 创建星闪客户端任务
errcode_t sle_client_task_init(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "SLEClientTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 2048;
    attr.priority = osPriorityNormal;

    osThreadId_t task_id = osThreadNew((osThreadFunc_t)sle_client_sample_task, NULL, &attr);
    if (task_id == NULL) {
        printf("[sle_client] Failed to create task!\r\n");
        return ERRCODE_FAIL;
    }
    
    printf("[sle_client] task created successfully\r\n");
    return ERRCODE_SUCC;
}
