#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdlib.h>

#include "cli.h"
#include "sensor.h"

#define UART_NODE DT_CHOSEN(zephyr_console)
#define CMD_BUF_SIZE 64
#define PROMPT "longan@board:~$ "

static const struct device *uart_dev;
static char rx_buf[CMD_BUF_SIZE];
static int rx_pos = 0;

K_MSGQ_DEFINE(cmd_queue, CMD_BUF_SIZE, 5, 4);

/* ================= UART ISR ================= */

void uart_cb(const struct device *dev, void *user_data)
{
    uint8_t c;

    while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
        uart_fifo_read(dev, &c, 1);

        /* Backspace */
        if (c == 0x08 || c == 0x7F) {
            if (rx_pos > 0) {
                rx_pos--;
                uart_poll_out(dev, '\b');
                uart_poll_out(dev, ' ');
                uart_poll_out(dev, '\b');
            }
            continue;
        }

        uart_poll_out(dev, c);

        if (c == '\r' || c == '\n') {
            uart_poll_out(dev, '\r');
            uart_poll_out(dev, '\n');

            rx_buf[rx_pos] = '\0';
            k_msgq_put(&cmd_queue, rx_buf, K_NO_WAIT);

            rx_pos = 0;
        } else {
            if (rx_pos < CMD_BUF_SIZE - 1)
                rx_buf[rx_pos++] = c;
        }
    }
}

/* ================= CLI THREAD ================= */

void cli_thread(void *a, void *b, void *c)
{
    char cmd[CMD_BUF_SIZE];

    printk("CLI READY\n");
    printk(PROMPT);

    while (1)
    {
        k_msgq_get(&cmd_queue, cmd, K_FOREVER);

        /* ===== TRIM ===== */
        for (int i = strlen(cmd)-1; i >= 0; i--) {
            if (cmd[i]=='\r'||cmd[i]=='\n'||cmd[i]==' ')
                cmd[i]='\0';
            else break;
        }

        struct mpu_data d = sensor_get_data();

        /* ===== COMMANDS ===== */

        if (strcmp(cmd, "GET ACCEL") == 0) {
            printk("ACCEL %d %d %d\n", d.ax, d.ay, d.az);
        }
        else if (strcmp(cmd, "GET GYRO") == 0) {
            printk("GYRO %d %d %d\n", d.gx, d.gy, d.gz);
        }
        else if (strcmp(cmd, "GET TEMP") == 0) {
            printk("TEMP %d\n", d.temp);
        }
        else if (strcmp(cmd, "GET ALL") == 0) {
            printk("ACC %d %d %d | GY %d %d %d | T %d\n",
                   d.ax, d.ay, d.az,
                   d.gx, d.gy, d.gz,
                   d.temp);
        }
        else if (strcmp(cmd, "LCD OFF") == 0) {
            sensor_lcd_enable(0);
            printk("LCD OFF\n");
        }
        else if (strcmp(cmd, "LCD ON") == 0) {
            sensor_lcd_enable(1);
            printk("LCD ON\n");
        }
        else if (strncmp(cmd, "SET RATE", 8) == 0) {
            int rate = atoi(cmd + 9);
            sensor_set_rate(rate);
            printk("RATE SET TO %d ms\n", rate);
        }
        else if (strcmp(cmd, "HELP") == 0) {
            printk("Commands:\n");
            printk("GET ACCEL\nGET GYRO\nGET TEMP\nGET ALL\n");
            printk("LCD ON\nLCD OFF\n");
            printk("SET RATE <ms>\n");
        }
        else {
            printk("ERR:UNKNOWN_CMD\n");
        }

        printk(PROMPT);
    }
}

/* ================= INIT ================= */

void cli_init(void)
{
    uart_dev = DEVICE_DT_GET(UART_NODE);

    uart_irq_callback_user_data_set(uart_dev, uart_cb, NULL);
    uart_irq_rx_enable(uart_dev);
}

K_THREAD_DEFINE(cli_tid, 4096, cli_thread, NULL, NULL, NULL, 7, 0, 0);
