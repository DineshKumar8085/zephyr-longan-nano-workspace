#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdlib.h>

#include "lcd_text.h"

/* ================= CONFIG ================= */

#define I2C_NODE DT_NODELABEL(i2c0)
#define DISPLAY_NODE DT_CHOSEN(zephyr_display)
#define UART_NODE DT_CHOSEN(zephyr_console)

#define MPU_ADDR 0x68
#define PWR_MGMT_1 0x6B
#define DATA_START 0x3B

#define CMD_BUF_SIZE 64
#define CMD_QUEUE_LEN 5

#define PROMPT "longan@board:~$ "

/* ================= QUEUE ================= */

K_MSGQ_DEFINE(cmd_queue, CMD_BUF_SIZE, CMD_QUEUE_LEN, 4);

/* ================= SHARED DATA ================= */

struct mpu_data {
    int ax, ay, az;
    int gx, gy, gz;
    int temp;
};

static struct mpu_data mpu;
K_MUTEX_DEFINE(mpu_mutex);

/* ================= SYSTEM STATE ================= */

static int lcd_enabled = 1;
static int sample_rate = 800;

/* ================= DEVICES ================= */

static const struct device *uart_dev;
static const struct device *i2c_dev;
static const struct device *lcd_dev;

/* ================= UART ================= */

static char rx_buf[CMD_BUF_SIZE];
static int rx_pos = 0;

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
        }
        else {
            if (rx_pos < CMD_BUF_SIZE - 1) {
                rx_buf[rx_pos++] = c;
            }
        }
    }
}

/* ================= SENSOR THREAD ================= */

void sensor_thread(void *p1, void *p2, void *p3)
{
    if (!device_is_ready(i2c_dev) || !device_is_ready(lcd_dev)) {
        printk("Device not ready in sensor thread\n");
        return;
    }

    uint8_t wake[2] = {PWR_MGMT_1, 0x01};
    i2c_write(i2c_dev, wake, 2, MPU_ADDR);

    while (1)
    {
        uint8_t reg = DATA_START;
        uint8_t data[14];

        i2c_write_read(i2c_dev, MPU_ADDR, &reg, 1, data, 14);

        struct mpu_data temp_data;

        temp_data.ax = ((data[0]<<8)|data[1]) * 1000 / 16384;
        temp_data.ay = ((data[2]<<8)|data[3]) * 1000 / 16384;
        temp_data.az = ((data[4]<<8)|data[5]) * 1000 / 16384;

        temp_data.gx = ((data[8]<<8)|data[9]) * 100 / 131;
        temp_data.gy = ((data[10]<<8)|data[11]) * 100 / 131;
        temp_data.gz = ((data[12]<<8)|data[13]) * 100 / 131;

        temp_data.temp = ((data[6]<<8)|data[7]) * 100 / 340 + 3653;

        k_mutex_lock(&mpu_mutex, K_FOREVER);
        mpu = temp_data;
        k_mutex_unlock(&mpu_mutex);

        /* LCD */
        if (lcd_enabled) {

            lcd_clear(lcd_dev);

            lcd_draw_string(lcd_dev,20,0,"MPU6050 DASH");

            lcd_draw_string(lcd_dev,0,15,"AX:");
            lcd_draw_int(lcd_dev,30,15,temp_data.ax);

            lcd_draw_string(lcd_dev,0,25,"AY:");
            lcd_draw_int(lcd_dev,30,25,temp_data.ay);

            lcd_draw_string(lcd_dev,0,35,"AZ:");
            lcd_draw_int(lcd_dev,30,35,temp_data.az);

            lcd_draw_string(lcd_dev,80,15,"GX:");
            lcd_draw_int(lcd_dev,110,15,temp_data.gx);

            lcd_draw_string(lcd_dev,80,25,"GY:");
            lcd_draw_int(lcd_dev,110,25,temp_data.gy);

            lcd_draw_string(lcd_dev,80,35,"GZ:");
            lcd_draw_int(lcd_dev,110,35,temp_data.gz);

            lcd_draw_string(lcd_dev,0,60,"TEMP:");
            lcd_draw_int(lcd_dev,50,60,temp_data.temp);
        }

        k_sleep(K_MSEC(sample_rate));
    }
}

/* ================= CLI THREAD ================= */

void cli_thread(void *p1, void *p2, void *p3)
{
    char cmd[CMD_BUF_SIZE];

    printk("UART COMMAND INTERFACE READY\n");
    printk(PROMPT);

    while (1)
    {
        k_msgq_get(&cmd_queue, cmd, K_FOREVER);

        /* Trim */
        for (int i = strlen(cmd)-1; i >= 0; i--) {
            if (cmd[i]=='\r'||cmd[i]=='\n'||cmd[i]==' ')
                cmd[i]='\0';
            else break;
        }

        struct mpu_data local;

        k_mutex_lock(&mpu_mutex, K_FOREVER);
        local = mpu;
        k_mutex_unlock(&mpu_mutex);

        if (strcmp(cmd, "GET ACCEL") == 0) {
            printk("ACCEL %d %d %d\n", local.ax, local.ay, local.az);
        }
        else if (strcmp(cmd, "GET GYRO") == 0) {
            printk("GYRO %d %d %d\n", local.gx, local.gy, local.gz);
        }
        else if (strcmp(cmd, "GET TEMP") == 0) {
            printk("TEMP %d\n", local.temp);
        }
        else if (strcmp(cmd, "GET ALL") == 0) {
            printk("ACC %d %d %d | GY %d %d %d | T %d\n",
                local.ax, local.ay, local.az,
                local.gx, local.gy, local.gz,
                local.temp);
        }
        else if (strcmp(cmd, "LCD OFF") == 0) {
            lcd_enabled = 0;
            lcd_clear(lcd_dev);
        }
        else if (strcmp(cmd, "LCD ON") == 0) {
            lcd_enabled = 1;
        }
        else if (strncmp(cmd, "SET RATE", 8) == 0) {
            sample_rate = atoi(cmd + 9);
            printk("RATE SET TO %d ms\n", sample_rate);
        }
        else if (strcmp(cmd, "HELP") == 0) {
            printk("GET ACCEL | GET GYRO | GET TEMP | GET ALL\n");
            printk("LCD ON | LCD OFF | SET RATE <ms>\n");
        }
        else {
            printk("ERR:UNKNOWN_CMD\n");
        }

        printk(PROMPT);
    }
}

/* ================= MAIN ================= */

int main(void)
{
    uart_dev = DEVICE_DT_GET(UART_NODE);
    i2c_dev  = DEVICE_DT_GET(I2C_NODE);
    lcd_dev  = DEVICE_DT_GET(DISPLAY_NODE);

    if (!device_is_ready(uart_dev) ||
        !device_is_ready(i2c_dev) ||
        !device_is_ready(lcd_dev)) {

        printk("Device not ready\n");
        return 0;
    }

    uart_irq_callback_user_data_set(uart_dev, uart_cb, NULL);
    uart_irq_rx_enable(uart_dev);

    return 0;
}

/* ================= THREADS ================= */

K_THREAD_DEFINE(sensor_tid, 4096, sensor_thread, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(cli_tid, 4096, cli_thread, NULL, NULL, NULL, 7, 0, 0);
