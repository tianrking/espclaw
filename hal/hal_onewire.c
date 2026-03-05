/*
 * ESPClaw - hal/hal_onewire.c
 * OneWire HAL implementation for DS18B20 temperature sensors.
 * Bit-banged implementation for reliability.
 */
#include "hal_onewire.h"
#include "config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "hal_1wire";

/* Bus state tracking */
typedef struct {
    bool active;
    int pin;
    int resolution;  /* Current resolution (9-12 bits) */
} onewire_bus_t;

static onewire_bus_t s_buses[ONEWIRE_MAX_BUSES];
static bool s_initialized = false;

/* Precise microsecond delay using esp_timer */
static inline void delay_us(uint32_t us)
{
    int64_t start = esp_timer_get_time();
    while ((esp_timer_get_time() - start) < us) {
        /* Busy wait for precision */
    }
}

/* CRC8 calculation for DS18B20 */
static uint8_t crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (int j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            byte >>= 1;
        }
    }
    return crc;
}

/* Set pin as output */
static void set_output(int pin)
{
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
}

/* Set pin as input (for reading) */
static void set_input(int pin)
{
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
}

/* Write bit */
static void write_bit(int pin, int bit)
{
    set_output(pin);
    if (bit) {
        gpio_set_level(pin, 0);
        delay_us(6);
        gpio_set_level(pin, 1);
        delay_us(64);
    } else {
        gpio_set_level(pin, 0);
        delay_us(60);
        gpio_set_level(pin, 1);
        delay_us(10);
    }
}

/* Read bit */
static int read_bit(int pin)
{
    set_output(pin);
    gpio_set_level(pin, 0);
    delay_us(6);
    set_input(pin);
    delay_us(9);
    int bit = gpio_get_level(pin);
    delay_us(55);
    return bit;
}

/* Write byte */
static void write_byte(int pin, uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        write_bit(pin, byte & 0x01);
        byte >>= 1;
    }
}

/* Read byte */
static uint8_t read_byte(int pin)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        byte |= (read_bit(pin) << i);
    }
    return byte;
}

/* Reset pulse - returns true if device present */
static bool reset_pulse(int pin)
{
    set_output(pin);
    gpio_set_level(pin, 0);
    delay_us(480);
    set_input(pin);
    delay_us(70);
    int presence = gpio_get_level(pin);
    delay_us(410);
    return (presence == 0);  /* Device pulls low if present */
}

esp_err_t hal_onewire_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    memset(s_buses, 0, sizeof(s_buses));
    for (int i = 0; i < ONEWIRE_MAX_BUSES; i++) {
        s_buses[i].pin = -1;
        s_buses[i].resolution = ONEWIRE_RES_12BIT;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "OneWire HAL ready: %d buses max", ONEWIRE_MAX_BUSES);
    return ESP_OK;
}

esp_err_t hal_onewire_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    for (int i = 0; i < ONEWIRE_MAX_BUSES; i++) {
        if (s_buses[i].active) {
            hal_onewire_release(i);
        }
    }

    s_initialized = false;
    ESP_LOGI(TAG, "OneWire HAL deinitialized");
    return ESP_OK;
}

bool hal_onewire_pin_valid(int pin)
{
    if (pin < GPIO_MIN_PIN || pin > GPIO_MAX_PIN) {
        return false;
    }
    return true;
}

esp_err_t hal_onewire_setup(int pin, int bus_id)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (bus_id < 0 || bus_id >= ONEWIRE_MAX_BUSES) {
        ESP_LOGW(TAG, "Invalid bus ID %d (valid: 0-%d)", bus_id, ONEWIRE_MAX_BUSES - 1);
        return ESP_ERR_INVALID_ARG;
    }

    if (!hal_onewire_pin_valid(pin)) {
        ESP_LOGW(TAG, "Pin %d not valid for OneWire", pin);
        return ESP_ERR_INVALID_ARG;
    }

    /* Configure GPIO with pull-up */
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,  /* Open-drain mode */
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        return err;
    }

    s_buses[bus_id].active = true;
    s_buses[bus_id].pin = pin;
    s_buses[bus_id].resolution = ONEWIRE_RES_12BIT;

    ESP_LOGI(TAG, "OneWire bus %d setup on pin %d", bus_id, pin);
    return ESP_OK;
}

esp_err_t hal_onewire_release(int bus_id)
{
    if (bus_id < 0 || bus_id >= ONEWIRE_MAX_BUSES) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_buses[bus_id].active) {
        return ESP_OK;
    }

    /* Reset GPIO to input */
    gpio_set_direction(s_buses[bus_id].pin, GPIO_MODE_INPUT);

    s_buses[bus_id].active = false;
    s_buses[bus_id].pin = -1;

    ESP_LOGI(TAG, "OneWire bus %d released", bus_id);
    return ESP_OK;
}

esp_err_t hal_onewire_scan(int bus_id, onewire_rom_t *devices,
                           int max_count, int *found)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (bus_id < 0 || bus_id >= ONEWIRE_MAX_BUSES || !s_buses[bus_id].active) {
        return ESP_ERR_INVALID_ARG;
    }

    int pin = s_buses[bus_id].pin;
    *found = 0;

    /* Reset and check for presence */
    if (!reset_pulse(pin)) {
        return ESP_OK;  /* No devices found */
    }

    /* Issue SEARCH_ROM command */
    write_byte(pin, 0xF0);

    /* Simple search - find first device only for now */
    /* Full search algorithm is complex; this simplified version finds one device */
    uint8_t rom[8] = {0};

    for (int bit_pos = 0; bit_pos < 64 && *found < max_count; bit_pos++) {
        int bit_a = read_bit(pin);  /* First bit */
        int bit_b = read_bit(pin);  /* Complement bit */

        if (bit_a && bit_b) {
            /* No device responded */
            break;
        }

        int selected_bit;
        if (bit_a || bit_b) {
            /* Only one type, select it */
            selected_bit = bit_a ? 1 : 0;
        } else {
            /* Both types present, select 0 first */
            selected_bit = 0;
        }

        /* Store and write selected bit */
        rom[bit_pos / 8] |= (selected_bit << (bit_pos % 8));
        write_bit(pin, selected_bit);
    }

    /* Verify CRC */
    if (crc8(rom, 7) == rom[7]) {
        memcpy(&devices[*found], rom, 8);
        (*found)++;
    }

    return ESP_OK;
}

esp_err_t hal_onewire_read_temp(int bus_id, const onewire_rom_t *rom,
                                onewire_temp_t *result)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (bus_id < 0 || bus_id >= ONEWIRE_MAX_BUSES || !s_buses[bus_id].active) {
        return ESP_ERR_INVALID_ARG;
    }

    int pin = s_buses[bus_id].pin;
    memset(result, 0, sizeof(*result));

    /* Reset and select device */
    if (!reset_pulse(pin)) {
        return ESP_ERR_NOT_FOUND;
    }

    if (rom) {
        /* MATCH_ROM with specific address */
        write_byte(pin, 0x55);
        for (int i = 0; i < 8; i++) {
            write_byte(pin, ((const uint8_t*)rom)[i]);
        }
    } else {
        /* SKIP_ROM - single device on bus */
        write_byte(pin, 0xCC);
    }

    /* Start temperature conversion */
    write_byte(pin, 0x44);

    /* Wait for conversion (depends on resolution) */
    int wait_ms;
    switch (s_buses[bus_id].resolution) {
        case 9:  wait_ms = 94;  break;
        case 10: wait_ms = 188; break;
        case 11: wait_ms = 375; break;
        case 12:
        default: wait_ms = 750; break;
    }
    vTaskDelay(pdMS_TO_TICKS(wait_ms));

    /* Read scratchpad */
    if (!reset_pulse(pin)) {
        return ESP_ERR_TIMEOUT;
    }

    if (rom) {
        write_byte(pin, 0x55);
        for (int i = 0; i < 8; i++) {
            write_byte(pin, ((const uint8_t*)rom)[i]);
        }
    } else {
        write_byte(pin, 0xCC);
    }

    write_byte(pin, 0xBE);  /* READ_SCRATCHPAD */

    /* Read 9 bytes */
    uint8_t scratchpad[9];
    for (int i = 0; i < 9; i++) {
        scratchpad[i] = read_byte(pin);
    }

    /* Verify CRC */
    if (crc8(scratchpad, 8) != scratchpad[8]) {
        result->valid = false;
        return ESP_ERR_INVALID_CRC;
    }

    /* Convert temperature (raw value is 16-bit, 0.0625°C per LSB) */
    int16_t raw = scratchpad[0] | (scratchpad[1] << 8);
    result->temp_c_x100 = (raw * 100) / 16;  /* Convert to °C * 100 */
    result->valid = true;

    /* Copy ROM if provided */
    if (rom) {
        memcpy(&result->rom, rom, sizeof(onewire_rom_t));
    }

    return ESP_OK;
}

esp_err_t hal_onewire_read_all(int bus_id, onewire_temp_t *results,
                               int max_count, int *found)
{
    esp_err_t err;
    onewire_rom_t devices[ONEWIRE_MAX_DEVICES];
    int device_count;

    err = hal_onewire_scan(bus_id, devices, max_count, &device_count);
    if (err != ESP_OK) {
        return err;
    }

    *found = 0;
    for (int i = 0; i < device_count && i < max_count; i++) {
        err = hal_onewire_read_temp(bus_id, &devices[i], &results[*found]);
        if (err == ESP_OK && results[*found].valid) {
            (*found)++;
        }
    }

    return ESP_OK;
}

esp_err_t hal_onewire_set_resolution(int bus_id, const onewire_rom_t *rom,
                                     int bits)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (bus_id < 0 || bus_id >= ONEWIRE_MAX_BUSES || !s_buses[bus_id].active) {
        return ESP_ERR_INVALID_ARG;
    }

    if (bits < 9 || bits > 12) {
        return ESP_ERR_INVALID_ARG;
    }

    int pin = s_buses[bus_id].pin;

    /* Reset and select device */
    if (!reset_pulse(pin)) {
        return ESP_ERR_NOT_FOUND;
    }

    if (rom) {
        write_byte(pin, 0x55);
        for (int i = 0; i < 8; i++) {
            write_byte(pin, ((const uint8_t*)rom)[i]);
        }
    } else {
        write_byte(pin, 0xCC);
    }

    /* Write scratchpad: TH, TL, Configuration */
    write_byte(pin, 0x4E);  /* WRITE_SCRATCHPAD */
    write_byte(pin, 0x00);  /* TH register */
    write_byte(pin, 0x00);  /* TL register */
    write_byte(pin, (bits - 9) << 5);  /* Resolution config: 9=0x00, 10=0x20, 11=0x40, 12=0x60 */

    /* Copy scratchpad to EEPROM */
    if (!reset_pulse(pin)) {
        return ESP_ERR_TIMEOUT;
    }

    if (rom) {
        write_byte(pin, 0x55);
        for (int i = 0; i < 8; i++) {
            write_byte(pin, ((const uint8_t*)rom)[i]);
        }
    } else {
        write_byte(pin, 0xCC);
    }
    write_byte(pin, 0x48);  /* COPY_SCRATCHPAD */
    vTaskDelay(pdMS_TO_TICKS(10));

    s_buses[bus_id].resolution = bits;
    return ESP_OK;
}

bool hal_onewire_bus_active(int bus_id)
{
    if (bus_id < 0 || bus_id >= ONEWIRE_MAX_BUSES) {
        return false;
    }
    return s_buses[bus_id].active;
}

int hal_onewire_get_pin(int bus_id)
{
    if (bus_id < 0 || bus_id >= ONEWIRE_MAX_BUSES) {
        return -1;
    }
    if (!s_buses[bus_id].active) {
        return -1;
    }
    return s_buses[bus_id].pin;
}

void hal_onewire_rom_to_str(const onewire_rom_t *rom, char *buf, size_t buf_sz)
{
    if (!rom || !buf || buf_sz < 17) {
        if (buf && buf_sz > 0) buf[0] = '\0';
        return;
    }

    snprintf(buf, buf_sz, "%02X%02X%02X%02X%02X%02X%02X%02X",
             rom->family,
             rom->serial[0], rom->serial[1], rom->serial[2],
             rom->serial[3], rom->serial[4], rom->serial[5],
             rom->crc);
}
