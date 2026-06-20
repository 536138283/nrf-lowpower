# nRF52832 低功耗定时 BLE 广播

本项目是一套基于 **Nordic nRF5 SDK + S132 SoftDevice** 的 nRF52832 低功耗 BLE 定时广播程序。
设备只做周期性、不可连接、不可扫描的 Legacy Advertising，不开启 GATT 服务，也不接受连接；每次短时间广播后立即停止广播并回到 System ON 低功耗睡眠。

## 快速开始

1. 安装 Nordic nRF5 SDK 17.1.0、GNU Arm Embedded Toolchain 和 nRF Command Line Tools。
2. 设置 SDK 路径并编译：

   ```sh
   make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560
   ```

3. 擦除芯片、烧录 S132 SoftDevice、烧录应用：

   ```sh
   make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560 erase
   make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560 flash_softdevice
   make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560 flash
   ```

## 默认运行参数

| 参数 | 默认值 | 位置 |
| --- | --- | --- |
| 设备名 | `nRF52-LP-ADV` | `src/main.c` |
| 广播类型 | 不可连接、不可扫描、非定向广播 | `src/main.c` |
| 广播间隔 | 100 ms | `ADV_INTERVAL_MS` |
| 单次广播时长 | 1 s | `ADV_ON_TIME_MS` |
| 广播周期 | 60 s | `ADV_PERIOD_MS` |
| Company ID | `0xFFFF`，仅开发占位 | `APP_COMPANY_IDENTIFIER` |

## 文档目录

- [架构说明](docs/architecture.md)：程序模块、启动流程、广播启停流程。
- [构建与烧录](docs/build_and_flash.md)：工具链、SDK 路径、SoftDevice、Makefile 目标和内存布局。
- [广播数据格式](docs/advertising_payload.md)：广播包字段和 manufacturer data 结构。
- [低功耗调优与测试](docs/low_power_tuning.md)：功耗设计点、参数调优方向和电流测试 checklist。

## 代码结构

```text
.
├── Makefile              # nRF5 SDK GCC 构建和烧录入口
├── nrf52832_xxaa.ld      # S132 v7.x 应用链接脚本
├── sdk_config.h          # nRF5 SDK 模块与低功耗配置
├── src/main.c            # 定时广播主程序
└── docs/                 # 项目说明文档
```

## 生产前必须确认

- 将 `APP_COMPANY_IDENTIFIER` 从 `0xFFFF` 替换为你自己的 Bluetooth SIG Company Identifier。
- 根据你的 S132 SoftDevice 版本确认 `nrf52832_xxaa.ld` 中的 Flash/RAM 起始地址。
- 在真实硬件上使用 PPK2 或等效工具测量完整广播周期的平均电流。
- 根据接收端扫描策略调整广播周期、广播时长和广播间隔。
