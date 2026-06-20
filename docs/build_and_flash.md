# 构建与烧录

## 依赖

建议使用以下环境：

- Nordic nRF5 SDK 17.1.0。
- S132 SoftDevice v7.x；本项目使用 Broadcaster / non-connectable Legacy Advertising。
- GNU Arm Embedded Toolchain，支持 Cortex-M4F hard-float。
- nRF Command Line Tools，提供 `nrfjprog`。
- 目标板：PCA10040 / nRF52 DK，或等价 nRF52832 自研板。

## 编译

```sh
make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560
```

如果你的 SDK 固定在默认路径，也可以直接修改 `Makefile` 顶部的 `SDK_ROOT` 默认值。

## 烧录

完整烧录流程：

```sh
make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560 erase
make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560 flash_softdevice
make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560 flash
```

只更新应用时：

```sh
make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560 flash
```

## Makefile 目标

| 目标 | 说明 |
| --- | --- |
| `make` / `make default` | 编译应用 hex。 |
| `make erase` | 调用 `nrfjprog --eraseall` 擦除整片。 |
| `make flash_softdevice` | 烧录 S132 SoftDevice hex。 |
| `make flash` | 烧录应用 hex 并复位。 |

## 内存布局

当前 `nrf52832_xxaa.ld` 假设使用 S132 v7.x：

| 区域 | 起始地址 | 长度 |
| --- | --- | --- |
| Application Flash | `0x26000` | `0x5a000` |
| Application RAM | `0x20002260` | `0xdda0` |

如果更换 SoftDevice 版本，必须按 Nordic 的 SoftDevice release note 或编译输出中的 RAM/Flash 提示同步更新链接脚本。

## 常见问题

### 找不到 `Makefile.common`

说明 `SDK_ROOT` 不正确，或者机器上没有安装 nRF5 SDK。请确认路径下存在：

```text
components/toolchain/gcc/Makefile.common
```

### 找不到 SoftDevice hex

`flash_softdevice` 默认使用：

```text
components/softdevice/s132/hex/s132_nrf52_7.2.0_softdevice.hex
```

如果 SDK 内 SoftDevice 文件名不同，请修改 `Makefile` 中的 `flash_softdevice` 目标。
