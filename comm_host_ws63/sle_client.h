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

#ifndef SLE_CLIENT_H
#define SLE_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "errcode.h"
#include "cmsis_os2.h"
#include "securec.h"
#include "sle_device_discovery.h"
#include "sle_connection_manager.h"
#include "sle_ssap_client.h"

// 星闪相关定义
#define SLE_NAME_MAX_LEN    31
#define SLE_SEEK_INTERVAL_DEFAULT 0x60
#define SLE_SEEK_WINDOW_DEFAULT   0x30

// 星闪连接参数
typedef struct {
    uint16_t conn_id;
    uint16_t interval_min;
    uint16_t interval_max;
    uint16_t max_latency;
    uint16_t supervision_timeout;
} sle_connection_param_t;

// 星闪设备信息
typedef struct {
    sle_addr_t addr;
    char name[SLE_NAME_MAX_LEN + 1];
    int8_t rssi;
    bool connected;
} sle_device_info_t;

// 星闪回调函数类型
typedef void (*sle_enable_callback_t)(errcode_t status);
typedef void (*sle_seek_enable_callback_t)(errcode_t status);
typedef void (*sle_seek_disable_callback_t)(errcode_t status);
typedef void (*sle_seek_result_callback_t)(sle_device_info_t *device_info);
typedef void (*sle_connect_callback_t)(uint16_t conn_id, sle_addr_t *addr, errcode_t status);
typedef void (*sle_disconnect_callback_t)(uint16_t conn_id, errcode_t status);

// 星闪回调函数结构
typedef struct {
    sle_enable_callback_t enable_cb;
    sle_seek_enable_callback_t seek_enable_cb;
    sle_seek_disable_callback_t seek_disable_cb;
    sle_seek_result_callback_t seek_result_cb;
    sle_connect_callback_t connect_cb;
    sle_disconnect_callback_t disconnect_cb;
} sle_callbacks_t;

/**
 * @brief  星闪客户端初始化
 * @retval 错误码
 */
errcode_t sle_client_init(void);

/**
 * @brief  创建星闪客户端任务
 * @retval 错误码
 */
errcode_t sle_client_task_init(void);

/**
 * @brief  发送货物数据到星闪服务器
 * @param  jiangsu: 江苏货物数量
 * @param  zhejiang: 浙江货物数量
 * @param  shanghai: 上海货物数量
 */
void sle_client_send_cargo_data(uint32_t jiangsu, uint32_t zhejiang, uint32_t shanghai);

/**
 * @brief  获取星闪连接状态
 * @retval 连接状态
 */
bool sle_client_is_connected(void);

/**
 * @brief  获取当前货物分拣信息 (外部函数)
 * @param  js: 江苏货物数量
 * @param  zj: 浙江货物数量
 * @param  sh: 上海货物数量
 */
extern void get_current_cargo_counts(uint32_t *js, uint32_t *zj, uint32_t *sh);

#endif /* SLE_CLIENT_H */
