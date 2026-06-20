# nRF52832 BLE Single Timed Advertiser Low-Power SDK App

This project is a Nordic **nRF5 SDK + S132 SoftDevice** application for an
nRF52832 that only performs timed, non-connectable BLE advertising. It is meant
for products that periodically broadcast a small payload and then immediately
return to System ON sleep.

> Note: the requested reference document `nrf52832_ble_single_adv_low_power_sdk.md`
> was not present in this checkout, so this implementation follows the nRF5 SDK
> low-power pattern implied by that filename: SoftDevice + `app_timer` +
> `nrf_pwr_mgmt_run()` + no UART/logging/services/connections.

## Runtime behaviour

Default cycle:

- Device name: `nRF52-LP-ADV`.
- Advertising type: legacy, non-connectable, non-scannable undirected.
- Advertising interval: 100 ms while the burst is active.
- Advertising on-time: 1 second.
- Sleep time: 59 seconds.
- Repeat forever without opening connections or GATT services.

The manufacturer data payload is 10 bytes:

| Bytes | Field |
| --- | --- |
| 0..1 | Bluetooth SIG company identifier, default `0xFFFF` development placeholder |
| 2..3 | Product marker, default little-endian `LP` |
| 4..7 | Little-endian boot-local advertising sequence |
| 8..9 | Little-endian uptime seconds modulo 65536 |

## Low-power design choices

- Uses `app_timer`/RTC to schedule start and stop events instead of busy waits.
- Calls `sd_ble_gap_adv_stop()` after every short burst to stop radio activity.
- Sleeps in the main loop with `nrf_pwr_mgmt_run()`.
- Enables the nRF52 DC/DC converter with `sd_power_dcdc_mode_set()`.
- Configures all GPIOs to the default disconnected state at boot.
- Disables RTT/UART logging and unused serial/peripheral drivers in `sdk_config.h`.
- Sets SoftDevice link counts to zero because this application never connects.

## Build

Install Nordic nRF5 SDK 17.1.0 and GNU Arm Embedded Toolchain, then build with:

```sh
make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560
```

Flash the SoftDevice and application:

```sh
make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560 erase
make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560 flash_softdevice
make SDK_ROOT=/path/to/nRF5_SDK_17.1.0_ddde560 flash
```

The linker script assumes S132 v7.x with application flash starting at
`0x26000`. If your SoftDevice version differs, update `nrf52832_xxaa.ld` using
Nordic's memory placement values for that SoftDevice.

## Tuning

Edit these constants in `src/main.c` for your product:

```c
#define ADV_INTERVAL_MS        100U
#define ADV_ON_TIME_MS         1000U
#define ADV_PERIOD_MS          60000U
#define APP_COMPANY_IDENTIFIER 0xFFFFU
```

Lower average current generally comes from increasing `ADV_PERIOD_MS`, reducing
`ADV_ON_TIME_MS`, and increasing `ADV_INTERVAL_MS`. Discovery latency moves in
the opposite direction, so validate final values with the real receiver.

## Measurement checklist

- Use your assigned Bluetooth SIG company identifier before production.
- Remove debugger, LEDs, pull-ups, sensor rails, and USB interface leakage when
  measuring current.
- Confirm the PCB supports DCDC mode before relying on the DCDC current numbers.
- Compare RC LFCLK vs a 32.768 kHz crystal on your hardware; crystal accuracy can
  reduce calibration overhead, while RC reduces BOM.
- Measure with Nordic PPK2 or an equivalent current profiler over multiple full
  advertising periods, not only during the 1-second radio burst.
