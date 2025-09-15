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

#ifndef SLE_SERVER_63B_H
#define SLE_SERVER_63B_H

#include <stdint.h>
#include <stdbool.h>
#include "errcode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

// 货物分拣信息结构体
typedef struct {
    uint32_t jiangsu;    // 江苏货物数量 (00)
    uint32_t zhejiang;   // 浙江货物数量 (01) 
    uint32_t shanghai;   // 上海货物数量 (02)
    uint64_t timestamp;  // 时间戳
    bool valid;          // 数据有效标志
} cargo_info_t;

/**
 * @brief  星闪服务器初始化
 * @retval 错误码
 */
errcode_t sle_server_63B_init(void);

/**
 * @brief  获取最新的货物分拣信息
 * @param  cargo_info: 输出的货物信息
 * @retval 是否获取成功
 */
bool sle_server_get_cargo_info(cargo_info_t *cargo_info);

/**
 * @brief  获取星闪连接状态
 * @retval true=已连接，false=未连接
 */
bool sle_server_is_connected(void);

/**
 * @brief  发送货物数据到星闪客户端
 * @param  jiangsu: 江苏货物数量
 * @param  zhejiang: 浙江货物数量
 * @param  shanghai: 上海货物数量
 * @retval 错误码
 */
errcode_t sle_server_send_cargo_data(uint32_t jiangsu, uint32_t zhejiang, uint32_t shanghai);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* SLE_SERVER_63B_H */
