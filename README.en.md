[中文](README.md) | **English**

# NearLink Application Examples

Two example projects from the National Undergraduate Embedded Chip and System Design Contest
(HiSilicon track), demonstrating communication between two HiSilicon WS63 development boards. One
board is labelled "63B" in its directory name to distinguish it from the other. The projects cover a
NearLink (SLE) server, Wi-Fi/UDP communication, UART forwarding, and OLED display, so the two-board
setup can validate NearLink working alongside common peripherals.

The work won a National Third Prize in the application track of the 2025 contest; this repository
covers the NearLink communication part.

## Repository layout

- `comm_host_63B/` — the WS63(B) side, which runs the NearLink server, receives sorting information
  from the other WS63 board (side A), and cycles the Jiangsu/Zhejiang/Shanghai sorting counts on an
  SSD1306 OLED.
- `comm_host_ws63/` — the WS63(A) side: Wi-Fi STA connection, UDP server, mini-program
  communication, UART parsing and forwarding, sorting statistics, and OLED display, with detailed
  wiring and build instructions in its own `README.md`.

## Getting started

### Prerequisites

1. Install the toolchain and SDK for the target platform (for example HiSpark Studio / a LiteOS
   development environment).
2. Make sure you can flash firmware to the board over USB/JTAG and read serial logs.
3. Prepare an SSD1306 OLED, buttons, and the UART/Wi-Fi hardware you need.

### Build and flash

#### WS63(B) side

1. Place `comm_host_63B` in the application sample path of the WS63 SDK and include its
   `CMakeLists.txt` in the build system.
2. Enable the `COMM_HOST_63B` sample in the SDK configuration interface (for example `menuconfig`),
   then build the firmware.
3. Flash the firmware. Once it boots, the OLED shows live sorting statistics and NearLink
   connection state.

#### WS63(A) side

1. Following `comm_host_ws63/README.md`, set the AP name and password in `wifi_config_ws63.h` and
   adjust the UDP port configuration as needed.
2. Enable `Support COMM_HOST_WS63 Sample` in HiSpark Studio, then build with
   `python build.py ws63-liteos-app`.
3. Flash the firmware. After power-up the device connects to the network automatically, starts the
   UDP service, listens on UART, and shows its IP and sorting state on the OLED.

## Highlights

- **NearLink data display (WS63-B)** — collects cargo information through the NearLink server and
  periodically refreshes the OLED, showing per-region sorting counts and connection state.
- **Multi-path data exchange (WS63-A)** — sorting commands received over UART are parsed and
  mirrored to the UDP mini-program while the statistics update on the OLED, forming a three-way
  serial ↔ Wi-Fi ↔ NearLink chain.

For GPIO assignments, network debugging, or the mini-program communication format, see the source
and documents in the relevant subdirectory.
