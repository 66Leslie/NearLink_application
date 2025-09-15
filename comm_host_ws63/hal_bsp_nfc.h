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

#ifndef HAL_BSP_NFC_H
#define HAL_BSP_NFC_H

#include "cmsis_os2.h"
#include <stdint.h>
#include <stdbool.h>
#include "errcode.h"

#define NFC_I2C_ADDR 0x55  // 器件的I2C从机地址
#define NFC_I2C_IDX I2C_BUS_1  // 使用I2C_BUS_1而不是数字1
#define NFC_I2C_SPEED 100000 // 100KHz
#define I2C_MASTER_ADDR          0x0    
/* io*/
#define I2C_SCL_MASTER_PIN 16
#define I2C_SDA_MASTER_PIN 15
#define CONFIG_PIN_MODE 2

#define NDEF_HEADER_SIZE 0x2 // NDEF协议的头部大小
#define NFC_PAGE_SIZE 16     // NFC页大小

#define NDEF_PROTOCOL_HEADER_OFFSET 0           // NDEF协议头(固定)
#define NDEF_PROTOCOL_LENGTH_OFFSET 1           // NDEF协议数据的总长度位
#define NDEF_PROTOCOL_MEG_CONFIG_OFFSET 2       // 标签的控制字节位
#define NDEF_PROTOCOL_DATA_TYPE_LENGTH_OFFSET 3 // 标签数据类型的长度位
#define NDEF_PROTOCOL_DATA_LENGTH_OFFSET 4      // 标签的数据长度位
#define NDEF_PROTOCOL_DATA_TYPE_OFFSET 6        // 标签的数据类型位
#define NDEF_PROTOCOL_VALID_DATA_OFFSET 20      // 有效数据位

// 货物分拣信息结构体
typedef struct {
    uint32_t jiangsu_count;   // 江苏货物数量 (对应0)
    uint32_t zhejiang_count;  // 浙江货物数量 (对应1)
    uint32_t shanghai_count;  // 上海货物数量 (对应2)
} cargo_sort_info_t;

// NFC页缓冲区
extern uint8_t nfcPageBuffer[NFC_PAGE_SIZE];

/**
 * @brief  从Page页中组成NDEF协议的包裹
 * @note
 * @param  *dataBuff: 最终的内容
 * @param  dataBuff_MaxSize: 存储缓冲区的长度
 * @retval
 */
uint32_t get_NDEFDataPackage(uint8_t *dataBuff, const uint16_t dataBuff_MaxSize);

/**
 * @brief  NFC传感器的引脚初始化
 * @note
 * @retval
 */
uint32_t nfc_Init(void);

/**
 * @brief  模拟NT3H读取头部信息
 * @param  ndefLen: NDEF数据长度
 * @param  ndef_Header: NDEF头部信息
 * @retval true/false
 */
bool NT3HReadHeaderNfc(uint8_t *ndefLen, uint8_t *ndef_Header);

/**
 * @brief  模拟NT3H读取用户数据
 * @param  page: 页号
 * @retval true/false
 */
bool NT3HReadUserData(uint8_t page);

/**
 * @brief  向NFC写入货物分拣信息
 * @param  cargo_info: 货物分拣信息
 * @retval 错误码
 */
errcode_t nfc_write_cargo_info(const cargo_sort_info_t *cargo_info);

/**
 * @brief  从NFC读取货物分拣信息
 * @param  cargo_info: 货物分拣信息
 * @retval 错误码
 */
errcode_t nfc_read_cargo_info(cargo_sort_info_t *cargo_info);

/**
 * @brief  NFC任务函数
 * @param  arg: 参数
 */
void nfc_task(void *arg);

/**
 * @brief  初始化NFC功能
 * @retval 错误码
 */
errcode_t nfc_module_init(void);

#endif /* __HAL_BSP_NFC_H__ */
