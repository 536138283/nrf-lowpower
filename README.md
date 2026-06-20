# nRF52832 Low-Power Timed BLE Advertiser

This repository contains a minimal Zephyr/Nordic Connect SDK application for an
nRF52832 that only performs timed, non-connectable Bluetooth LE advertising.
The radio is enabled for a short burst and is then stopped until the next cycle,
allowing the SoC to spend most of its time in idle low-power states.

## Behaviour

Default cycle:

- Advertise as `nRF52-LP-ADV` for 1 second.
- Use a 100 ms legacy advertising interval while advertising is enabled.
- Stop advertising for the remaining 59 seconds of the minute.
- Repeat forever without accepting connections.

The manufacturer payload is 10 bytes:

| Bytes | Field |
| --- | --- |
| 0..1 | Bluetooth SIG company identifier (`CONFIG_APP_COMPANY_ID`) |
| 2..3 | Product marker (`CONFIG_APP_PAYLOAD_MAGIC`, default `LP`) |
| 4..7 | Little-endian boot-local advertising sequence |
| 8..9 | Little-endian uptime seconds modulo 65536 |

Use your assigned Bluetooth SIG company identifier before shipping a product;
`0xFFFF` is only a development placeholder.

## Build and flash

Install the Nordic Connect SDK or an equivalent Zephyr workspace, then run:

```sh
west build -b nrf52dk_nrf52832 .
west flash
```

## Tuning for your product

The key settings live in `prj.conf` and can also be overridden at build time:

```sh
west build -b nrf52dk_nrf52832 . -- \
  -DCONFIG_APP_ADV_PERIOD_MS=300000 \
  -DCONFIG_APP_ADV_ON_TIME_MS=800 \
  -DCONFIG_APP_ADV_INTERVAL_MS=250 \
  -DCONFIG_APP_COMPANY_ID=0x1234
```

Lower average current usually comes from increasing `CONFIG_APP_ADV_PERIOD_MS`,
reducing `CONFIG_APP_ADV_ON_TIME_MS`, and increasing
`CONFIG_APP_ADV_INTERVAL_MS`. Discovery latency moves in the opposite direction,
so validate the final values with the scanner/phone that will receive the
advertisements.

## Low-power checklist

- Keep logging, UART, console, and assertions disabled for production builds.
- Enable the nRF52 DC/DC converter when the hardware layout supports it.
- Decide whether to use the internal RC LFCLK or an external 32.768 kHz crystal.
  The sample uses RC LFCLK to minimize BOM, but an external crystal normally
  improves timing accuracy and can reduce calibration overhead.
- Remove or isolate LEDs, pull-ups, sensors, and debugger leakage paths during
  current measurements.
- Measure on real hardware with a PPK2 or equivalent current profiler; BLE
  average current depends heavily on advertising interval, payload length,
  channel map, TX power, board leakage, and receiver latency requirements.
