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

#include <stdio.h>
#include <string.h>

#include "soc_osal.h"
#include "cmsis_os2.h"
#include "app_init.h"
#include "chip_io.h"

#include "pinctrl.h"
#include "gpio.h"
#include "uart.h"
#include "i2c.h"

#include "oled_ssd1306_ws63.h"
#include "udp_server_ws63.h"
#include "wifi_sta_connect_ws63.h"

#include "sle_client.h"

#define STACK_SIZE (4096)
#define UART_TASK_STACK_SIZE (4096)

/****************************
         Production Line Display
****************************/
uint8_t index_line = 0;  // 当前流水线编号 (0-9)

/****************************
         SLE
****************************/
static bool sle_enabled = false;

// 全局货物数据结构
typedef struct {
    uint32_t jiangsu_count;
    uint32_t zhejiang_count; 
    uint32_t shanghai_count;
} global_cargo_data_t;

static global_cargo_data_t g_global_cargo = {0};

// 函数前向声明
static void update_global_cargo_data(int sort_type);



// 提供给星闪模块调用，用于获取当前货物分拣数量（使用全局数据）
void get_current_cargo_counts(uint32_t *js, uint32_t *zj, uint32_t *sh)
{
    if (js == NULL || zj == NULL || sh == NULL) {
        return;
    }

    *js = g_global_cargo.jiangsu_count;
    *zj = g_global_cargo.zhejiang_count;
    *sh = g_global_cargo.shanghai_count;
    
    printf("获取当前货物数量: J=%u, Z=%u, S=%u\r\n", *js, *zj, *sh);
}

/****************************
         UART
****************************/
unsigned char uartWriteBuff[5] = {0xFF, '0', '0', '0', '0'};
char expressBoxNum[10] = {0};

// 串口数据处理任务
static void UartTask(void *arg)
{
    unused(arg);  // 标记未使用的参数

    uint8_t uart_buff[256];
    int32_t len;

    while (1) {
        // 使用WS63的UART API接收数据
        len = uapi_uart_read(UART_BUS_2, uart_buff, sizeof(uart_buff), 0);
        if (len > 0) {
            uart_buff[len] = '\0';
            printf("UART received: %s\r\n", uart_buff);

            // 处理接收到的数据
            if (len >= 5) {
                memcpy(expressBoxNum, uart_buff, len < 10 ? len : 9);
                expressBoxNum[len < 10 ? len : 9] = '\0';

                // 解析流水线编号设置指令 (格式: "LINE:3" 设置流水线编号为3)
                if (strncmp((char*)uart_buff, "LINE:", 5) == 0 && len >= 6) {
                    int line_num = uart_buff[5] - '0';
                    if (line_num >= 0 && line_num <= 9) {
                        index_line = line_num;
                        printf("Set production line number to: %d\r\n", index_line);
                        // 更新OLED显示
                        OledShowChar(60, 5, index_line + '0', FONT6_X8);
                    }
                }
                // 解析分拣信息 (格式: "sort_info:id=XX,dir=Y")
                else if (strncmp((char*)uart_buff, "sort_info:id=", 13) == 0 && len >= 20 && len < 100) {
                    // 创建本地字符串副本以便安全解析
                    char parse_buf[128] = {0};
                    size_t copy_len = (len < 127) ? len : 127;
                    memcpy_s(parse_buf, sizeof(parse_buf), uart_buff, copy_len);
                    parse_buf[copy_len] = '\0';
                    
                    printf("解析分拣信息: %s (长度=%d)\r\n", parse_buf, len);
                    
                    // 查找ID部分
                    char *id_start = strstr(parse_buf, "id=");
                    char *dir_start = strstr(parse_buf, "dir=");
                    
                    int id = 0;
                    char direction = 'N';
                    
                    if (id_start != NULL && (id_start + 5) < (parse_buf + copy_len)) {
                        id_start += 3; // 跳过"id="
                        char id_str[3] = {0};
                        if (id_start[0] && id_start[1]) {
                            id_str[0] = id_start[0];
                            id_str[1] = id_start[1];
                            id = (int)strtol(id_str, NULL, 16);
                        }
                    }
                    
                    if (dir_start != NULL && (dir_start + 4) < (parse_buf + copy_len)) {
                        dir_start += 4; // 跳过"dir="
                        if (dir_start[0]) {
                            direction = dir_start[0];
                        }
                    }
                    
                    printf("Received sorting info: ID=%02X(%d), Direction=%c\r\n", id, id, direction);
                    
                    // 构建消息发送给小程序 (保持原始格式)
                    char sort_msg[64] = {0};
                    int msg_len = snprintf(sort_msg, sizeof(sort_msg) - 1, "sort_info:id=%02X,dir=%c", id, direction);
                    if (msg_len > 0 && msg_len < (int)sizeof(sort_msg)) {
                        UdpSend(sort_msg, strlen(sort_msg));
                        printf("Forwarded sorting info to miniprogram: %s\r\n", sort_msg);
                    } else {
                        printf("构建UDP消息失败\r\n");
                    }
                    
                    // 根据分拣信息更新货物数据
                    // 映射：根据方向确定地区
                    // A->L 江苏, B->M 浙江, C->R 上海
                    int sort_type = -1;
                    if (direction == 'L' || direction == 'l' || direction == 'A' || direction == 'a') {
                        sort_type = 0; // 江苏
                    } else if (direction == 'M' || direction == 'm' || direction == 'B' || direction == 'b') {
                        sort_type = 1; // 浙江  
                    } else if (direction == 'R' || direction == 'r' || direction == 'C' || direction == 'c') {
                        sort_type = 2; // 上海
                    }
                    
                    if (sort_type >= 0 && sort_type <= 2) {
                        printf("根据方向%c映射到分拣类型: %d\r\n", direction, sort_type);
                        update_global_cargo_data(sort_type);
                        
                        // 发送确认响应给ctl_host
                        char response[32] = {0};
                        int resp_len = snprintf(response, sizeof(response) - 1, "SORT_OK:%d", sort_type);
                        if (resp_len > 0 && resp_len < (int)sizeof(response)) {
                            uapi_uart_write(UART_BUS_2, (uint8_t*)response, strlen(response), 0);
                            printf("已发送分拣确认: %s\r\n", response);
                        }
                    } else {
                        printf("未知分拣方向: %c，不更新货物数据\r\n", direction);
                    }
                }
                
                // 解析分拣指令 (格式: "SORT:0" 江苏+1, "SORT:1" 浙江+1, "SORT:2" 上海+1)
                else if (strncmp((char*)uart_buff, "SORT:", 5) == 0 && len >= 6) {
                    int sort_type = uart_buff[5] - '0';
                    if (sort_type >= 0 && sort_type <= 2) {
                        printf("收到分拣指令: SORT:%d\r\n", sort_type);
                        update_global_cargo_data(sort_type);
                        
                        // 发送确认响应给ctl_host
                        char response[32];
                        snprintf(response, sizeof(response), "SORT_OK:%d", sort_type);
                        uapi_uart_write(UART_BUS_2, (uint8_t*)response, strlen(response), 0);
                        printf("已发送分拣确认: %s\r\n", response);
                    } else {
                        printf("无效的分拣类型: %d\r\n", sort_type);
                    }
                }

                // 通过UDP发送数据给小程序
                UdpSend((const char*)uart_buff, len);

                // 注意：星闪不在这里发送，星闪专门用于发送货物数据给63B
                // 串口数据是与ctl_host的控制通信，不需要通过星闪发送
            }
        }
        osDelay(10);
    }
}

// UART接收缓冲区
static uint8_t uart_rx_buffer[512];

// 重复定义已删除，使用前面定义的 global_cargo_data_t

// 统一的数据更新函数
static void update_global_cargo_data(int sort_type) {
    switch(sort_type) {
        case 0: 
            g_global_cargo.jiangsu_count++; 
            printf("江苏货物+1, 当前总数: %u\r\n", g_global_cargo.jiangsu_count);
            break;
        case 1: 
            g_global_cargo.zhejiang_count++; 
            printf("浙江货物+1, 当前总数: %u\r\n", g_global_cargo.zhejiang_count);
            break;
        case 2: 
            g_global_cargo.shanghai_count++; 
            printf("上海货物+1, 当前总数: %u\r\n", g_global_cargo.shanghai_count);
            break;
        default:
            printf("无效的分拣类型: %d\r\n", sort_type);
            return;
    }
    
    printf("货物数据更新: J=%u, Z=%u, S=%u\r\n", 
           g_global_cargo.jiangsu_count, 
           g_global_cargo.zhejiang_count, 
           g_global_cargo.shanghai_count);
    

}

// 星闪货物数据发送任务
static void SleCargoTask(void *arg)
{
    unused(arg);
    
    printf("SLE Cargo Task started\r\n");
    static uint64_t last_sent_time = 0;
    
    while (1) {
        // 每1秒发送一次货物数据给63B，或者数据有更新时立即发送
        bool sle_conn_status = sle_client_is_connected();
        printf("[SleCargoTask] 检查发送条件: sle_enabled=%s, connected=%s\r\n", 
               sle_enabled ? "是" : "否", sle_conn_status ? "是" : "否");
        
        if (sle_enabled && sle_conn_status) {
            uint64_t current_time = osKernelGetTickCount();
            
            // 检查是否需要发送数据（定时发送或数据有更新）
            bool should_send = false;
            if (current_time - last_sent_time >= 1000) {  // 1秒定时发送
                should_send = true;
                printf("[SleCargoTask] 定时发送条件满足 (间隔=%llu ms)\r\n", current_time - last_sent_time);
            }
            // 如果是首次连接（last_sent_time为0），立即发送
            if (last_sent_time == 0) {
                should_send = true;
                printf("[SleCargoTask] 首次连接，立即发送货物数据\r\n");
            }
            
            printf("[SleCargoTask] 当前货物数据: J=%u, Z=%u, S=%u\r\n",
                   g_global_cargo.jiangsu_count, g_global_cargo.zhejiang_count, g_global_cargo.shanghai_count);
            
            if (should_send) {
                printf("[SleCargoTask] 开始发送货物数据...\r\n");
                // 使用全局真实数据而非模拟数据
                sle_client_send_cargo_data(
                    g_global_cargo.jiangsu_count,
                    g_global_cargo.zhejiang_count, 
                    g_global_cargo.shanghai_count
                );
                
                last_sent_time = current_time;
                printf("[SleCargoTask] ✅ 通过星闪发送真实货物数据: J=%u, Z=%u, S=%u\r\n", 
                       g_global_cargo.jiangsu_count, 
                       g_global_cargo.zhejiang_count, 
                       g_global_cargo.shanghai_count);
            } else {
                printf("[SleCargoTask] 暂不需要发送 (距上次发送%llu ms)\r\n", current_time - last_sent_time);
            }
        } else {
            if (sle_enabled) {
                printf("[SleCargoTask] SLE未连接，等待连接...\r\n");
            } else {
                printf("[SleCargoTask] SLE未启用，跳过数据发送\r\n");
            }
        }
        
        osDelay(1000); // 1秒检查一次
    }
}

// UART配置函数
static void usr_uart_config(void)
{
    // UART配置参数
    uart_attr_t attr = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE
    };

    // UART引脚配置 - 按照华清远见官方配置
    uart_pin_config_t pin_config = {
        .tx_pin = S_MGPIO7,  // 华清远见官方：UART2 TX使用GPIO7
        .rx_pin = S_MGPIO8,  // 华清远见官方：UART2 RX使用GPIO8
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE
    };

    // UART缓冲区配置
    uart_buffer_config_t uart_buffer_config = {
        .rx_buffer_size = 512,
        .rx_buffer = uart_rx_buffer
    };

    // 先去初始化UART
    uapi_uart_deinit(UART_BUS_2);

    // 初始化UART
    if (uapi_uart_init(UART_BUS_2, &pin_config, &attr, NULL, &uart_buffer_config) != ERRCODE_SUCC) {
        printf("UART init failed!\r\n");
    }
}



/****************************
           Main
****************************/
static void MainEntry(void *arg)
{
    unused(arg);  // 标记未使用的参数

    printf("=== COMM_HOST_WS63 MainEntry START ===\r\n");
    printf("System start...\r\n");
    
    // GPIO初始化
    uapi_gpio_init();

    printf("OLED init...\r\n");
    OledInit();
    printf("OLED clear screen...\r\n");
    OledFillScreen(0);

    printf("UART init...\r\n");
    usr_uart_config();

    // 初始化星闪功能
    printf("SLE init...\r\n");
    if (sle_client_init() == ERRCODE_SUCC) {
        sle_enabled = true;
        printf("SLE client initialized\r\n");
        
        // 创建星闪客户端任务
        if (sle_client_task_init() == ERRCODE_SUCC) {
            printf("SLE client task created\r\n");
        } else {
            printf("SLE client task creation failed\r\n");
        }
    } else {
        printf("SLE client init failed\r\n");
    }

    printf("OLED show...\r\n");
    OledShowString(5, 2, "Production Line", FONT6_X8);
    OledShowString(5, 3, "Current Line: ", FONT6_X8);
    OledShowChar(60, 5, index_line + '0', FONT6_X8);
    OledShowString(5, 7, "SLE Ready", FONT6_X8);
    printf("OLED display content updated\r\n");

    printf("Task Set start...\r\n");
    osThreadAttr_t attr, attr2;
    
    // UART任务
    attr.name = "UartTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = UART_TASK_STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)UartTask, NULL, &attr) == NULL) {
        printf("[UartTask] Failed to create UartTask!\n");
    }

    // WiFi连接
    WifiStaModule();

    // 网络任务
    attr2 = attr;
    attr2.name = "NetTask";
    attr2.stack_size = 0x1000;
    attr2.priority = osPriorityNormal3;

    if (osThreadNew((osThreadFunc_t)UdpServerDemo, NULL, &attr2) == NULL) {
        printf("[NetTask] Failed to create NetTask!\n");
    }

    // 星闪货物数据发送任务
    osThreadAttr_t attr3 = attr;
    attr3.name = "SleCargoTask";
    attr3.stack_size = 4096;
    attr3.priority = osPriorityNormal2;

    if (osThreadNew((osThreadFunc_t)SleCargoTask, NULL, &attr3) == NULL) {
        printf("[SleCargoTask] Failed to create SleCargoTask!\n");
    } else {
        printf("[SleCargoTask] SleCargoTask created successfully\n");
    }
}

static void comm_host_ws63_sample(void)
{
    osThreadAttr_t attr;
    attr.name = "comm_host_ws63_task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;
    
    if (osThreadNew((osThreadFunc_t)MainEntry, NULL, &attr) == NULL) {
        printf("Create comm_host_ws63_task fail.\r\n");
    }
    printf("Create comm_host_ws63_task succ.\r\n");
}

/* Run the sample. */
app_run(comm_host_ws63_sample);
