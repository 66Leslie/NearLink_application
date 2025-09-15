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

#ifndef WIFI_CONFIG_WS63_H
#define WIFI_CONFIG_WS63_H

#define UDP_DEMO

#ifdef UDP_DEMO
#define UDP_DEMO_SUPPORT
#define CONFIG_WIFI_STA_MODULE
#elif defined(UDP_AP_DEMO)
#define UDP_DEMO_SUPPORT
#define CONFIG_WIFI_AP_MODULE
#endif

/**
 * @brief enable HW iot cloud
 * HW iot cloud send message to Hi3861 board and Hi861 board publish message to HW iot cloud
 */

// CONFIG THE LOG
/* if you need the iot log for the development ,
please enable it, else please comment it
*/
#define CONFIG_LINKLOG_ENABLE   1

// CONFIG THE WIFI
/* Please modify the ssid and pwd for the own */
#define AP_SSID  "NNUWiFi"      // WIFI SSID
#define AP_PWD   "zxw66666"     // WIFI PWD

// UDP服务器配置
#define HOST_PORT    5566       // 本地端口
#define DEVICE_PORT  6789       // 设备端口

// UDP消息定义
#define UDP_RECV_LEN 1024

// 小程序消息命令定义
#define WECHAT_MSG_CONNECT      "CONNECT_REQUEST"
#define DEVICE_MSG_CONNECT_SUCCESS "CONNECT_SUCCESS"
#define DEVICE_MSG_CONNECT_OK   "CONNECT_OK"
#define WECHAT_MSG_LIGHT_ON     "_light_on"
#define WECHAT_MSG_LIGHT_OFF    "_light_off"
#define WECHAT_MSG_BLOCKER_ON   "_blocker_on"      // 阻拦器开启命令
#define WECHAT_MSG_BLOCKER_OFF  "_blocker_off"     // 阻拦器关闭命令
#define WECHAT_MSG_EJECTOR_ON   "_ejector_on"      // 弹出器开启命令
#define WECHAT_MSG_EJECTOR_OFF  "_ejector_off"     // 弹出器关闭命令
#define DEVICE_MSG_LIGHT_ON     "device_light_on"
#define DEVICE_MSG_LIGHT_OFF    "device_light_off"
#define DEVICE_MSG_BLOCKER_ON   "device_blocker_on"   // 阻拦器响应
#define DEVICE_MSG_BLOCKER_OFF  "device_blocker_off"  // 阻拦器响应
#define DEVICE_MSG_EJECTOR_ON   "device_ejector_on"   // 弹出器响应
#define DEVICE_MSG_EJECTOR_OFF  "device_ejector_off"  // 弹出器响应
#define WECHAT_MSG_UNLOAD_PAGE  "UnoladPage"
#define WECHAT_MSG_STEERING_POSITION    "_change_position"
#define WECHAT_MSG_SPEED_CHANGE    "_change_speed"
#define WECHAT_MSG_REFRESH    "_refresh"
#define RECV_DATA_FLAG_OTHER    (2)
#define RECV_DATA_FLAG_CONNECTED (3)
/* Duplicate legacy macros removed: keep _light_on/_light_off variants */

#endif // WIFI_CONFIG_WS63_H
