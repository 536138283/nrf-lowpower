# nRF52832 BLE 单广播低功耗方案：nRF5 SDK / S132 代码研究文档

版本：v1.0  
目标芯片：Nordic nRF52832  
推荐 SDK：nRF5 SDK 17.1.0  
推荐 SoftDevice：S132 v7.x  
目标应用：Beacon、资产标签、温湿度广播节点、电子价签状态广播、低频状态上报设备  
设计目标：不可连接、不可扫描、单广播、低功耗、可量产、可用 PPK2 实测验证

---

## 1. 方案边界

本方案专注于 **nRF52832 只做 BLE 单向广播** 的场景：

- 不建立 BLE 连接；
- 不开放 GATT 服务；
- 不做配对、绑定、安全管理；
- 不做扫描响应；
- 不做 BLE Mesh；
- 不做 DFU；
- 主循环只进入低功耗等待；
- 业务数据全部放进 Advertising Data，推荐使用 Manufacturer Specific Data。

这个边界非常重要。很多 nRF5 SDK 示例为了演示 BLE 功能，会初始化 GAP、GATT、服务、连接参数、Peer Manager、DFU、Button、LED、UART Log 等模块。对“纽扣电池单广播产品”而言，这些模块大部分都是不必要的，甚至会引入额外功耗、状态复杂度和量产风险。

---

## 2. SDK 选型结论

### 2.1 推荐组合

| 项目 | 推荐值 |
|---|---|
| SoC | nRF52832 |
| SDK | nRF5 SDK 17.1.0 |
| SoftDevice | S132 |
| 示例工程起点 | `examples/ble_peripheral/ble_app_beacon` |
| 开发板映射 | `pca10040` |
| 编译器 | SES / ARM GCC / Keil 均可，量产推荐固定一种工具链 |
| 广播模式 | Legacy Advertising |
| 广播类型 | Non-connectable, non-scannable, undirected |
| 低功耗模式 | System ON Sleep |
| 定时器 | `app_timer`，底层 RTC |
| 功耗测量 | Nordic Power Profiler Kit II / PPK2 |

### 2.2 为什么不用完整 `ble_advertising` 状态机？

nRF5 SDK 提供 `ble_advertising` 模块，适合连接型外设工程，例如心率计、UART over BLE、传感器 GATT 设备。它可以处理快速广播、慢速广播、超时、连接后停止、断连后重启等复杂状态。

但本项目是单广播 Beacon，推荐直接调用 SoftDevice GAP Advertising API：

- `sd_ble_gap_adv_set_configure()`
- `sd_ble_gap_adv_start()`
- `sd_ble_gap_adv_stop()`
- `sd_ble_gap_tx_power_set()`

直接使用 GAP API 的优点：

1. 状态机更少，功耗行为更可控；
2. 不会误引入连接、扫描响应、白名单、Peer Manager；
3. 代码量小，适合长期维护；
4. PPK2 功耗波形更容易分析；
5. 出现广播异常时更容易定位。

---

## 3. SDK 工程目录建议

推荐从 `ble_app_beacon` 拷贝出独立工程，不建议直接在 SDK 原始目录里改。

```text
project_nrf52832_beacon/
├── main.c
├── app_config.h
├── beacon_adv.c
├── beacon_adv.h
├── beacon_power.c
├── beacon_power.h
├── beacon_battery.c          # 可选
├── beacon_battery.h          # 可选
├── sdk_config.h
├── pca10040/
│   └── s132/
│       ├── armgcc/
│       │   ├── Makefile
│       │   └── beacon_gcc_nrf52.ld
│       └── ses/
│           └── beacon.emProject
└── README.md
```

### 3.1 文件职责

| 文件 | 作用 |
|---|---|
| `main.c` | 初始化顺序、主循环休眠、BLE 事件入口 |
| `app_config.h` | 产品参数集中配置，例如广播间隔、TX Power、Company ID、设备类型 |
| `beacon_adv.c/h` | 广播包构造、广播启动、广播停止、广播数据更新 |
| `beacon_power.c/h` | DCDC、GPIO 默认态、外设关闭、运输模式/System OFF |
| `beacon_battery.c/h` | 电池电压采样，采样后立即反初始化 SAADC |
| `sdk_config.h` | SDK 模块开关，直接影响 RAM、Flash 和功耗 |
| linker script | SoftDevice + Application 的 Flash/RAM 分区 |

---

## 4. 固件状态机

单广播低功耗设备的状态机应当尽量简单。

```text
上电 / 复位
   ↓
时钟、Timer、电源管理初始化
   ↓
板级低功耗配置：DCDC、GPIO、关闭 LED/UART/SPI/TWI/SAADC
   ↓
SoftDevice S132 初始化
   ↓
构造 Advertising Data
   ↓
配置广播参数
   ↓
设置 TX Power
   ↓
启动不可连接、不可扫描广播
   ↓
System ON Sleep
   ↓
RTC/App Timer 周期唤醒：可选更新电池、电量、计数、传感器数据
   ↓
更新广播 Payload
   ↓
继续 System ON Sleep
```

核心原则：**广播本身交给 SoftDevice 调度，应用 CPU 不参与每个广播事件。**

---

## 5. `sdk_config.h` 关键配置

`sdk_config.h` 是 nRF5 SDK 工程里最容易被忽视、但对功耗影响很大的文件。

### 5.1 必须启用的模块

```c
#define NRF_SDH_ENABLED                 1
#define NRF_SDH_BLE_ENABLED             1
#define NRF_PWR_MGMT_ENABLED            1
#define APP_TIMER_ENABLED               1
#define BLE_ADVDATA_ENABLED             1
```

解释：

- `NRF_SDH_ENABLED`：启用 SoftDevice Handler；
- `NRF_SDH_BLE_ENABLED`：启用 BLE 协议栈接口；
- `NRF_PWR_MGMT_ENABLED`：主循环进入低功耗用；
- `APP_TIMER_ENABLED`：用于低频周期任务，底层基于 RTC；
- `BLE_ADVDATA_ENABLED`：使用 SDK 的 `ble_advdata_encode()` 生成广播包。

### 5.2 量产应关闭的模块

```c
#define NRF_LOG_ENABLED                 0
#define NRF_LOG_BACKEND_RTT_ENABLED     0
#define NRF_LOG_BACKEND_UART_ENABLED    0

#define UART_ENABLED                    0
#define NRFX_UART_ENABLED               0
#define NRFX_UARTE_ENABLED              0

#define TWI_ENABLED                     0
#define NRFX_TWI_ENABLED                0
#define SPI_ENABLED                     0
#define NRFX_SPI_ENABLED                0
#define SAADC_ENABLED                   0   // 如果有电池采样，则只在 battery 模块局部启用/反初始化
#define PWM_ENABLED                     0
#define APP_UART_ENABLED                0
```

说明：

- 日志模块在调试阶段有价值，但量产固件必须关闭；
- UART/RTT/SWO 会改变电流波形；
- SAADC、TWI、SPI 等外设不应长期开启；
- LED 闪烁对纽扣电池产品是严重功耗负担，应取消或只在工装模式使用。

### 5.3 低频时钟配置

推荐硬件上放置 32.768 kHz 晶振，并在 `sdk_config.h` 中选择外部晶振作为 LFCLK 源。

示例：

```c
#define NRF_SDH_CLOCK_LF_SRC            1   // XTAL
#define NRF_SDH_CLOCK_LF_RC_CTIV        0
#define NRF_SDH_CLOCK_LF_RC_TEMP_CTIV   0
#define NRF_SDH_CLOCK_LF_ACCURACY       7   // 示例：20 ppm，实际按晶振规格配置
```

工程注意点：

1. 使用外部 32.768 kHz 晶振可以减少 RC 校准带来的额外唤醒；
2. `NRF_SDH_CLOCK_LF_ACCURACY` 应与晶振 ppm 匹配；
3. 如果为了省 BOM 不放 32 kHz 晶振，可以使用 RC，但要接受校准开销和睡眠时钟精度变差；
4. Beacon 广播不需要连接窗口同步，RC 也能工作，但超低功耗量产版本推荐 32 kHz 晶振。

---

## 6. `app_config.h` 推荐写法

所有产品级参数集中管理，避免散落在 `main.c` 和 `beacon_adv.c` 中。

```c
#ifndef APP_CONFIG_H__
#define APP_CONFIG_H__

#include <stdint.h>

#define APP_ENABLE_LOG                  0

#define APP_DEVICE_NAME                 "N52832_BCN"

// Nordic 示例 Company ID 是 0x0059。量产产品应申请/使用自己的 Bluetooth SIG Company Identifier。
#define APP_COMPANY_IDENTIFIER          0x0059

// 广播间隔，单位 ms。1000 ms 是低功耗与发现速度之间的常用平衡点。
#define APP_ADV_INTERVAL_MS             1000

// 0 表示持续广播，不自动停止。
#define APP_ADV_DURATION_SEC            0

// TX Power：根据实际距离需求调整。常用值：-20, -16, -12, -8, -4, 0, +4 dBm。
#define APP_TX_POWER_DBM                0

#define APP_BEACON_VERSION              0x01
#define APP_PRODUCT_TYPE                0x10

// 业务数据更新周期。比如每 60 秒采样一次电池并更新广播包。
#define APP_PAYLOAD_UPDATE_INTERVAL_MS  60000

#endif
```

---

## 7. 主程序代码：`main.c`

```c
#include <stdbool.h>
#include <stdint.h>

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_pwr_mgmt.h"
#include "app_timer.h"
#include "app_error.h"

#if APP_ENABLE_LOG
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#endif

#include "app_config.h"
#include "beacon_adv.h"
#include "beacon_power.h"
#include "beacon_battery.h"

#define APP_BLE_CONN_CFG_TAG            1
#define APP_BLE_OBSERVER_PRIO           3

APP_TIMER_DEF(m_payload_update_timer);

static uint32_t m_adv_counter = 0;

static void log_init(void)
{
#if APP_ENABLE_LOG
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
#endif
}

static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}

static void power_management_init(void)
{
    ret_code_t err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}

static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    (void)p_context;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_ADV_SET_TERMINATED:
            // 只有设置了有限广播 duration 时才会常见。
            // 本方案 APP_ADV_DURATION_SEC = 0，正常情况下不会因为超时停止。
            break;

        default:
            break;
    }
}

NRF_SDH_BLE_OBSERVER(m_ble_observer,
                     APP_BLE_OBSERVER_PRIO,
                     ble_evt_handler,
                     NULL);

static void ble_stack_init(void)
{
    ret_code_t err_code;
    uint32_t ram_start = 0;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);
}

static void payload_update_timeout_handler(void * p_context)
{
    (void)p_context;

    uint16_t battery_mv = beacon_battery_sample_mv();
    int8_t temperature_c = 25;   // 如果没有温度传感器，可固定或删除该字段。

    m_adv_counter++;

    beacon_adv_update_payload(battery_mv,
                              temperature_c,
                              m_adv_counter);
}

static void payload_update_timer_start(void)
{
    ret_code_t err_code;

    err_code = app_timer_create(&m_payload_update_timer,
                                APP_TIMER_MODE_REPEATED,
                                payload_update_timeout_handler);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_start(m_payload_update_timer,
                               APP_TIMER_TICKS(APP_PAYLOAD_UPDATE_INTERVAL_MS),
                               NULL);
    APP_ERROR_CHECK(err_code);
}

int main(void)
{
    log_init();
    timers_init();
    power_management_init();

    beacon_power_board_init();

    ble_stack_init();

    // 如果选择在 SoftDevice 启动后开启 DCDC，用 sd_power_dcdc_mode_set()。
    beacon_power_dcdc_enable_after_softdevice();

    beacon_adv_init();
    beacon_adv_start();

    payload_update_timer_start();

    for (;;)
    {
#if APP_ENABLE_LOG
        if (NRF_LOG_PROCESS() == false)
#endif
        {
            nrf_pwr_mgmt_run();
        }
    }
}
```

---

## 8. `main.c` 代码重点解释

### 8.1 初始化顺序

推荐顺序如下：

```text
log_init()
timers_init()
power_management_init()
beacon_power_board_init()
ble_stack_init()
beacon_power_dcdc_enable_after_softdevice()
beacon_adv_init()
beacon_adv_start()
payload_update_timer_start()
while nrf_pwr_mgmt_run()
```

这样安排的原因：

1. `app_timer_init()` 必须早于任何 `app_timer_create()`；
2. `nrf_pwr_mgmt_init()` 必须早于主循环 `nrf_pwr_mgmt_run()`；
3. GPIO、LED、外设等板级低功耗配置尽量在 BLE 启动前完成；
4. SoftDevice 启动后，BLE API 才能调用；
5. 广播数据必须先 encode，再配置 advertising set；
6. 主循环不做轮询，只进入低功耗。

### 8.2 `ram_start` 的意义

```c
uint32_t ram_start = 0;
nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
nrf_sdh_ble_enable(&ram_start);
```

SoftDevice 会占用一部分 RAM。`ram_start` 是 SoftDevice 根据 BLE 配置计算出来的 Application RAM 起始地址。

如果 linker script 里的 RAM ORIGIN 配置错误，常见现象是：

- `nrf_sdh_ble_enable()` 返回错误；
- Debug 能跑，Release 跑不起来；
- 改了 `sdk_config.h` 后 RAM 不匹配；
- 增加 BLE 功能后启动失败。

单广播设备不需要 GATT 服务和连接，因此 RAM 占用应保持较小。量产工程中要固定 SDK、SoftDevice 和 linker script，不要在不同工程配置间混用。

### 8.3 BLE 事件处理为什么这么少？

本方案不开连接、不扫、不配对、不做 GATT，所以 BLE 事件几乎不需要处理。保留 `BLE_GAP_EVT_ADV_SET_TERMINATED` 是为了支持未来设置有限广播时间。

如果 `APP_ADV_DURATION_SEC = 0`，广播不会因为 duration 到期而自动停止。

### 8.4 为什么使用 `app_timer` 更新 payload？

`app_timer` 底层使用 RTC，可以在 System ON Sleep 下运行。它适合：

- 每 10 秒更新计数器；
- 每 60 秒采样电池；
- 每 5 分钟采样温湿度；
- 定时切换广播内容。

不要用 `nrf_delay_ms()` 做周期任务，因为 delay 是 CPU busy-wait，会显著增加功耗。

---

## 9. 广播模块接口：`beacon_adv.h`

```c
#ifndef BEACON_ADV_H__
#define BEACON_ADV_H__

#include <stdint.h>

void beacon_adv_init(void);
void beacon_adv_start(void);
void beacon_adv_stop(void);

void beacon_adv_update_payload(uint16_t battery_mv,
                               int8_t temperature_c,
                               uint32_t counter);

#endif
```

---

## 10. 广播模块实现：`beacon_adv.c`

```c
#include <stdint.h>
#include <string.h>

#include "app_error.h"
#include "app_util.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_advdata.h"

#include "app_config.h"
#include "beacon_adv.h"

#define APP_BLE_CONN_CFG_TAG      1

static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;

static uint8_t m_adv_raw_buffer[BLE_GAP_ADV_SET_DATA_SIZE_MAX];

static ble_gap_adv_data_t m_adv_data =
{
    .adv_data =
    {
        .p_data = m_adv_raw_buffer,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
    },
    .scan_rsp_data =
    {
        .p_data = NULL,
        .len    = 0
    }
};

typedef struct
{
    uint8_t  version;
    uint8_t  product_type;
    uint16_t battery_mv;
    int8_t   temperature_c;
    uint32_t counter;
} __attribute__((packed)) beacon_payload_t;

static beacon_payload_t m_payload =
{
    .version       = APP_BEACON_VERSION,
    .product_type  = APP_PRODUCT_TYPE,
    .battery_mv    = 3000,
    .temperature_c = 25,
    .counter       = 0
};

static void adv_data_encode(void)
{
    ret_code_t err_code;

    ble_advdata_manuf_data_t manuf_data;
    memset(&manuf_data, 0, sizeof(manuf_data));

    manuf_data.company_identifier = APP_COMPANY_IDENTIFIER;
    manuf_data.data.p_data        = (uint8_t *)&m_payload;
    manuf_data.data.size          = sizeof(m_payload);

    ble_advdata_t advdata;
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type             = BLE_ADVDATA_SHORT_NAME;
    advdata.short_name_len        = 8;
    advdata.flags                 = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
    advdata.p_manuf_specific_data = &manuf_data;

    // ble_advdata_encode() 的 len 是输入/输出参数。
    // 每次编码前必须恢复为 buffer 最大长度。
    m_adv_data.adv_data.len = BLE_GAP_ADV_SET_DATA_SIZE_MAX;

    err_code = ble_advdata_encode(&advdata,
                                  m_adv_data.adv_data.p_data,
                                  &m_adv_data.adv_data.len);
    APP_ERROR_CHECK(err_code);
}

void beacon_adv_init(void)
{
    ret_code_t err_code;

    adv_data_encode();

    ble_gap_adv_params_t adv_params;
    memset(&adv_params, 0, sizeof(adv_params));

    adv_params.properties.type =
        BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;

    adv_params.p_peer_addr   = NULL;
    adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
    adv_params.interval      = MSEC_TO_UNITS(APP_ADV_INTERVAL_MS, UNIT_0_625_MS);
    adv_params.duration      = APP_ADV_DURATION_SEC * 100;  // SoftDevice 使用 10 ms 单位

    err_code = sd_ble_gap_adv_set_configure(&m_adv_handle,
                                            &m_adv_data,
                                            &adv_params);
    APP_ERROR_CHECK(err_code);
}

void beacon_adv_start(void)
{
    ret_code_t err_code;

    err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV,
                                       m_adv_handle,
                                       APP_TX_POWER_DBM);
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
    APP_ERROR_CHECK(err_code);
}

void beacon_adv_stop(void)
{
    ret_code_t err_code;

    err_code = sd_ble_gap_adv_stop(m_adv_handle);

    if (err_code != NRF_ERROR_INVALID_STATE)
    {
        APP_ERROR_CHECK(err_code);
    }
}

void beacon_adv_update_payload(uint16_t battery_mv,
                               int8_t temperature_c,
                               uint32_t counter)
{
    ret_code_t err_code;

    m_payload.battery_mv    = battery_mv;
    m_payload.temperature_c = temperature_c;
    m_payload.counter       = counter;

    adv_data_encode();

    // adv_params 传 NULL 表示只更新 advertising data，不重新配置广播参数。
    err_code = sd_ble_gap_adv_set_configure(&m_adv_handle,
                                            &m_adv_data,
                                            NULL);
    APP_ERROR_CHECK(err_code);
}
```

---

## 11. 广播代码重点解释

### 11.1 为什么选择不可连接、不可扫描广播？

```c
BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED
```

它表示：

- 不允许中心设备连接；
- 不响应 scan request；
- 不指定目标设备；
- 只做单向广播。

这正是低功耗 Beacon 的最小 BLE 行为。它避免了连接事件、GATT 服务、扫描响应和手机误连接。

### 11.2 为什么 `scan_rsp_data` 为 NULL？

```c
.scan_rsp_data =
{
    .p_data = NULL,
    .len    = 0
}
```

因为广播类型是 non-scannable，不会回复扫描响应。保留 scan response 只会增加复杂度，不符合本方案目标。

### 11.3 为什么广播 buffer 必须是静态变量？

```c
static uint8_t m_adv_raw_buffer[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
```

广播数据会被 SoftDevice 使用。不要把 advertising buffer 定义成函数局部数组，否则函数退出后栈内存失效，可能导致 HardFault 或广播内容异常。

错误示例：

```c
void bad_adv_init(void)
{
    uint8_t adv_buf[31]; // 错误：函数退出后内存生命周期结束
}
```

推荐做法：使用模块级 `static` buffer。

### 11.4 Manufacturer Specific Data 的结构

```c
typedef struct
{
    uint8_t  version;
    uint8_t  product_type;
    uint16_t battery_mv;
    int8_t   temperature_c;
    uint32_t counter;
} __attribute__((packed)) beacon_payload_t;
```

建议 Manufacturer Specific Data 放这些字段：

| 字段 | 长度 | 说明 |
|---|---:|---|
| `version` | 1 B | 广播协议版本 |
| `product_type` | 1 B | 产品类型 |
| `battery_mv` | 2 B | 电池电压，单位 mV |
| `temperature_c` | 1 B | 温度，单位 °C，整数 |
| `counter` | 4 B | 递增计数器，用于网关判断设备是否存活 |

不要直接放 `float`，建议用整数缩放。例如温度 25.34°C 可表示成 `int16_t temp_x100 = 2534`。

### 11.5 为什么需要 `__attribute__((packed))`？

C 结构体默认可能插入 padding 字节。广播协议是字节协议，必须固定字段位置。

推荐：

1. 所有广播 payload 结构体都加 `packed`；
2. 字段使用固定长度类型，例如 `uint8_t`、`uint16_t`、`uint32_t`；
3. 网关解析时按小端处理；
4. payload 协议版本必须保留；
5. 后续新增字段只能追加，不要随意改变已有字段顺序。

### 11.6 为什么每次 encode 前要重置长度？

```c
m_adv_data.adv_data.len = BLE_GAP_ADV_SET_DATA_SIZE_MAX;
```

`ble_advdata_encode()` 的长度参数是输入/输出参数：

- 输入：buffer 最大容量；
- 输出：实际编码后的长度。

如果第一次编码后长度变成 22，第二次编码前不恢复最大长度，SDK 会认为 buffer 只有 22 字节。当后续 payload 变长时可能返回 `NRF_ERROR_DATA_SIZE`。

### 11.7 广播间隔单位

```c
adv_params.interval = MSEC_TO_UNITS(APP_ADV_INTERVAL_MS, UNIT_0_625_MS);
```

BLE advertising interval 的底层单位是 0.625 ms，因此：

```text
100 ms  = 160 units
1000 ms = 1600 units
5000 ms = 8000 units
```

不要直接写：

```c
adv_params.interval = 1000; // 这不是 1000 ms，而是 625 ms
```

### 11.8 广播 duration 单位

```c
adv_params.duration = APP_ADV_DURATION_SEC * 100;
```

SoftDevice 的 advertising duration 单位通常按 10 ms 计。因此：

```text
1 s = 100 units
10 s = 1000 units
0 = 持续广播，不自动停止
```

本方案推荐 `0`，即持续低频广播。

### 11.9 TX Power 设置位置

```c
sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV,
                        m_adv_handle,
                        APP_TX_POWER_DBM);
```

TX Power 必须和 advertising handle 绑定。推荐在 `sd_ble_gap_adv_set_configure()` 之后、`sd_ble_gap_adv_start()` 之前设置。

量产调试建议：

| 场景 | TX Power 建议 |
|---|---:|
| 近距离货架标签 | -20 ~ -12 dBm |
| 房间级 Beacon | -8 ~ -4 dBm |
| 普通室内资产标签 | -4 ~ 0 dBm |
| 远距离或穿墙 | 0 ~ +4 dBm |

功耗和距离不是线性关系，最终要用 PPK2 + 实际网关/手机接收 RSSI 测试确定。

---

## 12. 电源模块：`beacon_power.h`

```c
#ifndef BEACON_POWER_H__
#define BEACON_POWER_H__

void beacon_power_board_init(void);
void beacon_power_dcdc_enable_after_softdevice(void);
void beacon_power_enter_system_off(void);

#endif
```

---

## 13. 电源模块：`beacon_power.c`

```c
#include <stdint.h>
#include <stdbool.h>

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_power.h"
#include "nrf_sdh.h"
#include "app_error.h"

#include "beacon_power.h"

static void unused_gpio_config(void)
{
    // 不要盲目配置所有 GPIO，避免影响 SWD、32k 晶振、NFC、外部传感器中断等功能。
    // 以下只是示例，量产时请按原理图列出 UNUSED_GPIO_LIST。

    static const uint8_t unused_pins[] =
    {
        2, 3, 4, 5, 6, 7, 8, 9,
        10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25,
        26, 27, 28, 29, 30, 31
    };

    for (uint32_t i = 0; i < sizeof(unused_pins); i++)
    {
        nrf_gpio_cfg_default(unused_pins[i]);
    }
}

void beacon_power_board_init(void)
{
    // 如果硬件放置了 DCDC 外部电感，可在 SoftDevice 启动前直接打开 DCDC。
    NRF_POWER->DCDCEN = 1;

    unused_gpio_config();

    // 量产板必须确认：
    // 1. LED 默认关闭；
    // 2. 外部传感器电源默认关闭或进入 shutdown；
    // 3. I2C/SPI 外设无上拉漏电；
    // 4. SWD 调试器断开后不会形成额外漏电；
    // 5. NFC 引脚如果不用，按 Nordic 推荐方式配置为 GPIO 或保留。
}

void beacon_power_dcdc_enable_after_softdevice(void)
{
    if (nrf_sdh_is_enabled())
    {
        ret_code_t err_code = sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
        APP_ERROR_CHECK(err_code);
    }
    else
    {
        NRF_POWER->DCDCEN = 1;
    }
}

void beacon_power_enter_system_off(void)
{
    // 运输模式/仓储模式可进入 System OFF。
    // 唤醒源可配置为按键、GPIO、NFC FieldDetect 或 Reset。
    NRF_POWER->SYSTEMOFF = 1;

    while (true)
    {
        // 等待进入 System OFF
    }
}
```

---

## 14. 电源代码重点解释

### 14.1 DCDC 开启条件

```c
NRF_POWER->DCDCEN = 1;
```

前提：硬件必须按照 Nordic 参考设计放置 DCDC 所需外围器件，尤其是 DCDC 电感。没有外部 DCDC 器件时不要强行开启。

建议：

- SoftDevice 启动前可以直接写寄存器；
- SoftDevice 启动后推荐用 `sd_power_dcdc_mode_set()`；
- Debug 和 Release 都要确认 DCDC 打开；
- 用 PPK2 对比 DCDC ON/OFF 的广播峰值和平均电流。

### 14.2 GPIO 不要“一刀切”

很多低功耗问题来自 GPIO：

- 外部上拉/下拉和 MCU 内部输出态冲突；
- 传感器未供电但 I2C 线上拉导致反灌电；
- LED 默认亮；
- 调试串口 TX/RX 接外部芯片；
- SPI CS 默认电平错误导致外设未睡眠；
- SWD 调试器连接时测功耗。

量产项目应在原理图上维护一张 `GPIO Low Power Table`：

| Pin | 功能 | 上电默认态 | Sleep 态 | 备注 |
|---|---|---|---|---|
| P0.02 | 未用 | input disconnect | default | 不接外部网络 |
| P0.05 | LED | output high/off | output high/off | 量产关闭 |
| P0.13 | SENSOR_EN | output low | output low | 关闭传感器电源 |
| P0.26 | I2C_SDA | input pull-up | input disconnect/按外设要求 | 防反灌 |

### 14.3 System ON Sleep vs System OFF

| 模式 | RAM 保持 | RTC 运行 | BLE 广播 | 唤醒方式 | 适用场景 |
|---|---|---|---|---|---|
| System ON Sleep | 是 | 是 | 是 | RTC/Radio/GPIO | 正常广播工作 |
| System OFF | 否/有限保持 | 否 | 否 | Reset/GPIO/NFC | 运输、仓储、长按关机 |

Beacon 正常运行时使用 System ON Sleep，因为 SoftDevice 需要定时唤醒 Radio 广播。运输模式才使用 System OFF。

---

## 15. 电池采样模块：`beacon_battery.c`

如果要广播电池电压，SAADC 不能常开。推荐采样时初始化，采样后立即反初始化。

```c
#include <stdint.h>

#include "nrf_drv_saadc.h"
#include "app_error.h"

#include "beacon_battery.h"

#define ADC_REF_MV                  600
#define ADC_RESOLUTION              4096
#define ADC_GAIN_DEN                1
#define ADC_GAIN_NUM                1

static nrf_saadc_value_t m_sample;

uint16_t beacon_battery_sample_mv(void)
{
    ret_code_t err_code;

    nrf_drv_saadc_config_t saadc_config = NRF_DRV_SAADC_DEFAULT_CONFIG;
    err_code = nrf_drv_saadc_init(&saadc_config, NULL);
    APP_ERROR_CHECK(err_code);

    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_VDD);

    err_code = nrf_drv_saadc_channel_init(0, &channel_config);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_sample_convert(0, &m_sample);
    APP_ERROR_CHECK(err_code);

    nrf_drv_saadc_channel_uninit(0);
    nrf_drv_saadc_uninit();

    // 简化计算。实际项目应根据 SAADC gain/reference/resolution 校准。
    // 对 VDD 输入，Nordic 内部路径通常带比例，必须按 SDK/芯片规格校准。
    uint32_t mv = ((uint32_t)m_sample * 3600UL) / ADC_RESOLUTION;

    if (mv > 65535)
    {
        mv = 65535;
    }

    return (uint16_t)mv;
}
```

注意：上面电压换算是示例框架。量产时必须用万用表/电源源表对 3.0 V、2.8 V、2.5 V、2.0 V 等点做校准，确认 SAADC 输入路径、增益、参考电压和分压关系。

---

## 16. 广播 Payload 设计建议

### 16.1 推荐 Payload

```text
Manufacturer Specific Data
├── Company ID: 2 bytes
└── Payload
    ├── protocol_version: 1 byte
    ├── product_type:     1 byte
    ├── battery_mv:       2 bytes
    ├── temperature_c:    1 byte
    ├── counter:          4 bytes
    └── crc8:             1 byte，可选
```

### 16.2 为什么要有 `counter`？

网关只看 RSSI 和设备 ID，无法区分：

- 设备确实在广播；
- 网关缓存了旧数据；
- 设备死机但网关显示最后一次数据；
- 广播包被重复上报。

加入递增 counter 后，服务器可以判断设备是否真的在更新。

### 16.3 是否需要加密？

如果广播内容包含敏感信息，例如门锁状态、资产位置、人员身份，不应明文广播。可以采用：

- 滚动设备 ID；
- AES-CTR / AES-CCM 应用层加密；
- 时间片 token；
- payload 中加入 nonce/counter；
- 网关侧解密。

单广播方案没有连接层加密，所有数据默认可被附近设备抓包。

---

## 17. 功耗优化策略

### 17.1 软件层

| 优化项 | 建议 |
|---|---|
| 广播类型 | Non-connectable + non-scannable |
| 广播间隔 | 默认 1000 ms，低功耗可 3000~10000 ms |
| TX Power | 从 -8 dBm 开始实测，不够再提高 |
| Payload 长度 | 越短越好，避免塞满 31 B |
| Scan Response | 禁用 |
| GATT/连接 | 禁用 |
| Log | 量产关闭 |
| Timer | 用 `app_timer`，不要 busy-wait |
| SAADC | 采样后立即 uninit |
| 外设 | 默认关闭，仅按需短时开启 |

### 17.2 硬件层

| 优化项 | 建议 |
|---|---|
| 电源 | CR2032/CR2450 直供 nRF52832，注意内阻和低温能力 |
| DCDC | 放置 Nordic 推荐外围并开启 |
| 32 MHz 晶振 | 按参考设计匹配负载电容 |
| 32.768 kHz 晶振 | 推荐保留，减少 RC 校准开销 |
| LED | 量产不焊或默认关闭 |
| 传感器 | 使用 load switch 或 EN 管脚彻底关闭 |
| I2C 上拉 | 评估漏电，必要时由 MCU 控制上拉电源 |
| 天线 | 预留 π 型匹配网络 |
| 测试点 | SWD、VDD 电流测试点、RF 测试点 |

---

## 18. 功耗估算模型

### 18.1 平均电流公式

```text
I_avg = I_sleep + E_adv / T_adv + E_sensor / T_sensor + I_leak
```

其中：

- `I_sleep`：System ON Sleep 底电流；
- `E_adv`：一次广播事件消耗的电荷；
- `T_adv`：广播间隔；
- `E_sensor`：一次传感器采样消耗的电荷；
- `T_sensor`：传感器采样周期；
- `I_leak`：外设、上拉、电池保护、ESD、分压电阻等漏电。

工程上不要只相信公式，必须用 PPK2 测量：

1. 先测空板睡眠底电流；
2. 再测 BLE 广播峰值和平均值；
3. 再打开传感器采样；
4. 最后测完整业务固件。

### 18.2 粗略寿命估算

```text
Life_hours = Battery_capacity_mAh × derating / I_avg_mA
Life_years = Life_hours / 24 / 365
```

`derating` 建议：

| 电池 | 理论容量 | 建议 derating |
|---|---:|---:|
| CR2032 | 220 mAh | 0.6~0.75 |
| CR2450 | 600~620 mAh | 0.7~0.85 |

原因：纽扣电池容量受脉冲电流、温度、截止电压、自放电、内阻和储存时间影响很大。

### 18.3 参考估算表

以下是工程估算值，实际以 PPK2 和真实电池测试为准。

| 广播间隔 | TX Power | 估算平均电流 | CR2032 理论寿命 | CR2450 理论寿命 | 适用场景 |
|---:|---:|---:|---:|---:|---|
| 100 ms | 0 dBm | 40~60 µA | 0.3~0.5 年 | 0.9~1.4 年 | 快速发现、演示、定位密集部署 |
| 500 ms | -4 dBm | 12~20 µA | 0.9~1.5 年 | 2.5~4.5 年 | 普通 Beacon |
| 1000 ms | -8~0 dBm | 5~10 µA | 1.8~3.5 年 | 5~10 年理论值 | 资产标签、状态广播 |
| 3000 ms | -8~-4 dBm | 3~6 µA | 3~6 年理论值 | 7 年以上理论值 | 超低功耗标签 |
| 5000 ms | -12~-8 dBm | 2~4 µA | 4 年以上理论值 | 受电池自放电影响 | 低频存在检测 |

注意：当估算寿命超过 5 年时，电池自放电、密封、温度、漏电、物料批次会成为主导因素，不能只按容量除以电流计算。

---

## 19. PPK2 功耗测试流程

### 19.1 硬件连接

推荐两种方式：

1. **Source Meter 模式**：PPK2 给目标板供电并测量电流；
2. **Ampere Meter 模式**：目标板由电池/外部电源供电，PPK2 串联测电流。

对纽扣电池产品，建议：

- 先用 PPK2 Source Meter 模式设定 3.0 V；
- 再用真实 CR2032/CR2450 做长期验证；
- 最后在低温、高温下复测广播稳定性。

### 19.2 测试用固件版本

至少准备 4 个固件：

| 固件 | 用途 |
|---|---|
| `sleep_only.hex` | 只初始化电源和 GPIO，测底电流 |
| `adv_1000ms.hex` | 只广播，测 BLE 平均电流 |
| `adv_sensor.hex` | 加传感器采样，测业务额外消耗 |
| `production.hex` | 量产固件，最终验收 |

### 19.3 PPK2 波形判断

正常单广播波形应表现为：

```text
低电流睡眠基线
   ↓
周期性 RADIO 峰值
   ↓
快速回到底电流
   ↓
无持续平台电流
```

异常波形：

| 现象 | 可能原因 |
|---|---|
| 睡眠电流几十/几百 µA | UART/LOG 未关、外设未关、GPIO 漏电 |
| 广播间隔之间有周期性毛刺 | app_timer 任务太频繁、RC 校准、传感器轮询 |
| RADIO 峰值后长时间不回落 | CPU 没睡、日志输出、错误处理循环 |
| 插上调试器电流正常，拔掉异常 | GPIO 浮空、复位脚、SWD 引脚状态问题 |

---

## 20. 常见 SDK 坑点

### 20.1 `APP_ERROR_CHECK()` 在量产中的处理

SDK 默认错误处理可能进入死循环并保持较高电流。量产建议：

- Debug 版本保留断言和日志；
- Release 版本错误后记录 reset reason，然后软复位；
- 避免设备卡死在高功耗错误循环。

### 20.2 `NRF_LOG_PROCESS()`

主循环常见写法：

```c
if (NRF_LOG_PROCESS() == false)
{
    nrf_pwr_mgmt_run();
}
```

量产关闭 log 后，主循环应直接进入 `nrf_pwr_mgmt_run()`。不要保留 UART log backend。

### 20.3 Linker RAM 配置

修改 `sdk_config.h` 后可能改变 SoftDevice RAM 需求。若启动 BLE 失败，应检查编译输出或 Debug log 中提示的 RAM 起始地址。

### 20.4 Debugger 对功耗的影响

测功耗时：

- 断开 J-Link；
- 关闭 RTT Viewer；
- 使用量产 hex；
- 关闭 debug 编译宏；
- 不要让 DK 板载 LED、接口 MCU 影响目标电流。

### 20.5 不要用 DK 板直接代表量产功耗

nRF52 DK 有板载调试器、LED、接口芯片、电平转换和其他外围，不能直接代表最终产品功耗。可以用 DK 验证功能，但功耗验收必须用接近量产硬件的样机。

---

## 21. 量产版本推荐参数

### 21.1 默认推荐

| 参数 | 推荐值 |
|---|---|
| Advertising Type | Non-connectable, non-scannable, undirected |
| Advertising Interval | 1000 ms |
| TX Power | -8 dBm 起测，不够再调到 -4/0 dBm |
| Payload | Manufacturer Specific Data，10~16 B |
| Scan Response | Disabled |
| DCDC | Enabled，前提是硬件支持 |
| LFCLK | 32.768 kHz XTAL |
| Sleep | System ON Sleep |
| Payload Update | 60 s 或更长 |
| Battery Sample | 5~30 min 一次，视产品需要 |
| Log | Disabled |
| LED | Disabled / DNP |

### 21.2 高实时性版本

| 参数 | 推荐值 |
|---|---|
| ADV Interval | 100~300 ms |
| TX Power | 0 dBm |
| 电池 | 不建议 CR2032 长寿命设计 |
| 场景 | 室内定位、快速发现、交互提示 |

### 21.3 长寿命版本

| 参数 | 推荐值 |
|---|---|
| ADV Interval | 3000~10000 ms |
| TX Power | -12~-8 dBm |
| Payload Update | 5~30 min |
| 电池 | CR2450 或更大容量 |
| 场景 | 存在检测、低频资产状态广播 |

---

## 22. 生产测试建议

### 22.1 工装测试项目

| 项目 | 方法 |
|---|---|
| Flash 烧录 | SoftDevice + Application + UICR/配置区 |
| 设备地址 | 读取 FICR DEVICEADDR 或写入自定义 ID |
| 广播内容 | 工装网关扫描并校验 payload |
| RSSI | 固定距离下检查 RSSI 窗口 |
| 电流 | 抽检 PPK2 或夹具电流 |
| 按键/唤醒 | 检查 System OFF 唤醒 |
| 电池电压 | 工装供电多点校准 |

### 22.2 建议烧录顺序

```text
1. 全片擦除
2. 烧录 S132 SoftDevice
3. 烧录 Application
4. 写入产品配置页/序列号/密钥
5. Verify
6. Reset
7. 工装扫描广播
8. 抽检功耗
```

### 22.3 版本信息建议

广播 payload 或 Flash 配置区应包含：

- Hardware version；
- Firmware version；
- Protocol version；
- Product type；
- Production batch；
- Device unique ID。

---

## 23. 可直接使用的最小代码清单

最小版本只需要：

```text
main.c
app_config.h
beacon_adv.c
beacon_adv.h
beacon_power.c
beacon_power.h
sdk_config.h
```

如果没有电池采样，可以删除 `beacon_battery.c/h`，并在 payload 里固定电压或删除 `battery_mv` 字段。

---

## 24. 代码审查 Checklist

### 24.1 BLE 部分

- [ ] 是否只使用 non-connectable non-scannable advertising？
- [ ] 是否未初始化 GATT、Peer Manager、Connection Params？
- [ ] 是否关闭 Scan Response？
- [ ] 广播间隔是否符合寿命目标？
- [ ] TX Power 是否经过实测？
- [ ] Payload 是否小于 Legacy Advertising 限制？
- [ ] Manufacturer Specific Data 是否有协议版本？

### 24.2 低功耗部分

- [ ] `NRF_LOG_ENABLED = 0`？
- [ ] UART/RTT/UARTE 是否关闭？
- [ ] LED 是否关闭或不焊？
- [ ] SAADC 是否采样后 uninit？
- [ ] I2C/SPI 传感器是否睡眠？
- [ ] 未用 GPIO 是否配置为低漏电状态？
- [ ] DCDC 是否在硬件支持下开启？
- [ ] 是否断开 J-Link 后测功耗？

### 24.3 量产部分

- [ ] SoftDevice/Application/linker script 是否版本锁定？
- [ ] 是否有唯一设备 ID？
- [ ] 是否有协议版本？
- [ ] 是否有工装扫描验证？
- [ ] 是否有 PPK2 抽检标准？
- [ ] 是否有低温电池验证？
- [ ] 是否有 RF 匹配和天线验证？

---

## 25. 参考资料

1. Nordic Semiconductor，nRF52832 产品页：说明 nRF52832 是 64 MHz Arm Cortex-M4F、支持 Bluetooth LE、2.4 GHz transceiver、+4 dBm TX power，并列出 Flash/RAM 和供电范围等关键特性。  
   URL: https://www.nordicsemi.com/Products/nRF52832

2. Nordic Semiconductor，官方文档入口：包含 nRF5 SDK 17.1.0、nRF52 系列、nRF52832 Product Specification、nRF52832 DK Hardware、Power Profiler Kit II 等文档入口。  
   URL: https://docs.nordicsemi.com/

3. Nordic Semiconductor，nRF5 SDK 17.1.0 文档入口。  
   URL: https://docs.nordicsemi.com/r/bundle/nrf5_SDK_v17.1.0/page/index.html

4. Nordic Semiconductor，nRF52832 Product Specification 文档入口。  
   URL: https://docs.nordicsemi.com/r/bundle/additionalresources/page/additionalresources/nrf52-series/nrf52832/resources/product-specification

5. Nordic Semiconductor，Power Profiler Kit II 文档入口。  
   URL: https://docs.nordicsemi.com/r/bundle/additionalresources/page/additionalresources/nrf91-series/nrf9151/guidelines/power-profiler-kit-ii

6. Silvano Cortesi, Marc Dreher, Michele Magno, “Design and Implementation of an RSSI-Based Bluetooth Low Energy Indoor Localization System”, 2023。该论文实现了基于 nRF52832 的固定 BLE beacon，100 ms 周期广播时整体功耗约 50 µA@3 V，并估算 500 mAh 纽扣电池可工作超过一年。  
   URL: https://arxiv.org/abs/2310.14704

---

## 26. 最终工程建议

如果目标是 **CR2450 供电、室内 10~30 m、寿命 3 年以上**，建议采用以下默认配置：

```text
nRF52832 + S132 + nRF5 SDK 17.1.0
Legacy Advertising
Non-connectable Non-scannable Undirected
ADV Interval = 1000~3000 ms
TX Power = -8 dBm 起测
DCDC = ON
LFCLK = 32.768 kHz XTAL
Payload = Manufacturer Specific Data，控制在 10~16 B
Payload Update = 60 s 或更长
Battery Sample = 5~30 min
System ON Sleep
Log/UART/LED/无关外设全部关闭
PPK2 实测作为最终依据
```

如果目标是 **CR2032 供电且希望超过 2 年**，不建议使用 100 ms 广播间隔，应优先选择：

```text
ADV Interval >= 1000 ms
TX Power <= -8 dBm
Payload 尽量短
传感器采样周期 >= 5 min
取消 LED
严控外部漏电 < 1 µA
```
