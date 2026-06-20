# 广播数据格式

## 广播类型

程序工作在 BLE Broadcaster 模式，使用 Legacy Advertising：

- `BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED`
- 不可连接。
- 不可扫描。
- 非定向。
- 三条主广播信道 37/38/39 全部启用。
- 广播间隔 100 ms。
- 1 Mbps PHY。
- TX power 默认 +4 dBm，用于提高覆盖距离。

这种方式适合只上报状态、ID、计数器或传感器摘要的低功耗设备。nRF52832/S132 不支持 BLE Coded PHY，若需要标准 BLE Long Range coded PHY，应选用支持 Coded PHY 的芯片；本项目通过三通道广播、100 ms 间隔和较高 TX power 改善实际覆盖距离。

## 广播字段

广播包由 `ble_advdata_encode()` 编码，当前包含：

| 字段 | 说明 |
| --- | --- |
| Flags | `BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE` |
| Complete Local Name | `nRF52-LP-ADV` |
| Manufacturer Specific Data | 自定义 10 字节负载 |

## Manufacturer Specific Data

Manufacturer Specific Data 总长度为 10 字节：

| Byte | 内容 | 说明 |
| --- | --- | --- |
| 0 | Company ID LSB | Bluetooth SIG Company Identifier 低字节。 |
| 1 | Company ID MSB | Bluetooth SIG Company Identifier 高字节。 |
| 2 | Magic LSB | 默认 `0x50`。 |
| 3 | Magic MSB | 默认 `0x4C`，整体小端表示 `0x4C50`。 |
| 4 | Sequence byte 0 | 广播序号低字节。 |
| 5 | Sequence byte 1 | 广播序号。 |
| 6 | Sequence byte 2 | 广播序号。 |
| 7 | Sequence byte 3 | 广播序号高字节。 |
| 8 | Uptime seconds LSB | uptime 秒数低字节。 |
| 9 | Uptime seconds MSB | uptime 秒数高字节。 |

## 接收端解析建议

接收端建议按以下顺序过滤：

1. 先过滤 BLE address 或设备名。
2. 再过滤 Company ID。
3. 再过滤 Magic 字段。
4. 使用 Sequence 判断是否为新广播。
5. 使用 Uptime 辅助判断设备是否重启。

## 生产注意事项

`0xFFFF` 只是开发占位值，产品发布前必须替换为公司申请到的 Bluetooth SIG Company Identifier。若没有 Company Identifier，建议不要以正式产品名义发布使用 manufacturer data 的广播协议。
