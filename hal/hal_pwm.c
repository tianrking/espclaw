/*
 * ESPClaw - hal/hal_pwm.c
 * PWM HAL implementation using ESP-IDF LEDC driver.
 */
#include "hal_pwm.h"
#include "config.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "hal_pwm";

/* PWM channel state tracking */
typedef struct {
    bool active;
    int pin;
    uint32_t freq_hz;
    uint32_t duty_pct;
} pwm_channel_t;

static pwm_channel_t s_channels[PWM_MAX_CHANNELS];
static bool s_initialized = false;

/* LEDC timer configuration (shared across channels) */
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_SPEED_MODE     LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES       LEDC_TIMER_13_BIT

esp_err_t hal_pwm_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    memset(s_channels, 0, sizeof(s_channels));

    /* Configure LEDC timer */
    ledc_timer_config_t timer_conf = {
        .speed_mode      = LEDC_SPEED_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num       = LEDC_TIMER,
        .freq_hz         = PWM_DEFAULT_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(err));
        return err;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "PWM HAL ready: %d channels available", PWM_MAX_CHANNELS);
    return ESP_OK;
}

esp_err_t hal_pwm_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Stop all active channels */
    for (int i = 0; i < PWM_MAX_CHANNELS; i++) {
        if (s_channels[i].active) {
            ledc_stop(LEDC_SPEED_MODE, i, 0);
            s_channels[i].active = false;
        }
    }

    s_initialized = false;
    ESP_LOGI(TAG, "PWM HAL deinitialized");
    return ESP_OK;
}

bool hal_pwm_pin_valid(int pin)
{
    /* Check against GPIO safety range */
    if (pin < GPIO_MIN_PIN || pin > GPIO_MAX_PIN) {
        return false;
    }
    /* Most GPIO pins can be used for LEDC output */
    return true;
}

esp_err_t hal_pwm_setup(int pin, int channel, uint32_t freq_hz)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (channel < 0 || channel >= PWM_MAX_CHANNELS) {
        ESP_LOGW(TAG, "Invalid channel %d (valid: 0-%d)", channel, PWM_MAX_CHANNELS - 1);
        return ESP_ERR_INVALID_ARG;
    }

    if (!hal_pwm_pin_valid(pin)) {
        ESP_LOGW(TAG, "Pin %d not valid for PWM", pin);
        return ESP_ERR_INVALID_ARG;
    }

    if (freq_hz < PWM_MIN_FREQ_HZ || freq_hz > PWM_MAX_FREQ_HZ) {
        ESP_LOGW(TAG, "Frequency %lu out of range (%d-%d Hz)",
                 freq_hz, PWM_MIN_FREQ_HZ, PWM_MAX_FREQ_HZ);
        return ESP_ERR_INVALID_ARG;
    }

    /* Stop existing channel if active */
    if (s_channels[channel].active) {
        ledc_stop(LEDC_SPEED_MODE, channel, 0);
    }

    /* Configure LEDC channel */
    ledc_channel_config_t chan_conf = {
        .gpio_num   = pin,
        .speed_mode = LEDC_SPEED_MODE,
        .channel    = channel,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    esp_err_t err = ledc_channel_config(&chan_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Update frequency if different from default */
    if (freq_hz != PWM_DEFAULT_FREQ_HZ) {
        err = ledc_set_freq(LEDC_SPEED_MODE, LEDC_TIMER, freq_hz);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set frequency: %s", esp_err_to_name(err));
        }
    }

    s_channels[channel].active = true;
    s_channels[channel].pin = pin;
    s_channels[channel].freq_hz = freq_hz;
    s_channels[channel].duty_pct = 0;

    ESP_LOGI(TAG, "PWM ch%d: pin=%d, freq=%lu Hz", channel, pin, freq_hz);
    return ESP_OK;
}

esp_err_t hal_pwm_set_duty(int channel, uint32_t duty_pct)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (channel < 0 || channel >= PWM_MAX_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_channels[channel].active) {
        ESP_LOGW(TAG, "Channel %d not configured", channel);
        return ESP_ERR_INVALID_STATE;
    }

    if (duty_pct > PWM_MAX_DUTY) {
        duty_pct = PWM_MAX_DUTY;
    }

    /* Calculate duty value for 13-bit resolution */
    uint32_t max_duty = (1 << 13) - 1;  /* 8191 for 13-bit */
    uint32_t duty_value = (max_duty * duty_pct) / 100;

    esp_err_t err = ledc_set_duty(LEDC_SPEED_MODE, channel, duty_value);
    if (err != ESP_OK) {
        return err;
    }

    err = ledc_update_duty(LEDC_SPEED_MODE, channel);
    if (err != ESP_OK) {
        return err;
    }

    s_channels[channel].duty_pct = duty_pct;
    return ESP_OK;
}

esp_err_t hal_pwm_set_freq(int channel, uint32_t freq_hz)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (channel < 0 || channel >= PWM_MAX_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_channels[channel].active) {
        return ESP_ERR_INVALID_STATE;
    }

    if (freq_hz < PWM_MIN_FREQ_HZ || freq_hz > PWM_MAX_FREQ_HZ) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ledc_set_freq(LEDC_SPEED_MODE, LEDC_TIMER, freq_hz);
    if (err != ESP_OK) {
        return err;
    }

    s_channels[channel].freq_hz = freq_hz;
    return ESP_OK;
}

esp_err_t hal_pwm_stop(int channel)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (channel < 0 || channel >= PWM_MAX_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_channels[channel].active) {
        return ESP_OK;  /* Already stopped */
    }

    esp_err_t err = ledc_stop(LEDC_SPEED_MODE, channel, 0);
    if (err != ESP_OK) {
        return err;
    }

    s_channels[channel].active = false;
    s_channels[channel].duty_pct = 0;
    ESP_LOGI(TAG, "PWM ch%d stopped", channel);
    return ESP_OK;
}

esp_err_t hal_pwm_get_status(int channel, uint32_t *freq_hz, uint32_t *duty_pct)
{
    if (channel < 0 || channel >= PWM_MAX_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_channels[channel].active) {
        return ESP_ERR_NOT_FOUND;
    }

    if (freq_hz) {
        *freq_hz = s_channels[channel].freq_hz;
    }
    if (duty_pct) {
        *duty_pct = s_channels[channel].duty_pct;
    }
    return ESP_OK;
}

bool hal_pwm_channel_active(int channel)
{
    if (channel < 0 || channel >= PWM_MAX_CHANNELS) {
        return false;
    }
    return s_channels[channel].active;
}
