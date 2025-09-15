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

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "lwip/netifapi.h"
#include "lwip/ip4_addr.h"
#include "wifi_device.h"
#include "wifi_event.h"
#include "wifi_hotspot.h"
#include "wifi_hotspot_config.h"
#include "cmsis_os2.h"
#include "soc_osal.h"
#include "chip_io.h"
#include "td_type.h"

#include "wifi_config_ws63.h"
#include "oled_ssd1306_ws63.h"
#include "wifi_sta_connect_ws63.h"

#define WIFI_SCAN_AP_LIMIT 64
#define WIFI_CONN_STATUS_MAX_GET_TIMES 5
#define DHCP_BOUND_STATUS_MAX_GET_TIMES 20
#define WIFI_STA_IP_MAX_GET_TIMES 5

static char g_local_ip[20] = {0};
static int g_staConnect = 0;
static int g_netId = -1;

const char* get_local_ip(void)
{
    return g_local_ip;
}

// WiFi事件回调函数
static void wifi_scan_state_changed(td_s32 state, td_s32 size)
{
    UNUSED(state);
    UNUSED(size);
    printf("Scan done!\r\n");
    return;
}

static void wifi_connection_changed(td_s32 state, const wifi_linked_info_stru *info, td_s32 reason_code)
{
    UNUSED(reason_code);

    if (state == WIFI_STATE_AVALIABLE) {
        printf("[WiFi]:%s, [RSSI]:%d\r\n", info->ssid, info->rssi);
        g_staConnect = 1;
    } else {
        g_staConnect = 0;
    }
}

// 获取匹配的网络
static errcode_t example_get_match_network(const char *expected_ssid,
                                           const char *key,
                                           wifi_sta_config_stru *expected_bss)
{
    uint32_t num = WIFI_SCAN_AP_LIMIT;
    uint32_t bss_index = 0;

    // 获取扫描结果
    uint32_t scan_len = sizeof(wifi_scan_info_stru) * WIFI_SCAN_AP_LIMIT;
    wifi_scan_info_stru *result = osal_kmalloc(scan_len, OSAL_GFP_ATOMIC);
    if (result == NULL) {
        return ERRCODE_MALLOC;
    }

    memset_s(result, scan_len, 0, scan_len);
    if (wifi_sta_get_scan_info(result, &num) != ERRCODE_SUCC) {
        osal_kfree(result);
        return ERRCODE_FAIL;
    }

    // 筛选扫描到的Wi-Fi网络，选择待连接的网络
    for (bss_index = 0; bss_index < num; bss_index++) {
        if (strlen(expected_ssid) == strlen(result[bss_index].ssid)) {
            if (memcmp(expected_ssid, result[bss_index].ssid, strlen(expected_ssid)) == 0) {
                break;
            }
        }
    }

    // 未找到待连接AP
    if (bss_index >= num) {
        osal_kfree(result);
        return ERRCODE_FAIL;
    }

    // 找到网络后复制网络信息和接入密码
    if (memcpy_s(expected_bss->ssid, WIFI_MAX_SSID_LEN, result[bss_index].ssid, WIFI_MAX_SSID_LEN) != EOK) {
        osal_kfree(result);
        return ERRCODE_MEMCPY;
    }
    if (memcpy_s(expected_bss->bssid, WIFI_MAC_LEN, result[bss_index].bssid, WIFI_MAC_LEN) != EOK) {
        osal_kfree(result);
        return ERRCODE_MEMCPY;
    }
    expected_bss->security_type = result[bss_index].security_type;
    if (memcpy_s(expected_bss->pre_shared_key, WIFI_MAX_KEY_LEN, key, strlen(key)) != EOK) {
        osal_kfree(result);
        return ERRCODE_MEMCPY;
    }
    expected_bss->ip_type = DHCP; // IP类型为动态DHCP获取
    osal_kfree(result);
    return ERRCODE_SUCC;
}

static errcode_t wifi_connect(void)
{
    char ifname[WIFI_IFNAME_MAX_SIZE + 1] = "wlan0"; // WiFi STA 网络设备名
    wifi_sta_config_stru expected_bss = {0};         // 连接请求信息
    const char expected_ssid[] = AP_SSID;
    const char key[] = AP_PWD; // 待连接的网络接入密码
    struct netif *netif_p = NULL;
    wifi_linked_info_stru wifi_status;
    uint8_t index = 0;

    // 创建STA
    if (wifi_sta_enable() != ERRCODE_SUCC) {
        printf("STA enable fail !\r\n");
        return ERRCODE_FAIL;
    }

    do {
        printf("Start Scan !\r\n");
        osDelay(100); // 延时1s

        // 启动STA扫描
        if (wifi_sta_scan() != ERRCODE_SUCC) {
            printf("STA scan fail, try again !\r\n");
            continue;
        }

        osDelay(300); // 延时3s

        // 获取待连接的网络
        if (example_get_match_network(expected_ssid, key, &expected_bss) != ERRCODE_SUCC) {
            printf("Can not find AP, try again !\r\n");
            continue;
        }

        printf("STA start connect.\r\n");
        // 启动连接
        if (wifi_sta_connect(&expected_bss) != ERRCODE_SUCC) {
            continue;
        }

        // 检查网络是否连接成功
        for (index = 0; index < WIFI_CONN_STATUS_MAX_GET_TIMES; index++) {
            osDelay(50); // 延时500ms
            memset_s(&wifi_status, sizeof(wifi_linked_info_stru), 0, sizeof(wifi_linked_info_stru));
            if (wifi_sta_get_ap_info(&wifi_status) != ERRCODE_SUCC) {
                continue;
            }
            if (wifi_status.conn_state == WIFI_CONNECTED) {
                break;
            }
        }
        if (wifi_status.conn_state == WIFI_CONNECTED) {
            break; // 连接成功退出循环
        }
    } while (1);

    // DHCP获取IP地址
    netif_p = netifapi_netif_find(ifname);
    if (netif_p == NULL) {
        return ERRCODE_FAIL;
    }

    if (netifapi_dhcp_start(netif_p) != ERR_OK) {
        printf("STA DHCP Fail.\r\n");
        return ERRCODE_FAIL;
    }

    for (uint8_t i = 0; i < DHCP_BOUND_STATUS_MAX_GET_TIMES; i++) {
        osDelay(50); // 延时500ms
        if (netifapi_dhcp_is_bound(netif_p) == ERR_OK) {
            printf("STA DHCP bound success.\r\n");
            break;
        }
    }

    for (uint8_t i = 0; i < WIFI_STA_IP_MAX_GET_TIMES; i++) {
        osDelay(1); // 延时10ms
        if (netif_p->ip_addr.u_addr.ip4.addr != 0) {
            printf("STA IP %u.%u.%u.%u\r\n", 
                   (netif_p->ip_addr.u_addr.ip4.addr & 0x000000ff),
                   (netif_p->ip_addr.u_addr.ip4.addr & 0x0000ff00) >> 8,
                   (netif_p->ip_addr.u_addr.ip4.addr & 0x00ff0000) >> 16,
                   (netif_p->ip_addr.u_addr.ip4.addr & 0xff000000) >> 24);

            // 保存IP地址字符串
            snprintf(g_local_ip, sizeof(g_local_ip), "%u.%u.%u.%u",
                     (netif_p->ip_addr.u_addr.ip4.addr & 0x000000ff),
                     (netif_p->ip_addr.u_addr.ip4.addr & 0x0000ff00) >> 8,
                     (netif_p->ip_addr.u_addr.ip4.addr & 0x00ff0000) >> 16,
                     (netif_p->ip_addr.u_addr.ip4.addr & 0xff000000) >> 24);

            // 在OLED上显示IP地址
            OledShowString2(0, 0, g_local_ip, FONT6_X8);
            OledShowString2(90, 0, ":5566", FONT6_X8);

            // 连接成功
            printf("STA connect success.\r\n");
            return ERRCODE_SUCC;
        }
    }
    printf("STA connect fail.\r\n");
    return ERRCODE_FAIL;
}

/**
 * @brief This function will start wifi station module, and WiFi will connect to the hotspot
 *        The function gets DHCP, and so on...
 */
void WifiStaModule(void)
{
    wifi_event_stru wifi_event_cb = {0};

    wifi_event_cb.wifi_event_scan_state_changed = wifi_scan_state_changed;
    wifi_event_cb.wifi_event_connection_changed = wifi_connection_changed;

    // 注册事件回调
    if (wifi_register_event_cb(&wifi_event_cb) != 0) {
        printf("wifi_event_cb register fail.\r\n");
        return;
    }
    printf("wifi_event_cb register succ.\r\n");

    // 等待wifi初始化完成
    while (wifi_is_wifi_inited() == 0) {
        osDelay(10); // 延时100ms
    }

    g_netId = wifi_connect();
    printf("wifi sta dhcp done\r\n");
    return;
}
