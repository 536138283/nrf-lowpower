# 架构说明

## 目标

程序目标是让 nRF52832 周期性广播少量数据，并在非广播时间尽可能保持低功耗：

- 以 BLE Broadcaster 角色只使用 Advertising，不建立连接。
- 不初始化 GATT 服务，不维护连接参数。
- 广播窗口尽量短，窗口结束后调用 SoftDevice API 停止广播。
- 广播停止后保持 System ON idle，由 RTC/LFCLK 驱动的 `app_timer` 唤醒下一次广播。
- 主循环不 busy wait，只调用 `nrf_pwr_mgmt_run()` 进入 System ON sleep。

## 模块划分

| 模块 | 文件/函数 | 作用 |
| --- | --- | --- |
| GPIO 低功耗准备 | `low_power_gpio_prepare()` | 启动时将 P0.00-P0.31 配回默认状态，降低悬空或外设泄漏风险。 |
| 定时器 | `timers_init()` | 初始化 `app_timer`，创建广播开始和广播停止两个 single-shot timer。 |
| 电源管理 | `power_management_init()` | 初始化 `nrf_pwr_mgmt` 并启用 nRF52 DC/DC。 |
| BLE 协议栈 | `ble_stack_init()` | 启动 SoftDevice 并应用 `sdk_config.h` 中的 BLE 配置。 |
| GAP 名称 | `gap_params_init()` | 设置广播中使用的设备名。 |
| 广播数据 | `advertising_payload_update()` | 编码设备名、flags 和 manufacturer data，并配置不可连接/不可扫描 Legacy 广播、37/38/39 三通道和 TX power。 |
| 广播控制 | `advertising_start()` / `advertising_stop()` | 周期性启动/停止广播。 |

## 启动流程

```text
main()
 ├─ low_power_gpio_prepare()
 ├─ timers_init()
 ├─ power_management_init()
 ├─ ble_stack_init()
 ├─ gap_params_init()
 ├─ advertising_start()
 └─ for (;;) nrf_pwr_mgmt_run()
```

## 广播周期流程

```text
advertising_start()
 ├─ 更新 manufacturer data
 ├─ sd_ble_gap_adv_set_configure()
 ├─ sd_ble_gap_tx_power_set()
 ├─ sd_ble_gap_adv_start()
 └─ 启动 m_adv_stop_timer，延时 ADV_ON_TIME_MS

m_adv_stop_timer 到期
 └─ advertising_stop()
     ├─ sd_ble_gap_adv_stop()
     └─ 启动 m_adv_start_timer，延时 ADV_PERIOD_MS - ADV_ON_TIME_MS

m_adv_start_timer 到期
 └─ advertising_start()
```

## 为什么使用两个 single-shot timer

两个 single-shot timer 能让广播窗口和休眠窗口边界清晰：

- 广播启动后只安排一次停止事件。
- 广播停止后只安排下一次启动事件。
- 没有周期 timer 在广播期间重复触发，避免状态竞争。
- 后续如果要加入动态周期、按键唤醒或传感器采样，也更容易插入逻辑。
