/*
 * ESPClaw - channel/channel_serial.c
 * Serial CLI channel — interactive shell via USB serial.
 *
 * Input task: reads lines, handles local /commands, posts to inbound queue
 * Output task: reads from outbound queue → prints to serial
 *
 * Local commands (not sent to LLM):
 *   /help  — show this list
 *   /tools — list registered tools
 *   /heap  — current free heap
 *   /reset — software reset
 */
#include "channel/channel.h"
#include "bus/message_bus.h"
#include "messages.h"
#include "config.h"
#include "platform.h"
#include "tool/tool_registry.h"
#include "hal/hal_gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "serial";
static message_bus_t *s_bus = NULL;

/* Install USB Serial JTAG VFS driver so getchar()/putchar() work */
static void serial_init_usb_jtag(void)
{
    usb_serial_jtag_driver_config_t cfg = {
        .rx_buffer_size = 256,
        .tx_buffer_size = 256,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));
    usb_serial_jtag_vfs_register();
    ESP_LOGI(TAG, "USB Serial JTAG VFS driver installed");
}

/* -----------------------------------------------------------------------
 * Local command handler — returns true if command was handled locally
 * (caller should NOT forward to LLM queue)
 * ----------------------------------------------------------------------- */
static bool serial_handle_local_cmd(const char *line)
{
    if (strcmp(line, "/help") == 0) {
        printf("ESPClaw local commands:\n"
               "  /help  - show this help\n"
               "  /tools - list registered tools (callable by LLM)\n"
               "  /heap  - show free heap memory\n"
               "  /gpio  - show allowed GPIO pin range\n"
               "  /reset - software reset\n"
               "Anything else is sent to the LLM agent.\n");
        return true;
    }

    if (strcmp(line, "/tools") == 0) {
        int n = tool_registry_count();
        printf("%d tools registered:\n", n);
        /* Re-use registry list via init log — simplest approach is to
         * call init again; it only prints, doesn't allocate */
        tool_registry_init();
        printf("Ask the LLM: \"what tools do you have?\" for descriptions.\n");
        return true;
    }

    if (strcmp(line, "/heap") == 0) {
        printf("Free heap: %lu bytes\n", (unsigned long)esp_get_free_heap_size());
        return true;
    }

    if (strcmp(line, "/gpio") == 0) {
        char pins[64] = {0};
        hal_gpio_allowed_pins_str(pins, sizeof(pins));
        printf("GPIO allowed pins: [%s] (range %d-%d)\n",
               pins[0] ? pins : "none", GPIO_MIN_PIN, GPIO_MAX_PIN);
        return true;
    }

    if (strcmp(line, "/reset") == 0) {
        printf("Resetting...\n");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
        return true;
    }

    return false; /* not a local command */
}

/* -----------------------------------------------------------------------
 * Serial input task: read lines, post to inbound queue
 * ----------------------------------------------------------------------- */
static void serial_input_task(void *arg)
{
    char line[CHANNEL_RX_BUF_SIZE];
    int pos = 0;

    printf("\nespclaw> ");
    fflush(stdout);

    while (1) {
        int c = getchar();
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        /* Echo the character */
        putchar(c);
        fflush(stdout);

        if (c == '\n' || c == '\r') {
            if (pos > 0) {
                line[pos] = '\0';
                printf("\n");

                /* Check for local /commands first */
                if (serial_handle_local_cmd(line)) {
                    printf("espclaw> ");
                    fflush(stdout);
                } else {
                    /* Forward to agent via inbound queue */
                    inbound_msg_t msg = {0};
                    strncpy(msg.text, line, sizeof(msg.text) - 1);
                    msg.source = MSG_SOURCE_SERIAL;
                    msg.chat_id = 0;
                    message_bus_post_inbound(s_bus, &msg, pdMS_TO_TICKS(100));
                }

                pos = 0;
            } else {
                printf("\nespclaw> ");
                fflush(stdout);
            }
        } else if (c == 127 || c == '\b') {
            /* Backspace */
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (pos < (int)(sizeof(line) - 1)) {
            line[pos++] = (char)c;
        }
    }
}

/* -----------------------------------------------------------------------
 * Serial output task: read from outbound queue, print to serial
 * ----------------------------------------------------------------------- */
#ifdef CONFIG_ESPCLAW_CHANNEL_MQTT
extern void mqtt_post(const char *text);
#endif

static void serial_output_task(void *arg)
{
    outbound_msg_t msg;

    while (1) {
        if (xQueueReceive(s_bus->outbound, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.target) {
            case MSG_SOURCE_SERIAL:
                printf("%s\n", msg.text);
                printf("espclaw> ");
                fflush(stdout);
                break;
#ifdef CONFIG_ESPCLAW_CHANNEL_TELEGRAM
            case MSG_SOURCE_TELEGRAM:
                telegram_post(msg.text, msg.chat_id);
                break;
#endif
#ifdef CONFIG_ESPCLAW_CHANNEL_MQTT
            case MSG_SOURCE_MQTT:
                mqtt_post(msg.text);
                break;
#endif
            default:
                /* Ignore other targets (one-way notification channels) */
                break;
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * Channel ops vtable
 * ----------------------------------------------------------------------- */
static esp_err_t serial_start(message_bus_t *bus)
{
    s_bus = bus;

    /* Init USB Serial JTAG so stdin works */
    serial_init_usb_jtag();

    xTaskCreate(serial_input_task,  "ser_in",  CHANNEL_TASK_STACK_SIZE,
                NULL, CHANNEL_TASK_PRIORITY, NULL);
    xTaskCreate(serial_output_task, "ser_out", CHANNEL_TASK_STACK_SIZE,
                NULL, CHANNEL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Serial CLI started");
    return ESP_OK;
}

static bool serial_is_available(void)
{
    return true; /* Serial is always available */
}

const channel_ops_t serial_channel_ops = {
    .name = "serial",
    .start = serial_start,
    .is_available = serial_is_available,
};

