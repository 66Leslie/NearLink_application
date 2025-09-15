# Comm Host WS63 移植项目

## 项目简介

本项目是将原有的 `comm_host` 项目移植到 WS63 平台的版本。项目实现了一个基于WiFi UDP通信的主机控制系统，具备以下功能：

- WiFi STA模式连接
- UDP服务器功能
- OLED SSD1306显示
- UART通信
- 键盘输入处理

## 文件结构

```
comm_host_ws63/
├── comm_host_ws63.c          # 主程序文件
├── wifi_sta_connect_ws63.c   # WiFi连接模块
├── wifi_sta_connect_ws63.h   # WiFi连接头文件
├── wifi_config_ws63.h        # WiFi配置头文件
├── udp_server_ws63.c         # UDP服务器模块
├── udp_server_ws63.h         # UDP服务器头文件
├── oled_ssd1306_ws63.c       # OLED显示模块
├── oled_ssd1306_ws63.h       # OLED显示头文件
├── oled_fonts_ws63.h         # OLED字体文件
├── CMakeLists.txt            # 编译配置文件
└── README.md                 # 说明文档
```

## 硬件连接

### GPIO引脚分配
- **GPIO10**: 按键输入
- **GPIO7**: UART2 RXD
- **GPIO8**: UART2 TXD
- **GPIO13**: I2C0 SDA (OLED)
- **GPIO14**: I2C0 SCL (OLED)

### 外设连接
1. **OLED显示屏 (SSD1306)**
   - VCC -> 3.3V
   - GND -> GND
   - SDA -> GPIO13
   - SCL -> GPIO14

2. **按键**
   - 一端接GPIO10
   - 另一端接GND

3. **UART设备**
   - RX -> GPIO8 (UART2 TXD)
   - TX -> GPIO7 (UART2 RXD)
   - GND -> GND

## 配置说明

### WiFi配置
在 `wifi_config_ws63.h` 文件中修改WiFi连接参数：

```c
#define AP_SSID  "你的WiFi名称"     // WIFI SSID
#define AP_PWD   "你的WiFi密码"     // WIFI PWD
```

### UDP服务器配置
```c
#define HOST_PORT    5566       // 本地端口
#define DEVICE_PORT  6789       // 设备端口
```

## 编译和烧录

### 1. 配置编译选项
在HiSpark Studio中，通过menuconfig配置：
```
Application samples  --->
    Farsight samples  --->
        [*] Support COMM_HOST_WS63 Sample.
```

### 2. 编译项目
```bash
python build.py ws63-liteos-app
```

### 3. 烧录程序
使用HiSpark Studio的烧录功能将编译生成的固件烧录到开发板。

## 功能说明

### 1. WiFi连接
- 系统启动后自动连接到配置的WiFi网络
- 连接成功后在OLED上显示IP地址

### 2. UDP服务器
- 监听5566端口
- 接收客户端发送的控制命令
- 支持的命令格式：
  - `LIGHT_OFF0` - 关闭0号灯
  - `LIGHT_ON1` - 打开1号灯
  - 等等

### 3. UART通信
- 波特率：115200
- 数据位：8
- 停止位：1
- 校验位：无
- 接收到的数据会通过UDP转发

### 4. 按键控制
- 按下按键可以切换控制序列
- 当前序列号显示在OLED上

### 5. OLED显示
- 显示当前IP地址和端口
- 显示控制序列信息
- 显示系统状态

## 调试说明

### 串口调试
- 波特率：115200
- 可以通过串口查看系统运行日志

### 网络调试
- 可以使用网络调试工具连接到设备的UDP服务器
- IP地址会显示在OLED屏幕上

## 注意事项

1. 确保WiFi配置正确，否则无法连接网络
2. 检查硬件连接，特别是I2C和UART的引脚连接
3. 如果OLED不显示，检查I2C连接和地址设置
4. UART通信问题可能是引脚配置或波特率设置错误

## 移植说明

本项目从原有的Hi3861平台移植到WS63平台，主要修改包括：

1. **GPIO API适配**: 使用WS63的GPIO API替换原有的IoT GPIO API
2. **UART API适配**: 使用WS63的UART API和引脚配置方式
3. **I2C API适配**: 使用WS63的I2C API进行OLED通信
4. **WiFi API适配**: 使用WS63的WiFi连接API
5. **引脚定义更新**: 根据WS63的引脚定义更新GPIO编号

## 版本信息

- 原始版本：基于Hi3861平台的comm_host
- 移植版本：适配WS63平台的comm_host_ws63
- 移植日期：2025年8月7日
