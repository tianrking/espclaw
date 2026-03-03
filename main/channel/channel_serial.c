/*
 * ESPClaw - channel/channel_serial.c
 * Serial CLI channel — interactive shell via USB serial.
 *
 * Input task: reads lines from serial → posts to inbound queue
 * Output task: reads from outbound queue → prints to serial
 * Echo task (Step 3): temporarily echoes inbound → outbound for testing
 */
#include "channel/channel.h"
#include "bus/message_bus.h"
#include "messages.h"
#include "config.h"
#include "platform.h"
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

                /* Post to inbound queue */
                inbound_msg_t msg = {0};
                strncpy(msg.text, line, sizeof(msg.text) - 1);
                msg.source = MSG_SOURCE_SERIAL;
                msg.chat_id = 0;
                message_bus_post_inbound(s_bus, &msg, pdMS_TO_TICKS(100));

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
static void serial_output_task(void *arg)
{
    outbound_msg_t msg;

    while (1) {
        if (xQueueReceive(s_bus->outbound, &msg, portMAX_DELAY) == pdTRUE) {
            if (msg.target == MSG_SOURCE_SERIAL) {
                printf("%s\n", msg.text);
                printf("espclaw> ");
                fflush(stdout);
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
    /* echo_agent_task removed in Step 4 — real agent_loop handles responses */

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
