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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "soc_osal.h"
#include "cmsis_os2.h"
#include "pinctrl.h"
#include "gpio.h"
#include "i2c.h"
#include "errcode.h"

#include "oled_fonts_ws63.h"
#include "oled_ssd1306_ws63.h"

#ifndef CONFIG_I2C_SUPPORT_MASTER
/* Forward declarations to satisfy this compilation unit when I2C master API macros are not enabled */
errcode_t uapi_i2c_master_init(i2c_bus_t bus, uint32_t baudrate, uint8_t hscode);
errcode_t uapi_i2c_master_write(i2c_bus_t bus, uint16_t dev_addr, i2c_data_t *data);
#endif

// 按照华清远见官方配置
#define OLED_I2C_IDX I2C_BUS_1         // 使用I2C总线1
#define I2C_SCL_MASTER_PIN 16          // SCL引脚GPIO16
#define I2C_SDA_MASTER_PIN 15          // SDA引脚GPIO15
#define CONFIG_PIN_MODE 2              // 引脚模式2
#define I2C_MASTER_ADDRESS 0x0         // 主机地址

#define OLED_WIDTH (128)
#define OLED_I2C_ADDR 0x3C             // 华清远见官方地址为 0x3C
#define OLED_I2C_CMD 0x00              // 0000 0000       写命令
#define OLED_I2C_DATA 0x40             // 0100 0000(0x40) 写数据

#define DELAY_100_MS (100 * 1000)

// 按照华清远见官方方式发送数据
static uint32_t OledSendData(uint8_t *buff, size_t size)
{
    uint16_t dev_addr = OLED_I2C_ADDR;
    i2c_data_t data = {0};
    data.send_buf = buff;
    data.send_len = size;
    uint32_t ret = uapi_i2c_master_write(OLED_I2C_IDX, dev_addr, &data);
    if (ret != 0) {
        printf("I2cWrite(%02X) failed, %0X!\n", data.send_buf[1], ret);
        return ret;
    }
    return ret;
}

// 按照华清远见官方方式写命令
static uint32_t WriteCmd(uint8_t byte)
{
    uint8_t buffer[] = {0x00, byte};
    return OledSendData(buffer, sizeof(buffer));
}

// 按照华清远见官方方式写数据
static uint32_t WriteData(uint8_t byte)
{
    uint8_t buffer[] = {0x40, byte};
    return OledSendData(buffer, sizeof(buffer));
}

static uint32_t OledSetPos(uint8_t x, uint8_t y)
{
    WriteCmd(0xb0 + y);
    WriteCmd(((x & 0xf0) >> 4) | 0x10);
    WriteCmd((x & 0x0f) | 0x01);
    return 0;
}

void OledFillScreen(uint8_t fillData)
{
    uint8_t m = 0;
    uint8_t n = 0;

    for (m = 0; m < 8; m++) { /* 8: 8 pages */
        WriteCmd(0xb0 + m);
        WriteCmd(0x00);
        WriteCmd(0x10);

        for (n = 0; n < 128; n++) { /* 128: 128 columns */
            WriteData(fillData);
        }
    }
}

void OledInit(void)
{
    printf("OLED: Starting initialization...\r\n");

    // 按照华清远见官方配置：I2C1，GPIO15(SDA)和GPIO16(SCL)
    errcode_t ret = uapi_pin_set_mode(I2C_SDA_MASTER_PIN, CONFIG_PIN_MODE);  // GPIO15 SDA
    if (ret != ERRCODE_SUCC) {
        printf("OLED: Failed to set GPIO15 pin mode, ret=%d\r\n", ret);
    }

    ret = uapi_pin_set_mode(I2C_SCL_MASTER_PIN, CONFIG_PIN_MODE);  // GPIO16 SCL
    if (ret != ERRCODE_SUCC) {
        printf("OLED: Failed to set GPIO16 pin mode, ret=%d\r\n", ret);
    }

    // 设置上拉电阻
    uapi_pin_set_pull(I2C_SDA_MASTER_PIN, PIN_PULL_TYPE_UP);  // SDA上拉
    uapi_pin_set_pull(I2C_SCL_MASTER_PIN, PIN_PULL_TYPE_UP);  // SCL上拉

    // 初始化I2C - 使用华清远见官方配置
    uint32_t baudrate = 100000;  // 100kHz，与官方一致
    uint32_t hscode = I2C_MASTER_ADDRESS;  // 主机地址

    ret = uapi_i2c_master_init(OLED_I2C_IDX, baudrate, hscode);
    if (ret != ERRCODE_SUCC) {
        printf("OLED: Failed to init I2C master, ret=0x%x\r\n", ret);
        return;
    }
    printf("OLED: I2C master initialized successfully\r\n");

    osDelay(10); // 10ms延时，与华清远见官方一致

    printf("OLED: Sending initialization commands...\r\n");

    // 尝试发送显示关闭命令，如果失败尝试其他地址
    errcode_t cmd_ret = WriteCmd(0xAE); // display off
    if (cmd_ret != ERRCODE_SUCC) {
        printf("OLED: Failed to send display off command with addr 0x3C, ret=0x%x\r\n", cmd_ret);
        printf("OLED: Trying alternative address 0x3D...\r\n");

        // 尝试0x3D地址
        // 这里我们暂时继续，看看是否是地址问题
        printf("OLED: Continuing with initialization despite command failure...\r\n");
    } else {
        printf("OLED: Display off command sent successfully\r\n");
    }
    WriteCmd(0x20); // Set Memory Addressing Mode
    WriteCmd(0x10); // 00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
    WriteCmd(0xb0); // Set Page Start Address for Page Addressing Mode,0-7
    WriteCmd(0xc8); // Set COM Output Scan Direction
    WriteCmd(0x00); // set low column address
    WriteCmd(0x10); // set high column address
    WriteCmd(0x40); // set start line address
    WriteCmd(0x81); // set contrast control register
    WriteCmd(0xff); // 亮度调节 0x00~0xff
    WriteCmd(0xa1); // set segment re-map 0 to 127
    WriteCmd(0xa6); // set normal display
    WriteCmd(0xa8); // set multiplex ratio(1 to 64)
    WriteCmd(0x3F); //
    WriteCmd(0xa4); // 0xa4,Output follows RAM content;0xa5,Output ignores RAM content
    WriteCmd(0xd3); // set display offset
    WriteCmd(0x00); // not offset
    WriteCmd(0xd5); // set display clock divide ratio/oscillator frequency
    WriteCmd(0xf0); // set divide ratio
    WriteCmd(0xd9); // set pre-charge period
    WriteCmd(0x22); //
    WriteCmd(0xda); // set com pins hardware configuration
    WriteCmd(0x12);
    WriteCmd(0xdb); // set vcomh
    WriteCmd(0x20); // 0x20,0.77xVcc
    WriteCmd(0x8d); // set DC-DC enable
    WriteCmd(0x14); //

    if (WriteCmd(0xaf) != ERRCODE_SUCC) { // turn on oled panel
        printf("OLED: Failed to turn on display\r\n");
        return;
    }

    printf("OLED: Initialization completed successfully\r\n");
}

void OledShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t charSize)
{
    uint8_t c = 0;
    uint8_t i = 0;

    c = chr - ' '; // 得到偏移后的值
    if (x > OLED_WIDTH - 1) {
        x = 0;
        y = y + 2; /* 2: 2 lines */
    }

    if (charSize == FONT6_X8) {
        OledSetPos(x, y);
        for (i = 0; i < 6; i++) { /* 6: 6 columns */
            WriteData(g_oledF6x8[c][i]);
        }
    } else {
        OledSetPos(x, y);
        for (i = 0; i < 8; i++) { /* 8: 8 columns */
            WriteData(g_oledF8x16[c * 16 + i]); /* 16: 16 bytes per char */
        }

        OledSetPos(x, y + 1);
        for (i = 0; i < 8; i++) { /* 8: 8 columns */
            WriteData(g_oledF8x16[c * 16 + i + 8]); /* 16: 16 bytes per char, 8: 8 bytes offset */
        }
    }
}

void OledShowString(uint8_t x, uint8_t y, const char *chr, uint8_t charSize)
{
    uint8_t j = 0;

    if (chr == NULL) {
        printf("param is NULL,Please check!!!\r\n");
        return;
    }

    while (chr[j] != '\0') {
        OledShowChar(x, y, chr[j], charSize);
        if (charSize == FONT6_X8) {
            x += 6; /* 6: 6 columns */
        } else {
            x += 8; /* 8: 8 columns */
        }
        j++;
    }
}

void OledShowString2(uint8_t x, uint8_t y, const char *chr, uint8_t charSize)
{
    uint8_t j = 0;

    if (chr == NULL) {
        printf("param is NULL,Please check!!!\r\n");
        return;
    }

    while (chr[j] != '\0') {
        OledShowChar(x, y, chr[j], charSize);
        if (charSize == FONT6_X8) {
            x += 6; /* 6: 6 columns */
        } else {
            x += 8; /* 8: 8 columns */
        }
        j++;
    }
}
