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

#include "pinctrl.h"
#include "gpio.h"

#include "oled_ssd1306_63B.h"
#include "sle_server_63B.h"

#define STACK_SIZE (4096)
#define DISPLAY_TASK_STACK_SIZE (2048)

/****************************
         显示任务
****************************/
static void DisplayTask(void *arg)
{
    unused(arg);
    
    printf("=== DisplayTask START ===\r\n");
    
    while (1) {
        cargo_info_t cargo_info = {0};
        
        // 清空屏幕
        OledFillScreen(0);
        
        // 显示标题
        OledShowString(0, 0, "CARGO SORT", FONT6_X8);
        
        // 获取货物信息并显示
        bool connected = sle_server_is_connected();
        if (connected && sle_server_get_cargo_info(&cargo_info)) {
            // 显示连接状态
            OledShowString(0, 1, "SLE: OK", FONT6_X8);
            
            // 显示货物分拣信息 - 使用简化字符串
            char line[16];  // 减小缓冲区
            memset(line, 0, sizeof(line));  // 确保清零
            snprintf(line, sizeof(line), "JS:%u", cargo_info.jiangsu);
            OledShowString(0, 2, line, FONT6_X8);
            
            memset(line, 0, sizeof(line));
            snprintf(line, sizeof(line), "ZJ:%u", cargo_info.zhejiang);
            OledShowString(0, 3, line, FONT6_X8);
            
            memset(line, 0, sizeof(line));
            snprintf(line, sizeof(line), "SH:%u", cargo_info.shanghai);
            OledShowString(0, 4, line, FONT6_X8);
            
            printf("Display cargo: JS=%u, ZJ=%u, SH=%u\r\n", 
                   cargo_info.jiangsu, cargo_info.zhejiang, cargo_info.shanghai);
        } else {
            // 显示连接状态和等待信息
            if (connected) {
                OledShowString(0, 1, "SLE: OK", FONT6_X8);
                OledShowString(0, 2, "Wait data", FONT6_X8);
                OledShowString(0, 3, "        ", FONT6_X8);  // 清空行
                OledShowString(0, 4, "        ", FONT6_X8);  // 清空行
            } else {
                OledShowString(0, 1, "SLE: Wait", FONT6_X8);
                OledShowString(0, 2, "Connect  ", FONT6_X8);
                OledShowString(0, 3, "        ", FONT6_X8);  // 清空行
                OledShowString(0, 4, "        ", FONT6_X8);  // 清空行
            }
        }
        
        osDelay(500); // 0.5秒更新一次
    }
}

/****************************
         主任务
****************************/
static void MainEntry(void *arg)
{
    unused(arg);
    
    printf("=== COMM_HOST_63B MainEntry START ===\r\n");
    
    // GPIO初始化
    uapi_gpio_init();
    printf("GPIO initialized\r\n");
    
    // OLED初始化
    printf("Initializing OLED...\r\n");
    OledInit();
    printf("OLED initialization completed\r\n");
    
    // 星闪服务器初始化
    printf("Initializing SLE Server...\r\n");
    errcode_t ret = sle_server_63B_init();
    if (ret == ERRCODE_SUCC) {
        printf("SLE Server initialization completed\r\n");
    } else {
        printf("SLE Server initialization failed: %x\r\n", ret);
    }
    
    // 创建显示任务
    printf("Creating display task...\r\n");
    osThreadAttr_t display_attr = {
        .name = "DisplayTask",
        .attr_bits = 0U,
        .cb_mem = NULL,
        .cb_size = 0U,
        .stack_mem = NULL,
        .stack_size = DISPLAY_TASK_STACK_SIZE,
        .priority = osPriorityNormal,
    };
    
    if (osThreadNew((osThreadFunc_t)DisplayTask, NULL, &display_attr) == NULL) {
        printf("Failed to create DisplayTask!\r\n");
    } else {
        printf("DisplayTask created successfully\r\n");
    }
    
    // 主任务保持运行
    while (1) {
        osDelay(5000); // 每5秒输出一次状态
        printf("Main task running, SLE connected: %s\r\n", 
               sle_server_is_connected() ? "true" : "false");
    }
}

/****************************
         应用程序入口
****************************/
static void comm_host_63B_sample(void)
{
    printf("=== COMM_HOST_63B SAMPLE STARTING ===\r\n");
    
    osThreadAttr_t attr;
    attr.name = "comm_host_63B_task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;
    
    if (osThreadNew((osThreadFunc_t)MainEntry, NULL, &attr) == NULL) {
        printf("Create comm_host_63B_task fail.\r\n");
    } else {
        printf("Create comm_host_63B_task succ.\r\n");
    }
}

/* Run the sample. */
app_run(comm_host_63B_sample);