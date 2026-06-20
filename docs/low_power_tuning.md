# 低功耗调优与测试

## 当前低功耗设计点

| 设计点 | 作用 |
| --- | --- |
| Broadcaster：不可连接、不可扫描 Legacy 广播 | 避免连接建立、扫描响应和 GATT 交互带来的额外功耗。 |
| 短广播窗口 | 只在需要被发现时打开 radio。 |
| 停止广播后睡眠 | 调用 `sd_ble_gap_adv_stop()` 后回到 `nrf_pwr_mgmt_run()`。 |
| `app_timer` 调度 | 使用 RTC/LFCLK 定时，避免 CPU busy wait，非广播时间保持 System ON idle。 |
| DCDC | 硬件支持时降低 radio 和 CPU 活动期电流。 |
| GPIO 默认态 | 减少开发板 LED、按键、外设和悬空引脚漏电。 |
| 关闭日志/串口 | 避免 UART/RTT/printf 让系统无法进入理想低功耗状态。 |

## 参数调优方向

关键参数位于 `src/main.c`，当前默认三通道广播、100 ms 间隔、+4 dBm TX power：

```c
#define ADV_INTERVAL_MS        100U
#define ADV_ON_TIME_MS         1000U
#define ADV_PERIOD_MS          60000U
#define APP_COMPANY_IDENTIFIER 0xFFFFU
#define APP_ADV_TX_POWER_DBM   4
```

### 降低平均电流

优先尝试：

1. 增大 `ADV_PERIOD_MS`，例如从 60 s 改成 300 s。
2. 减小 `ADV_ON_TIME_MS`，例如从 1000 ms 改成 300-800 ms。
3. 增大 `ADV_INTERVAL_MS`，例如从 100 ms 改成 250 ms 或 500 ms。
4. 缩短广播包长度，减少广播数据字段。
5. 如距离允许，降低 `APP_ADV_TX_POWER_DBM`。
6. 在自研硬件上确认没有 LED、USB-UART、传感器电源或上拉电阻漏电。

### 提高发现速度

如果手机或网关发现慢，可以反向调整：

1. 减小 `ADV_INTERVAL_MS`。
2. 增大 `ADV_ON_TIME_MS`。
3. 减小 `ADV_PERIOD_MS`。
4. 保持 37/38/39 三通道广播，不要屏蔽主广播信道。
5. 维持或提高 `APP_ADV_TX_POWER_DBM`，同时确认法规和电池寿命。
6. 根据接收端扫描窗口和扫描间隔做实测匹配。

## 测试方法

建议用 Nordic PPK2 或同等级电流分析仪：

1. 断开调试器或确认调试器不会给目标板引入额外电流路径。
2. 如果使用 nRF52 DK，隔离板载 debugger、LED 和 USB-UART 对电流的影响。
3. 至少测量多个完整广播周期，而不是只看 1 秒广播窗口。
4. 同时记录广播周期参数、供电电压、DCDC 是否启用、LFCLK 来源和接收端扫描参数。
5. 分别测量开发板和最终 PCB，因为开发板漏电通常明显高于产品板。

## 远距离能力说明

当前 nRF52832 方案使用 1M PHY，不支持 BLE Coded PHY。项目通过以下方式增强实际覆盖距离：

- Legacy advertising 三通道 37/38/39 全启用，增加被扫描命中的概率。
- 广播间隔固定 100 ms，在功耗和发现速度之间取得平衡。
- TX power 默认 +4 dBm。
- 接收端应配置足够长的 scan window，并尽量使用低噪声天线与合理安装位置。

如果项目明确要求 BLE 5 Long Range / Coded PHY，需要换用支持 Coded PHY 的 nRF52/nRF53 器件，而不是 nRF52832。

## RC LFCLK 与外部 32.768 kHz 晶振

当前配置使用 RC LFCLK。它减少 BOM，但需要校准。若产品对平均电流、时间精度或温漂更敏感，可以评估外部 32.768 kHz 晶振：

- 外部晶振通常时间精度更好。
- 外部晶振可能减少 RC 校准开销。
- RC 方案硬件更简单，但需要结合实际周期和温度范围测试平均电流。

## 生产检查清单

- [ ] 替换正式 Company ID。
- [ ] 确认 SoftDevice、链接脚本和 RAM 起始地址匹配。
- [ ] 确认 DCDC 外围电感布局满足 Nordic 硬件参考设计。
- [ ] 确认未使用 GPIO 均为低漏电状态。
- [ ] 确认量产固件关闭日志、串口和 assert。
- [ ] 用最终外壳、电池和 PCB 测量完整周期平均电流。
