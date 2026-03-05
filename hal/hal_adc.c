/*
 * ESPClaw - hal/hal_adc.c
 * ADC HAL implementation with calibration support.
 */
#include "hal_adc.h"
#include "config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "hal_adc";

/* ADC unit and channel mapping */
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static bool s_initialized = false;
static int s_channel_atten[ADC_MAX_CHANNELS];  /* Attenuation per channel */

/* Pin to ADC1 channel mapping (C3/C5/S3 compatible) */
static int get_adc1_channel(int pin)
{
    /* ADC1 channels: GPIO pins to ADC channel mapping */
    /* C3/C5: ADC1_CH0=GPIO0, CH1=GPIO1, CH2=GPIO2, CH3=GPIO3, CH4=GPIO4 */
    /* S3: ADC1_CH0=GPIO1, CH1=GPIO2, CH2=GPIO3, CH3=GPIO4, CH4=GPIO5 */
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5)
    if (pin >= 0 && pin <= 4) return pin;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    const int pin_map[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        if (pin == pin_map[i]) return i;
    }
#endif
    return -1;
}

esp_err_t hal_adc_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t err;

    /* Initialize ADC1 unit */
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    err = adc_oneshot_new_unit(&init_cfg, &s_adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Setup calibration (best effort - may not be available on all chips) */
#if CONFIG_IDF_TARGET_ESP32C3
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);
#elif CONFIG_IDF_TARGET_ESP32S3
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);
#elif CONFIG_IDF_TARGET_ESP32C5
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali_handle);
#else
    err = ESP_ERR_NOT_SUPPORTED;
#endif

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration enabled");
    } else {
        ESP_LOGI(TAG, "ADC calibration not available, using raw values");
        s_cali_handle = NULL;
    }

    /* Default attenuation for all channels */
    for (int i = 0; i < ADC_MAX_CHANNELS; i++) {
        s_channel_atten[i] = ADC_ATTEN_DB_12;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "ADC HAL ready");
    return ESP_OK;
}

esp_err_t hal_adc_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    if (s_cali_handle) {
        adc_cali_delete(s_cali_handle);
        s_cali_handle = NULL;
    }

    if (s_adc_handle) {
        adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "ADC HAL deinitialized");
    return ESP_OK;
}

bool hal_adc_pin_valid(int pin)
{
    int ch = get_adc1_channel(pin);
    return (ch >= 0 && ch < ADC_MAX_CHANNELS);
}

int hal_adc_pin_to_channel(int pin)
{
    return get_adc1_channel(pin);
}

static esp_err_t ensure_channel_configured(int pin)
{
    int ch = get_adc1_channel(pin);
    if (ch < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Configure ADC channel (re-config is OK) */
    adc_oneshot_chan_cfg_t config = {
        .atten = s_channel_atten[ch],
        .bitwidth = ADC_BITWIDTH_12,
    };
    return adc_oneshot_config_channel(s_adc_handle, ch, &config);
}

esp_err_t hal_adc_read_raw(int pin, int *raw_value)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!hal_adc_pin_valid(pin)) {
        ESP_LOGW(TAG, "Pin %d not valid for ADC", pin);
        return ESP_ERR_INVALID_ARG;
    }

    int ch = get_adc1_channel(pin);
    esp_err_t err = ensure_channel_configured(pin);
    if (err != ESP_OK) {
        return err;
    }

    int raw;
    err = adc_oneshot_read(s_adc_handle, ch, &raw);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(err));
        return err;
    }

    *raw_value = raw;
    return ESP_OK;
}

esp_err_t hal_adc_read_voltage(int pin, int *voltage_mv)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    int raw;
    esp_err_t err = hal_adc_read_raw(pin, &raw);
    if (err != ESP_OK) {
        return err;
    }

    /* Try calibrated conversion */
    if (s_cali_handle) {
        int voltage;
        err = adc_cali_raw_to_voltage(s_cali_handle, raw, &voltage);
        if (err == ESP_OK) {
            *voltage_mv = voltage;
            return ESP_OK;
        }
    }

    /* Fallback: simple linear conversion (less accurate) */
    *voltage_mv = (raw * ADC_DEFAULT_VREF_MV) / 4095;
    return ESP_OK;
}

esp_err_t hal_adc_read_averaged(int pin, int samples, int *voltage_mv)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (samples < 1) samples = 1;
    if (samples > 64) samples = 64;

    int64_t sum = 0;
    int valid = 0;

    for (int i = 0; i < samples; i++) {
        int voltage;
        if (hal_adc_read_voltage(pin, &voltage) == ESP_OK) {
            sum += voltage;
            valid++;
        }
    }

    if (valid == 0) {
        return ESP_FAIL;
    }

    *voltage_mv = (int)(sum / valid);
    return ESP_OK;
}

esp_err_t hal_adc_set_atten(int pin, int atten)
{
    if (!hal_adc_pin_valid(pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (atten < 0 || atten > 3) {
        return ESP_ERR_INVALID_ARG;
    }

    int ch = get_adc1_channel(pin);
    s_channel_atten[ch] = atten;
    return ESP_OK;
}
