#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/uart.h"

/* ================= CONFIG ================= */

#define LED_GPIO    15
#define UART_PORT   UART_NUM_0
#define BUF_SIZE    128

static QueueHandle_t uart_queue;

/* ================= STATE ================= */

typedef enum {
    MODE_NORMAL,
    MODE_CONFIG
} system_mode_t;

static system_mode_t sys_mode = MODE_NORMAL;

/* ================= BUFFER UART LINE ================= */

static char rx_buf[128];
static int rx_idx = 0;

/* ================= CHECKSUM ================= */

static uint8_t checksum_calc = 0;
static uint8_t checksum_recv = 0;

/* ================= ADDED: PENDING CONFIG ================= */
/* dùng để tránh đổi baud ngay trong lúc đang truyền config */
static int pending_baud = 0;
static int baud_update_flag = 0;

/* ================= GPIO ================= */

static void led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&cfg);
    gpio_set_level(LED_GPIO, 0);
}

static void led_control(int state)
{
    gpio_set_level(LED_GPIO, state);
}

/* ================= UART APPLY CONFIG ================= */

static void uart_apply_config(int baud)
{
    uart_config_t cfg = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(UART_PORT, &cfg);
}

/* ================= RESET CONFIG ================= */

static void config_reset(void)
{
    checksum_calc = 0;
    checksum_recv = 0;

    /* ADDED: reset pending config */
    pending_baud = 0;
    baud_update_flag = 0;
}

/* ================= PARSE CONFIG LINE ================= */

static void parse_line(char *line)
{
    if (strncmp(line, "BAUDRATE=", 9) == 0) {

        /* FIX: không apply ngay, chỉ lưu lại */
        pending_baud = atoi(line + 9);
        baud_update_flag = 1;
    }
    else if (strncmp(line, "CHECKSUM=", 9) == 0) {
    sscanf(line + 9, "%2hhx", &checksum_recv);
}
}

/* ================= HANDLE COMMAND ================= */

static void handle_command(char *cmd)
{
    if (sys_mode == MODE_CONFIG)
    {
        if (strcmp(cmd, "END_CONFIG") == 0)
        {
            /* checksum check */
            if (checksum_calc == checksum_recv)
            {
                uart_write_bytes(UART_PORT, "CONFIG OK\r\n",
                                 strlen("CONFIG OK\r\n"));

                /* FIX: chỉ apply baud sau khi config OK */
                if (baud_update_flag)
                {
                    /* đợi TX gửi xong trước khi đổi baud */
                    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));
                    uart_apply_config(pending_baud);
                    baud_update_flag = 0;
                }
            }
            else
            {
                uart_write_bytes(UART_PORT, "CHECKSUM FAIL\r\n",
                                 strlen("CHECKSUM FAIL\r\n"));
            }

            sys_mode = MODE_NORMAL;
            config_reset();
        }
        else
        {
        /* KHÔNG tính dòng CHECKSUM vào XOR */
        if (strncmp(cmd, "CHECKSUM=", 9) != 0)
        {
            int len = strlen(cmd);

            for (int i = 0; i < len; i++)
                checksum_calc ^= cmd[i];
        }
        parse_line(cmd);
        }
        return;
    }

    /* ================= NORMAL MODE ================= */

    if (strcmp(cmd, "UPDATE_CONFIG") == 0)
    {
        sys_mode = MODE_CONFIG;
        config_reset();

        uart_write_bytes(UART_PORT,
                         "SEND CONFIGURATION FILE\r\n",
                         strlen("SEND CONFIGURATION FILE\r\n"));
    }
    else if (strcmp(cmd, "LED_ON") == 0)
    {
        led_control(1);
        uart_write_bytes(UART_PORT, "LED ON\r\n",
                         strlen("LED ON\r\n"));
    }
    else if (strcmp(cmd, "LED_OFF") == 0)
    {
        led_control(0);
        uart_write_bytes(UART_PORT, "LED OFF\r\n",
                         strlen("LED OFF\r\n"));
    }
    else
    {
        uart_write_bytes(UART_PORT,
                         "INVALID CMD\r\n",
                         strlen("INVALID CMD\r\n"));
    }
}

/* ================= UART INIT ================= */

static void uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(UART_PORT, &cfg);

    uart_driver_install(UART_PORT,
                        BUF_SIZE * 2,
                        BUF_SIZE * 2,
                        20,
                        &uart_queue,
                        0);
}

/* ================= UART TASK ================= */

static void uart_task(void *arg)
{
    uint8_t ch;

    while (1)
    {
        int len = uart_read_bytes(UART_PORT, &ch, 1, portMAX_DELAY);

        if (len <= 0)
            continue;

        /* bỏ CR */
        if (ch == '\r')
            continue;

        /* END LINE */
        if (ch == '\n')
        {
            rx_buf[rx_idx] = '\0';

            if (rx_idx > 0)
            {
                printf("CMD: %s\n", rx_buf);
                handle_command(rx_buf);
            }

            rx_idx = 0;
        }
        else
        {
            if (rx_idx < sizeof(rx_buf) - 1)
            {
                rx_buf[rx_idx++] = ch;
            }
        }
    }
}

/* ================= MAIN ================= */

void app_main(void)
{
    led_init();
    uart_init();

    xTaskCreate(uart_task,
                "uart_task",
                4096,
                NULL,
                10,
                NULL);
}