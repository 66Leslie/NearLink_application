# NearLink 应用示例仓库

本仓库收录了全国大学生嵌入式与芯片设计大赛（海思赛道）的两个示例工程，演示两块 WS63 海思开发板之间的通信：其中一块在目录名中以“63B”标识，表示另一块板子。工程涵盖星闪（SLE）服务器、WiFi/UDP 通信、UART 转发以及 OLED 显示等功能，便于在两板联调时验证星闪与常见外设的协同工作。

相关成果获得了 2025 年全国大学生嵌入式与芯片设计大赛应用赛道的国家三等奖，本仓库对应其中的星闪通讯环节。

## 仓库结构

- `comm_host_63B/`：以目录名“63B”指代的 WS63(B) 板侧示例，运行星闪服务器，从另一块 WS63 板（A 侧）获取货物分拣信息并在 SSD1306 OLED 上循环显示江苏/浙江/上海的分拣计数。【F:comm_host_63B/comm_host_63B.c†L28-L100】【F:comm_host_63B/sle_server_63B.h†L26-L57】
- `comm_host_ws63/`：WS63(A) 板侧示例，包含 WiFi STA 连接、UDP 服务器、小程序通信、UART 解析与转发、分拣统计和 OLED 显示，并附带详细的硬件接线与构建说明（见子目录 `README.md`）。【F:comm_host_ws63/README.md†L4-L80】【F:comm_host_ws63/comm_host_ws63.c†L22-L136】

## 快速开始

### 环境准备

1. 安装对应平台的开发工具链与 SDK（如 HiSpark Studio / LiteOS 开发环境）。
2. 确保能够通过 USB/JTAG 将固件烧录到目标开发板，并能查看串口日志。
3. 准备一块 SSD1306 OLED、按键与必要的 UART、WiFi 连接设备。

### 构建与烧录

#### WS63(B) 板示例

1. 将 `comm_host_63B` 目录放入 WS63 平台 SDK 的应用示例路径，并在构建系统中包含其 `CMakeLists.txt`。
2. 在 SDK 的配置界面（如 menuconfig）启用 `COMM_HOST_63B` 示例后编译生成固件。
3. 通过开发板工具烧录生成的固件，启动后可在 OLED 上查看实时的分拣统计与星闪连接状态。

#### WS63(A) 板示例

1. 参照 `comm_host_ws63/README.md` 调整 `wifi_config_ws63.h` 中的 AP 名称与密码，并按需修改 UDP 端口配置。【F:comm_host_ws63/README.md†L49-L78】
2. 在 HiSpark Studio 中开启 `Support COMM_HOST_WS63 Sample` 选项后，执行 `python build.py ws63-liteos-app` 进行编译。【F:comm_host_ws63/README.md†L83-L93】
3. 烧录生成的固件，上电后设备会自动连网、启动 UDP 服务、监听 UART，并在 OLED 上显示 IP 与分拣状态。

## 功能亮点

- **星闪数据展示（WS63-B）**：通过星闪服务器收集货物信息，周期性刷新 OLED，直观显示各地区分拣数量及连接状态。【F:comm_host_63B/comm_host_63B.c†L28-L100】
- **多通路数据交换（WS63-A）**：UART 接收的分拣指令会被解析并同步到 UDP 小程序，同时统计结果在 OLED 上更新，形成“串口 ↔ WiFi ↔ 星闪”三向协同链路。【F:comm_host_ws63/comm_host_ws63.c†L36-L136】【F:comm_host_ws63/README.md†L13-L44】

如需了解具体 GPIO 分配、网络调试或小程序通信格式，请查阅对应子目录下的源代码与文档。
