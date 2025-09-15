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

#ifndef OLED_SSD1306_WS63_H
#define OLED_SSD1306_WS63_H

#include <stdint.h>

#define FONT6_X8  1
#define FONT8_X16 2

/**
 * @brief Initialize OLED display
 */
void OledInit(void);

/**
 * @brief Fill screen with specified data
 * @param fillData Data to fill (0x00 for black, 0xFF for white)
 */
void OledFillScreen(uint8_t fillData);

/**
 * @brief Show a character on OLED
 * @param x X coordinate
 * @param y Y coordinate  
 * @param chr Character to display
 * @param charSize Font size (FONT6_X8 or FONT8_X16)
 */
void OledShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t charSize);

/**
 * @brief Show a string on OLED
 * @param x X coordinate
 * @param y Y coordinate
 * @param chr String to display
 * @param charSize Font size (FONT6_X8 or FONT8_X16)
 */
void OledShowString(uint8_t x, uint8_t y, const char *chr, uint8_t charSize);

/**
 * @brief Show a string on OLED (alternative function)
 * @param x X coordinate
 * @param y Y coordinate
 * @param chr String to display
 * @param charSize Font size (FONT6_X8 or FONT8_X16)
 */
void OledShowString2(uint8_t x, uint8_t y, const char *chr, uint8_t charSize);

#endif // OLED_SSD1306_WS63_H
