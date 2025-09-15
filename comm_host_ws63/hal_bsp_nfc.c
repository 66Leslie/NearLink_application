/*
 * Copyright (c) 2023 Beijing HuaQing YuanJian Education Technology Co., Ltd
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pinctrl.h"
#include "gpio.h"
#include "i2c.h"
#include "cmsis_os2.h"
#include "securec.h"
#include "hal_bsp_nfc.h"
#include "common_def.h"

#ifndef CONFIG_I2C_SUPPORT_MASTER
/* Forward declaration to satisfy this compilation unit when I2C master API macros are not enabled */
errcode_t uapi_i2c_master_init(i2c_bus_t bus, uint32_t baudrate, uint8_t hscode);
#endif

// NFC页缓冲区
uint8_t nfcPageBuffer[NFC_PAGE_SIZE] = {0};

// 模拟的货物分拣信息，默认值为12345
static cargo_sort_info_t g_cargo_info = {
    .jiangsu_count = 1,   // 江苏 (0)
    .zhejiang_count = 2,  // 浙江 (1)
    .shanghai_count = 345 // 上海 (2)
};

// NFC任务句柄
static osThreadId_t g_nfc_task_id = NULL;

/**
 * @brief  模拟NT3H读取头部信息
 * @param  ndefLen: NDEF数据长度
 * @param  ndef_Header: NDEF头部信息
 * @retval true/false
 */
bool NT3HReadHeaderNfc(uint8_t *ndefLen, uint8_t *ndef_Header)
{
    if (ndefLen == NULL || ndef_Header == NULL) {
        return false;
    }
    
    // 模拟返回数据长度，包含货物分拣信息
    *ndefLen = 32;  // 假设数据长度为32字节
    *ndef_Header = 0x03;  // NDEF消息头
    
    return true;
}

/**
 * @brief  模拟NT3H读取用户数据
 * @param  page: 页号
 * @retval true/false
 */
bool NT3HReadUserData(uint8_t page)
{
    // 清空缓冲区
    memset(nfcPageBuffer, 0, NFC_PAGE_SIZE);

    // 生成货物信息文本
    char cargo_text[32];
    snprintf(cargo_text, sizeof(cargo_text), "JS:%d ZJ:%d SH:%d",
            g_cargo_info.jiangsu_count,
            g_cargo_info.zhejiang_count,
            g_cargo_info.shanghai_count);

    if (page == 0) {
        // 第一页：NDEF头部信息
        nfcPageBuffer[0] = 0x03;  // NDEF消息开始
        nfcPageBuffer[1] = 0x26;  // 数据长度 (38字节)
        nfcPageBuffer[2] = 0xD1;  // TNF + MB + ME + SR (Text Record)
        nfcPageBuffer[3] = 0x01;  // Type Length (1字节)
        nfcPageBuffer[4] = 0x22;  // Payload Length (34字节)
        nfcPageBuffer[5] = 0x54;  // Type: 'T' (Text)
        nfcPageBuffer[6] = 0x02;  // Text encoding (UTF-8) + language length (2)
        nfcPageBuffer[7] = 0x65;  // Language: 'e'
        nfcPageBuffer[8] = 0x6E;  // Language: 'n'

        // 从第9个字节开始写入货物信息
        int text_len = strlen(cargo_text);
        if (text_len > 7) text_len = 7; // 第一页最多7个字符
        memcpy(&nfcPageBuffer[9], cargo_text, text_len);
    } else if (page == 1) {
        // 第二页：继续货物信息或系统信息
        if (strlen(cargo_text) > 7) {
            // 如果货物信息超过7个字符，继续写入剩余部分
            strncpy((char*)nfcPageBuffer, &cargo_text[7], 16);
        } else {
            // 否则写入系统信息
            strncpy((char*)nfcPageBuffer, " System:WS63", 16);
        }
    }
    
    return true;
}

/**
 * @brief  从Page页中组成NDEF协议的包裹
 * @note
 * @param  *dataBuff: 最终的内容
 * @param  dataBuff_MaxSize: 存储缓冲区的长度
 * @retval
 */
uint32_t get_NDEFDataPackage(uint8_t *dataBuff, const uint16_t dataBuff_MaxSize)
{
    if (dataBuff == NULL || dataBuff_MaxSize <= 0) {
        printf("dataBuff==NULL or dataBuff_MaxSize<=0\r\n");
        return ERRCODE_FAIL;
    }

    uint8_t userMemoryPageNum = 0; // 用户的数据操作页数

    // 算出要取多少页
    if (dataBuff_MaxSize <= NFC_PAGE_SIZE) {
        userMemoryPageNum = 1; // 1页
    } else {
        // 需要访问多少页
        userMemoryPageNum = (dataBuff_MaxSize / NFC_PAGE_SIZE) + 
                            ((dataBuff_MaxSize % NFC_PAGE_SIZE) > 0 ? 1 : 0);
    }

    // 内存拷贝
    uint8_t *p_buff = (uint8_t *)malloc(userMemoryPageNum * NFC_PAGE_SIZE);
    if (p_buff == NULL) {
        printf("p_buff == NULL.\r\n");
        return ERRCODE_FAIL;
    }

    // 读取数据
    for (int i = 0; i < userMemoryPageNum; i++) {
        if (NT3HReadUserData(i) == true) {
            memcpy_s(p_buff + i * NFC_PAGE_SIZE, userMemoryPageNum * NFC_PAGE_SIZE,
                     nfcPageBuffer, NFC_PAGE_SIZE);
        }
    }

    memcpy_s(dataBuff, dataBuff_MaxSize, p_buff, dataBuff_MaxSize);

    free(p_buff);
    p_buff = NULL;

    return ERRCODE_SUCC;
}

/**
 * @brief  NFC引脚初始化
 * @note
 * @retval
 */
uint32_t nfc_Init(void)
{
    uint32_t result;
    uint32_t baudrate = NFC_I2C_SPEED;
    uint32_t hscode = I2C_MASTER_ADDR;
    
    uapi_pin_set_mode(I2C_SCL_MASTER_PIN, CONFIG_PIN_MODE);
    uapi_pin_set_mode(I2C_SDA_MASTER_PIN, CONFIG_PIN_MODE);       
    uapi_pin_set_pull(I2C_SCL_MASTER_PIN, PIN_PULL_TYPE_UP);
    uapi_pin_set_pull(I2C_SDA_MASTER_PIN, PIN_PULL_TYPE_UP);
   
    result = uapi_i2c_master_init(NFC_I2C_IDX, baudrate, hscode);
    if (result != ERRCODE_SUCC) {
        printf("I2C Init status is 0x%x!!!\r\n", result);
        return result;
    }
    printf("I2C nfc Init is succeeded!!!\r\n");

    return ERRCODE_SUCC;
}

/**
 * @brief  向NFC写入货物分拣信息
 * @param  cargo_info: 货物分拣信息
 * @retval 错误码
 */
errcode_t nfc_write_cargo_info(const cargo_sort_info_t *cargo_info)
{
    if (cargo_info == NULL) {
        return ERRCODE_FAIL;
    }
    
    // 更新全局货物信息
    g_cargo_info = *cargo_info;
    
    printf("NFC: Updated cargo info - JS:%d, ZJ:%d, SH:%d\r\n", 
           g_cargo_info.jiangsu_count, 
           g_cargo_info.zhejiang_count, 
           g_cargo_info.shanghai_count);
    
    return ERRCODE_SUCC;
}

/**
 * @brief  从NFC读取货物分拣信息
 * @param  cargo_info: 货物分拣信息
 * @retval 错误码
 */
errcode_t nfc_read_cargo_info(cargo_sort_info_t *cargo_info)
{
    if (cargo_info == NULL) {
        return ERRCODE_FAIL;
    }
    
    *cargo_info = g_cargo_info;
    return ERRCODE_SUCC;
}

/**
 * @brief  NFC任务函数
 * @param  arg: 参数
 */
void nfc_task(void *arg)
{
    unused(arg);

    uint8_t ndefLen = 0;
    uint8_t ndef_Header = 0;
    static uint32_t last_touch_time = 0;
    static uint32_t touch_count = 0;

    printf("NFC Task started - Ready for phone touch\r\n");

    while (1) {
        uint32_t current_time = osKernelGetTickCount();

        // 模拟NFC碰触检测 (每3秒模拟一次手机碰触)
        if ((current_time - last_touch_time) > 3000) {
            touch_count++;
            last_touch_time = current_time;

            printf("\r\n=== NFC Touch Detected #%d ===\r\n", touch_count);

            // 模拟NFC读取操作
            if (NT3HReadHeaderNfc(&ndefLen, &ndef_Header)) {
                ndefLen += NDEF_HEADER_SIZE;

                if (ndefLen > NDEF_HEADER_SIZE) {
                    uint8_t *ndefBuff = (uint8_t *)malloc(ndefLen + 1);
                    if (ndefBuff != NULL) {
                        if (get_NDEFDataPackage(ndefBuff, ndefLen) == ERRCODE_SUCC) {
                            printf("NFC: Sending cargo info to phone...\r\n");
                            printf("Cargo Data: JS=%d, ZJ=%d, SH=%d\r\n",
                                   g_cargo_info.jiangsu_count,
                                   g_cargo_info.zhejiang_count,
                                   g_cargo_info.shanghai_count);

                            // 显示发送的原始数据
                            printf("Raw NFC Data: ");
                            for (int i = 0; i < ndefLen && i < 48; i++) {
                                if (ndefBuff[i] >= 32 && ndefBuff[i] <= 126) {
                                    printf("%c", ndefBuff[i]);
                                } else {
                                    printf(".");
                                }
                            }
                            printf("\r\n");

                            printf("NFC: Data sent successfully!\r\n");
                        }
                        free(ndefBuff);
                    }
                }
            }
            printf("=== NFC Touch Complete ===\r\n\r\n");
        }

        osDelay(1000); // 1秒检测一次
    }
}

/**
 * @brief  初始化NFC功能
 * @retval 错误码
 */
errcode_t nfc_module_init(void)
{
    // 初始化NFC硬件
    if (nfc_Init() != ERRCODE_SUCC) {
        printf("NFC hardware init failed\r\n");
        return ERRCODE_FAIL;
    }
    
    // 创建NFC任务
    osThreadAttr_t attr = {0};
    attr.name = "NFCTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 2048;
    attr.priority = osPriorityNormal;

    g_nfc_task_id = osThreadNew((osThreadFunc_t)nfc_task, NULL, &attr);
    if (g_nfc_task_id == NULL) {
        printf("Failed to create NFC task!\r\n");
        return ERRCODE_FAIL;
    }
    
    printf("NFC module initialized successfully\r\n");
    return ERRCODE_SUCC;
}
