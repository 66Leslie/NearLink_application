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

#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "lwip/ip4_addr.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "soc_osal.h"
#include "cmsis_os2.h"
#include "uart.h"
#include "chip_io.h"

#include "wifi_config_ws63.h"
#include "oled_ssd1306_ws63.h"
#include "wifi_sta_connect_ws63.h"
#include "udp_server_ws63.h"

// 全局变量：保存UDP socket和客户端地址
static int g_sockfd = -1;
static struct sockaddr_in g_client_addr = {0};
static socklen_t g_client_addr_len = sizeof(g_client_addr);
static int g_client_connected = 0; // 标记是否已收到过小程序消息

extern unsigned char uartWriteBuff[];
extern char expressBoxNum[];
extern uint8_t index_line;



// 供其他文件调用来发送UDP数据
void UdpSend(const char* buf, size_t len)
{
    if (g_sockfd == -1 || !g_client_connected) {
        printf("[UDP] Cannot send: socket not ready or client not connected\n");
        return;
    }

    if (buf == NULL || len == 0) {
        printf("[UDP] Invalid buffer or length\n");
        return;
    }

    ssize_t sent = sendto(g_sockfd, buf, len, 0, (struct sockaddr *)&g_client_addr, g_client_addr_len);
    if (sent > 0) {
        printf("[UDP] Sent %d bytes to client\n", (int)sent);
    } else {
        printf("[UDP] Failed to send data to client\n");
    }
}

int UdpTransportInit(struct sockaddr_in serAddr, struct sockaddr_in remoteAddr)
{
    UNUSED(remoteAddr);  // 标记未使用的参数

    int sServer = socket(AF_INET, SOCK_DGRAM, 0);
    if (sServer < 0) {
        printf("[UDP]create server socket failed\r\n");
        return -1;
    }

    // 本地主机ip和端口号
    serAddr.sin_family = AF_INET;
    serAddr.sin_port = htons(HOST_PORT);
    serAddr.sin_addr.s_addr = inet_addr(get_local_ip());
    if (bind(sServer, (struct sockaddr*)&serAddr, sizeof(serAddr)) == -1) {
        printf("[UDP]bind socket failed\r\n");
        lwip_close(sServer);
        return -1;
    }

    return sServer;
}

void UdpServerDemo(void *arg)
{
    UNUSED(arg);  // 标记未使用的参数

    struct sockaddr_in serAddr = {0};
    struct sockaddr_in remoteAddr = {0};
    static int recvDataFlag = -1;
    char *sendData = NULL;
    int sServer = 0;

    printf("[UDP]initing...\r\n");
    sServer = UdpTransportInit(serAddr, remoteAddr);
    if(sServer < 0) return;

    // 保存socket描述符到全局变量
    g_sockfd = sServer;

    int addrLen = sizeof(remoteAddr);
    static char recvData[UDP_RECV_LEN] = {0};

    while (1) {
        printf("[UDP]waiting for data on Port:%d...\r\n", HOST_PORT);

        // 接收数据
        ssize_t recvLen = recvfrom(sServer, recvData, sizeof(recvData) - 1, 0,
                                   (struct sockaddr *)&remoteAddr, (socklen_t *)&addrLen);
        
        if (recvLen > 0) {
            recvData[recvLen] = '\0';
            printf("[UDP]recv %d bytes: %s\r\n", (int)recvLen, recvData);
            
            // 保存客户端地址信息
            if (!g_client_connected) {
                g_client_addr = remoteAddr;
                g_client_addr_len = addrLen;
                g_client_connected = 1;
                printf("[UDP]Client connected from %s:%d\r\n", 
                       inet_ntoa(remoteAddr.sin_addr), ntohs(remoteAddr.sin_port));
            }

            // 处理接收到的命令
            if (strstr(recvData, "CONNECT_REQUEST") != NULL) {
                printf(">>> Connection request received.\n");
                recvDataFlag = 1;
                sendData = "CONNECT_OK";  // 修改为小程序期望的响应

                // 发送连接响应
                ssize_t sentLen = sendto(sServer, sendData, strlen(sendData), 0,
                                         (struct sockaddr *)&remoteAddr, addrLen);
                if (sentLen > 0) {
                    printf("[UDP]send connect response: %s\r\n", sendData);
                } else {
                    printf("[UDP]send connect response failed\r\n");
                }
                recvDataFlag = -1;

            } else if (strstr(recvData, "_change_position") != NULL) {
                printf("Control equipment information received:%s\r\n", recvData);
                recvDataFlag = 1;

                // 按照原来3861的逻辑解析位置命令
                uint8_t value_flag = 17;  // "_change_position" 后面的位置
                uint16_t pwm_value = 0;
                while (recvData[value_flag] != '_') {
                    pwm_value *= 10;
                    pwm_value += (recvData[value_flag++] - 48);
                }
                pwm_value = 20 * pwm_value + 500;  // 转换为PWM值

                uartWriteBuff[2] = pwm_value / 1000 + 48;
                uartWriteBuff[3] = pwm_value / 100 % 10 + 48;
                uartWriteBuff[4] = pwm_value / 10 % 10 + 48;

                switch (recvData[16]) {  // 舵机ID位置
                    case '0':
                        uartWriteBuff[1] = '3';
                        break;
                    case '1':
                        uartWriteBuff[1] = '2';
                        break;
                    case '2':
                        uartWriteBuff[1] = '1';
                        break;
                    case '3':
                        uartWriteBuff[1] = '0';
                        break;
                    default:
                        break;
                }

                // 确保协议头为0xFF
                uartWriteBuff[0] = 0xFF;
                int32_t write_len = uapi_uart_write(UART_BUS_2, uartWriteBuff, 5, 0);
                if (write_len == 5) {
                    printf("Uart Write data: len = %d\r\n", write_len);
                }

            } else if (strstr(recvData, "_change_speed") != NULL) {
                printf("Control equipment information received:%s\r\n", recvData);
                recvDataFlag = 1;

                // 按照原来3861的逻辑处理速度命令
                uartWriteBuff[1] = '7';
                switch (recvData[13]) {  // "_change_speed" 后面的速度值位置
                    case '0':
                        uartWriteBuff[2] = '0';
                        uartWriteBuff[3] = '5';
                        uartWriteBuff[4] = '0';
                        break;
                    case '1':
                        uartWriteBuff[2] = '1';
                        uartWriteBuff[3] = '0';
                        uartWriteBuff[4] = '6';
                        break;
                    case '2':
                        uartWriteBuff[2] = '1';
                        uartWriteBuff[3] = '7';
                        uartWriteBuff[4] = '8';
                        break;
                    case '3':
                        uartWriteBuff[2] = '2';
                        uartWriteBuff[3] = '4';
                        uartWriteBuff[4] = '0';
                        break;
                    default:
                        break;
                }

                // 确保协议头为0xFF
                uartWriteBuff[0] = 0xFF;
                int32_t write_len = uapi_uart_write(UART_BUS_2, uartWriteBuff, 5, 0);
                if (write_len == 5) {
                    printf("Uart Write data: len = %d\r\n", write_len);
                }

            } else if (strstr(recvData, "_refresh") != NULL) {
                printf("Control equipment information received:%s\r\n", recvData);
                recvDataFlag = -1;

                sendData = expressBoxNum;
                ssize_t sentLen = sendto(sServer, sendData, strlen(sendData), 0,
                                         (struct sockaddr *)&remoteAddr, addrLen);
                if (sentLen > 0) {
                    printf("[UDP]send refresh response: %s\r\n", sendData);
                }

            } else if (strstr(recvData, "_cargo_status") != NULL) {
                printf("Cargo status request received\r\n");
                recvDataFlag = -1;

                // 获取当前货物数据并发送给小程序
                extern void get_current_cargo_counts(uint32_t *js, uint32_t *zj, uint32_t *sh);
                uint32_t js, zj, sh;
                get_current_cargo_counts(&js, &zj, &sh);
                
                static char cargo_response[128];
                snprintf(cargo_response, sizeof(cargo_response), 
                        "CARGO_DATA:J=%u,Z=%u,S=%u", js, zj, sh);
                
                ssize_t sentLen = sendto(sServer, cargo_response, strlen(cargo_response), 0,
                                         (struct sockaddr *)&remoteAddr, addrLen);
                if (sentLen > 0) {
                    printf("[UDP]send cargo status: %s\r\n", cargo_response);
                }

            } else if (strstr(recvData, "UnoladPage") != NULL) {
                printf("The applet exits the current interface\r\n");

            } else if (recvLen == 1 && (recvData[0] == 'H' || recvData[0] == 'G' ||
                                       recvData[0] == 'M' || recvData[0] == 'E' ||
                                       recvData[0] == 'P' || recvData[0] == 'Q' ||
                                       recvData[0] == 'C' || recvData[0] == 'I' ||
                                       recvData[0] == 'J' || recvData[0] == 'K' ||
                                       recvData[0] == 'L')) {
                printf("Single character command received: %c\r\n", recvData[0]);
                recvDataFlag = 1;

                // 将单字符命令封装为 0xFF + op + '0''0''0' 的5字节协议发送给控制机
                uartWriteBuff[0] = 0xFF;
                uartWriteBuff[1] = (unsigned char)recvData[0];
                uartWriteBuff[2] = '0';
                uartWriteBuff[3] = '0';
                uartWriteBuff[4] = '0';

                // 确保协议头为0xFF
                uartWriteBuff[0] = 0xFF;
                int32_t write_len = uapi_uart_write(UART_BUS_2, (unsigned char *)uartWriteBuff, 5, 0);
                if (write_len == 5) {
                    printf("UART sent framed cmd: 0xFF %c 000\r\n", recvData[0]);
                }

                // 发送确认响应
                sendData = "device_cmd_ok";
                ssize_t sentLen = sendto(sServer, sendData, strlen(sendData), 0,
                                         (struct sockaddr *)&remoteAddr, addrLen);
                if (sentLen > 0) {
                    printf("[UDP]send cmd response: %s\r\n", sendData);
                }

            } else if (recvLen >= 1 && recvData[0] >= '0' && recvData[0] <= '9') {
                printf(">>> Received number command: %c (len=%d)\n", recvData[0], (int)recvLen);
                recvDataFlag = 2;  // RECV_DATA_FLAG_OTHER

                // 将接收到的字符转换为数字并更新OLED显示
                index_line = recvData[0] - '0';
                printf("Updating OLED display to show: %c (index_line=%d)\n", recvData[0], index_line);
                OledShowChar(60, 5, recvData[0], FONT6_X8);
                printf("OLED display updated\n");

            } else if (strstr(recvData, WECHAT_MSG_LIGHT_OFF) != NULL) {
                printf(">>> Light OFF command recognized.\n");
                recvDataFlag = 1;

                switch (recvData[10]) {
                    case '0':
                        uartWriteBuff[1] = '4';
                        uartWriteBuff[2] = '0';
                        uartWriteBuff[3] = '5';
                        uartWriteBuff[4] = '8';
                        break;
                    case '1':
                        uartWriteBuff[1] = '5';
                        uartWriteBuff[2] = '0';
                        uartWriteBuff[3] = '5';
                        uartWriteBuff[4] = '0';
                        break;
                    case '2':
                        uartWriteBuff[1] = '6';
                        uartWriteBuff[2] = '0';
                        uartWriteBuff[3] = '4';
                        uartWriteBuff[4] = '0';
                        break;
                    default:
                        break;
                }
                
                // 通过UART发送数据
                // 确保协议头为0xFF
                uartWriteBuff[0] = 0xFF;
                int32_t write_len = uapi_uart_write(UART_BUS_2, (unsigned char *)uartWriteBuff, 5, 0);
                if (write_len == 5) {
                    printf("Uart Write data: len = %d\r\n", write_len);
                }

            } else if (strstr(recvData, WECHAT_MSG_LIGHT_ON) != NULL) {
                printf(">>> Light ON command recognized.\n");
                recvDataFlag = 1;

                switch (recvData[9]) {
                    case '0':
                        uartWriteBuff[1] = '4';
                        uartWriteBuff[2] = '1';
                        uartWriteBuff[3] = '5';
                        uartWriteBuff[4] = '8';
                        break;
                    case '1':
                        uartWriteBuff[1] = '5';
                        uartWriteBuff[2] = '1';
                        uartWriteBuff[3] = '5';
                        uartWriteBuff[4] = '0';
                        break;
                    case '2':
                        uartWriteBuff[1] = '6';
                        uartWriteBuff[2] = '1';
                        uartWriteBuff[3] = '4';
                        uartWriteBuff[4] = '0';
                        break;
                    default:
                        break;
                }
                
                // 通过UART发送数据
                // 确保协议头为0xFF
                uartWriteBuff[0] = 0xFF;
                int32_t write_len = uapi_uart_write(UART_BUS_2, (unsigned char *)uartWriteBuff, 5, 0);
                if (write_len == 5) {
                    printf("Uart Write data: len = %d\r\n", write_len);
                }
            } else if (strstr(recvData, WECHAT_MSG_BLOCKER_ON) != NULL) {
                // 阻拦器开启，等价于 _light_on0
                printf(">>> Blocker ON command recognized.\n");
                recvDataFlag = 1;

                uartWriteBuff[1] = '4';
                uartWriteBuff[2] = '1';
                uartWriteBuff[3] = '5';
                uartWriteBuff[4] = '8';

                // 确保协议头为0xFF
                uartWriteBuff[0] = 0xFF;
                int32_t write_len = uapi_uart_write(UART_BUS_2, (unsigned char *)uartWriteBuff, 5, 0);
                if (write_len == 5) {
                    printf("Uart Write data (blocker on): len = %d\r\n", write_len);
                }

            } else if (strstr(recvData, WECHAT_MSG_BLOCKER_OFF) != NULL) {
                // 阻拦器关闭，等价于 _light_off0
                printf(">>> Blocker OFF command recognized.\n");
                recvDataFlag = 1;

                uartWriteBuff[1] = '4';
                uartWriteBuff[2] = '0';
                uartWriteBuff[3] = '5';
                uartWriteBuff[4] = '8';

                // 确保协议头为0xFF
                uartWriteBuff[0] = 0xFF;
                int32_t write_len = uapi_uart_write(UART_BUS_2, (unsigned char *)uartWriteBuff, 5, 0);
                if (write_len == 5) {
                    printf("Uart Write data (blocker off): len = %d\r\n", write_len);
                }

            } else if (strstr(recvData, WECHAT_MSG_EJECTOR_ON) != NULL) {
                // 弹出器开启，默认控制弹出器1，若带编号则解析
                printf(">>> Ejector ON command recognized.\n");
                recvDataFlag = 1;

                char id_char = '1';
                char *id_ptr = strstr(recvData, WECHAT_MSG_EJECTOR_ON) + strlen(WECHAT_MSG_EJECTOR_ON);
                if (id_ptr && (*id_ptr == '1' || *id_ptr == '2')) {
                    id_char = *id_ptr;
                }
                if (id_char == '1') {
                    uartWriteBuff[1] = '5';
                    uartWriteBuff[2] = '1';
                    uartWriteBuff[3] = '5';
                    uartWriteBuff[4] = '0';
                } else { // '2'
                    uartWriteBuff[1] = '6';
                    uartWriteBuff[2] = '1';
                    uartWriteBuff[3] = '4';
                    uartWriteBuff[4] = '0';
                }

                int32_t write_len = uapi_uart_write(UART_BUS_2, (unsigned char *)uartWriteBuff, 5, 0);
                if (write_len == 5) {
                    printf("Uart Write data (ejector on %c): len = %d\r\n", id_char, write_len);
                }

            } else if (strstr(recvData, WECHAT_MSG_EJECTOR_OFF) != NULL) {
                // 弹出器关闭，默认控制弹出器1，若带编号则解析
                printf(">>> Ejector OFF command recognized.\n");
                recvDataFlag = 1;

                char id_char = '1';
                char *id_ptr = strstr(recvData, WECHAT_MSG_EJECTOR_OFF) + strlen(WECHAT_MSG_EJECTOR_OFF);
                if (id_ptr && (*id_ptr == '1' || *id_ptr == '2')) {
                    id_char = *id_ptr;
                }
                if (id_char == '1') {
                    uartWriteBuff[1] = '5';
                    uartWriteBuff[2] = '0';
                    uartWriteBuff[3] = '5';
                    uartWriteBuff[4] = '0';
                } else { // '2'
                    uartWriteBuff[1] = '6';
                    uartWriteBuff[2] = '0';
                    uartWriteBuff[3] = '4';
                    uartWriteBuff[4] = '0';
                }

                int32_t write_len = uapi_uart_write(UART_BUS_2, (unsigned char *)uartWriteBuff, 5, 0);
                if (write_len == 5) {
                    printf("Uart Write data (ejector off %c): len = %d\r\n", id_char, write_len);
                }
            } else {
                printf(">>> Received unknown command: %s\n", recvData);
                recvDataFlag = 2;  // RECV_DATA_FLAG_OTHER
            }

            // 按照原来3861的逻辑发送响应
            if (recvDataFlag == 1) {
                sendData = "device_light_on";
                ssize_t sentLen = sendto(sServer, sendData, strlen(sendData), 0,
                                         (struct sockaddr *)&remoteAddr, addrLen);
                if (sentLen > 0) {
                    printf("[UDP]send response: %s\r\n", sendData);
                }
            } else if (recvDataFlag == 0) {
                sendData = "device_light_off";
                ssize_t sentLen = sendto(sServer, sendData, strlen(sendData), 0,
                                         (struct sockaddr *)&remoteAddr, addrLen);
                if (sentLen > 0) {
                    printf("[UDP]send response: %s\r\n", sendData);
                }
            } else if (recvDataFlag == 2) {
                sendData = "Received a message from the server";
                ssize_t sentLen = sendto(sServer, sendData, strlen(sendData), 0,
                                         (struct sockaddr *)&remoteAddr, addrLen);
                if (sentLen > 0) {
                    printf("[UDP]send response: %s\r\n", sendData);
                }
            }

            // 重置标志位，为下一次接收做准备
            recvDataFlag = -1;
        } else {
            printf("[UDP]recv failed, error: %d\r\n", errno);
        }
        
        osDelay(10); // 短暂延时
    }

    lwip_close(sServer);
}
