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

#ifndef WIFI_STA_CONNECT_WS63_H
#define WIFI_STA_CONNECT_WS63_H

/**
 * @brief Get local IP address
 * @return const char* IP address string
 */
const char* get_local_ip(void);

/**
 * @brief Start WiFi STA module and connect to AP
 */
void WifiStaModule(void);

#endif // WIFI_STA_CONNECT_WS63_H
