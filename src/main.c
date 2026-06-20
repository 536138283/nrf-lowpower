/**
 * nRF52832 timed, non-connectable BLE advertiser using Nordic nRF5 SDK + S132.
 *
 * This application is intentionally advertising-only: it never initializes GAP
 * connectable parameters, never starts services, and never accepts connections.
 * A short advertising window is opened periodically, then the SoftDevice radio
 * is stopped and the CPU returns to system-on sleep through nrf_pwr_mgmt_run().
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "app_error.h"
#include "app_timer.h"
#include "ble_advdata.h"
#include "nrf_gpio.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_soc.h"

#define DEVICE_NAME                     "nRF52-LP-ADV"
#define APP_BLE_CONN_CFG_TAG            1
#define APP_BLE_OBSERVER_PRIO           3

#define ADV_INTERVAL_MS                 100U
#define ADV_ON_TIME_MS                  1000U
#define ADV_PERIOD_MS                   60000U
#define APP_COMPANY_IDENTIFIER          0xFFFFU  /* Replace before production. */
#define APP_PAYLOAD_MAGIC               0x4C50U  /* Little-endian ASCII "LP". */
#define ADV_INTERVAL_UNITS              MSEC_TO_UNITS(ADV_INTERVAL_MS, UNIT_0_625_MS)

#if ADV_PERIOD_MS <= ADV_ON_TIME_MS
#error "ADV_PERIOD_MS must be greater than ADV_ON_TIME_MS."
#endif

APP_TIMER_DEF(m_adv_start_timer);
APP_TIMER_DEF(m_adv_stop_timer);
static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
static uint8_t m_encoded_adv_data[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static ble_gap_adv_data_t m_gap_adv_data =
{
    .adv_data =
    {
        .p_data = m_encoded_adv_data,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX,
    },
    .scan_rsp_data =
    {
        .p_data = NULL,
        .len    = 0,
    },
};

static uint32_t m_sequence;

static void advertising_start_timer_handler(void * p_context);
static void advertising_stop_timer_handler(void * p_context);

static void low_power_gpio_prepare(void)
{
    for (uint32_t pin = 0; pin < 32; pin++)
    {
        nrf_gpio_cfg_default(pin);
    }
}

static void advertising_payload_update(void)
{
    ret_code_t err_code;
    uint8_t manufacturer_payload[10];
    ble_advdata_t advdata;
    ble_advdata_manuf_data_t manuf_data;
    ble_gap_adv_params_t adv_params;

    const uint32_t uptime_s = app_timer_cnt_get() / APP_TIMER_TICKS(1000);

    manufacturer_payload[0] = (uint8_t)(APP_COMPANY_IDENTIFIER & 0xFFU);
    manufacturer_payload[1] = (uint8_t)(APP_COMPANY_IDENTIFIER >> 8);
    manufacturer_payload[2] = (uint8_t)(APP_PAYLOAD_MAGIC & 0xFFU);
    manufacturer_payload[3] = (uint8_t)(APP_PAYLOAD_MAGIC >> 8);
    manufacturer_payload[4] = (uint8_t)(m_sequence & 0xFFU);
    manufacturer_payload[5] = (uint8_t)(m_sequence >> 8);
    manufacturer_payload[6] = (uint8_t)(m_sequence >> 16);
    manufacturer_payload[7] = (uint8_t)(m_sequence >> 24);
    manufacturer_payload[8] = (uint8_t)(uptime_s & 0xFFU);
    manufacturer_payload[9] = (uint8_t)(uptime_s >> 8);
    m_sequence++;

    memset(&advdata, 0, sizeof(advdata));
    memset(&manuf_data, 0, sizeof(manuf_data));
    memset(&adv_params, 0, sizeof(adv_params));

    manuf_data.company_identifier = APP_COMPANY_IDENTIFIER;
    manuf_data.data.p_data        = &manufacturer_payload[2];
    manuf_data.data.size          = sizeof(manufacturer_payload) - sizeof(uint16_t);

    advdata.name_type             = BLE_ADVDATA_FULL_NAME;
    advdata.flags                 = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    advdata.p_manuf_specific_data = &manuf_data;

    m_gap_adv_data.adv_data.len = BLE_GAP_ADV_SET_DATA_SIZE_MAX;
    err_code = ble_advdata_encode(&advdata,
                                  m_gap_adv_data.adv_data.p_data,
                                  &m_gap_adv_data.adv_data.len);
    APP_ERROR_CHECK(err_code);

    adv_params.properties.type = BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
    adv_params.p_peer_addr     = NULL;
    adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY;
    adv_params.interval        = ADV_INTERVAL_UNITS;
    adv_params.duration        = 0;
    adv_params.primary_phy     = BLE_GAP_PHY_1MBPS;

    err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_gap_adv_data, &adv_params);
    APP_ERROR_CHECK(err_code);
}

static void advertising_start(void)
{
    ret_code_t err_code;

    advertising_payload_update();

    err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_start(m_adv_stop_timer,
                               APP_TIMER_TICKS(ADV_ON_TIME_MS),
                               NULL);
    APP_ERROR_CHECK(err_code);
}

static void advertising_stop(void)
{
    ret_code_t err_code = sd_ble_gap_adv_stop(m_adv_handle);

    if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_INVALID_STATE))
    {
        APP_ERROR_CHECK(err_code);
    }

    err_code = app_timer_start(m_adv_start_timer,
                               APP_TIMER_TICKS(ADV_PERIOD_MS - ADV_ON_TIME_MS),
                               NULL);
    APP_ERROR_CHECK(err_code);
}

static void advertising_start_timer_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    advertising_start();
}

static void advertising_stop_timer_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    advertising_stop();
}

static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_adv_start_timer,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                advertising_start_timer_handler);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_adv_stop_timer,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                advertising_stop_timer_handler);
    APP_ERROR_CHECK(err_code);
}

static void power_management_init(void)
{
    ret_code_t err_code;

    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);

    err_code = sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
    APP_ERROR_CHECK(err_code);
}

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

static void gap_params_init(void)
{
    ret_code_t err_code;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);
}

int main(void)
{
    low_power_gpio_prepare();
    timers_init();
    power_management_init();
    ble_stack_init();
    gap_params_init();

    advertising_start();

    for (;;)
    {
        nrf_pwr_mgmt_run();
    }
}
