/*
 * Low-power timed BLE advertiser for nRF52832.
 *
 * The device never accepts connections. It enables legacy non-connectable
 * advertising for a short burst, then stops the controller until the next
 * scheduled burst so Zephyr can idle the CPU and SoC peripherals.
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#define ADV_INTERVAL_UNITS(ms) ((uint16_t)(((uint32_t)(ms) * 1000U) / 625U))
#define ADV_INTERVAL_MIN_UNITS ADV_INTERVAL_UNITS(CONFIG_APP_ADV_INTERVAL_MS)
#define ADV_INTERVAL_MAX_UNITS ADV_INTERVAL_MIN_UNITS
#define COMPANY_ID ((uint16_t)CONFIG_APP_COMPANY_ID)
#define PAYLOAD_MAGIC ((uint16_t)CONFIG_APP_PAYLOAD_MAGIC)

BUILD_ASSERT(CONFIG_APP_ADV_ON_TIME_MS > 0, "advertising on-time must be positive");
BUILD_ASSERT(CONFIG_APP_ADV_PERIOD_MS > CONFIG_APP_ADV_ON_TIME_MS,
	     "advertising period must be longer than on-time");
BUILD_ASSERT(ADV_INTERVAL_MIN_UNITS >= 0x0020 && ADV_INTERVAL_MIN_UNITS <= 0x4000,
	     "legacy advertising interval must be 20 ms to 10.24 s");

static uint8_t manufacturer_data[10];
static uint32_t sequence;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, manufacturer_data, sizeof(manufacturer_data)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		(sizeof(CONFIG_BT_DEVICE_NAME) - 1)),
};

static const struct bt_le_adv_param adv_param = {
	.id = BT_ID_DEFAULT,
	.sid = 0,
	.secondary_max_skip = 0,
	.options = BT_LE_ADV_OPT_USE_IDENTITY,
	.interval_min = ADV_INTERVAL_MIN_UNITS,
	.interval_max = ADV_INTERVAL_MAX_UNITS,
	.peer = NULL,
};

static void start_advertising(struct k_work *work);
static void stop_advertising(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(start_work, start_advertising);
static K_WORK_DELAYABLE_DEFINE(stop_work, stop_advertising);

static void encode_payload(void)
{
	const int64_t uptime_s = k_uptime_get() / MSEC_PER_SEC;

	sys_put_le16(COMPANY_ID, &manufacturer_data[0]);
	sys_put_le16(PAYLOAD_MAGIC, &manufacturer_data[2]);
	sys_put_le32(sequence++, &manufacturer_data[4]);
	sys_put_le16((uint16_t)uptime_s, &manufacturer_data[8]);
}

static void start_advertising(struct k_work *work)
{
	ARG_UNUSED(work);

	encode_payload();
	(void)bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	(void)k_work_schedule(&stop_work, K_MSEC(CONFIG_APP_ADV_ON_TIME_MS));
}

static void stop_advertising(struct k_work *work)
{
	ARG_UNUSED(work);

	(void)bt_le_adv_stop();
	(void)k_work_schedule(&start_work,
				K_MSEC(CONFIG_APP_ADV_PERIOD_MS - CONFIG_APP_ADV_ON_TIME_MS));
}

int main(void)
{
	const int err = bt_enable(NULL);

	if (err) {
		return err;
	}

	(void)k_work_schedule(&start_work, K_NO_WAIT);
	return 0;
}
