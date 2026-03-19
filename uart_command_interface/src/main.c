#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdlib.h>

#include "lcd_text.h"

#define I2C_NODE DT_NODELABEL(i2c0)
#define DISPLAY_NODE DT_CHOSEN(zephyr_display)
#define UART_NODE DT_CHOSEN(zephyr_console)

#define MPU_ADDR 0x68
#define PWR_MGMT_1 0x6B
#define DATA_START 0x3B

#define UART_BUF_SIZE 64

#define PROMPT "longan@board:~$ "

/* UART buffer */
static char rx_buf[UART_BUF_SIZE];
static int rx_pos = 0;
static volatile int cmd_ready = 0;

/* Sensor data */
static int ax, ay, az;
static int gx, gy, gz;
static int temp;

/* UART device */
static const struct device *uart_dev;

static int lcd_enabled = 1;

static int sample_rate = 800;

/* ================= UART ISR ================= */
void uart_cb(const struct device *dev, void *user_data)
{
    uint8_t c;

    while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
        uart_fifo_read(dev, &c, 1);

        /* ===== HANDLE BACKSPACE ===== */
        if (c == 0x08 || c == 0x7F) {   // Backspace or DEL

            if (rx_pos > 0) {
                rx_pos--;

                /* Erase character from terminal */
                uart_poll_out(dev, '\b');
                uart_poll_out(dev, ' ');
                uart_poll_out(dev, '\b');
            }
            continue;
        }

        /* ===== NORMAL ECHO ===== */
        uart_poll_out(dev, c);

        if (c == '\r' || c == '\n') {

            uart_poll_out(dev, '\r');
            uart_poll_out(dev, '\n');

            rx_buf[rx_pos] = '\0';
            cmd_ready = 1;
            rx_pos = 0;
        } 
        else {
            if (rx_pos < UART_BUF_SIZE - 1) {
                rx_buf[rx_pos++] = c;
            }
        }
    }
}
/* ================= COMMAND HANDLER ================= */
void execute_command(const struct device *lcd)
{
    if (strcmp(rx_buf, "GET ACCEL") == 0) {
        printk("ACCEL X:%d Y:%d Z:%d\n", ax, ay, az);
    }
    else if (strcmp(rx_buf, "GET GYRO") == 0) {
        printk("GYRO X:%d Y:%d Z:%d\n", gx, gy, gz);
    }
    else if (strcmp(rx_buf, "GET TEMP") == 0) {
        printk("TEMP %d\n", temp);
    }
    else if (strcmp(rx_buf, "GET ALL") == 0) {
        printk("ACCEL %d %d %d\n", ax, ay, az);
        printk("GYRO  %d %d %d\n", gx, gy, gz);
        printk("TEMP  %d\n", temp);
    }
    else if (strcmp(rx_buf, "LCD OFF") == 0) {
        lcd_enabled = 0;
        lcd_clear(lcd);
    }
    else if (strcmp(rx_buf, "LCD ON") == 0) {
        lcd_enabled = 1;
    }
    else if (strncmp(rx_buf, "SET RATE", 8) == 0) {
        int rate = atoi(rx_buf + 9);
        sample_rate = rate;
        printk("RATE SET TO %d ms\n", sample_rate);
}
    else if (strcmp(rx_buf, "HELP") == 0) {
        printk("Commands:\n");
        printk("GET ACCEL\nGET GYRO\nGET TEMP\nGET ALL\nLCD ON\nLCD OFF\n");
}
    else {
        printk("ERR:UNKNOWN_CMD\n");
    }
}

/* ================= MAIN ================= */
int main(void)
{
    const struct device *i2c = DEVICE_DT_GET(I2C_NODE);
    const struct device *lcd = DEVICE_DT_GET(DISPLAY_NODE);
    uart_dev = DEVICE_DT_GET(UART_NODE);

    if (!device_is_ready(i2c) || !device_is_ready(lcd) || !device_is_ready(uart_dev)) {
        printk("Device not ready\n");
        return 0;
    }

    /* UART init */
    uart_irq_callback_user_data_set(uart_dev, uart_cb, NULL);
    uart_irq_rx_enable(uart_dev);

    printk("UART COMMAND INTERFACE READY\n");
    printk(PROMPT);

    /* Wake MPU6050 */
    uint8_t wake[2] = {PWR_MGMT_1, 0x01};
    i2c_write(i2c, wake, 2, MPU_ADDR);

    lcd_clear(lcd);

    while (1)
    {
        /* ===== SENSOR READ ===== */
        uint8_t reg = DATA_START;
        uint8_t data[14];

        i2c_write_read(i2c, MPU_ADDR, &reg, 1, data, 14);

        int16_t ax_raw = (data[0]<<8)|data[1];
        int16_t ay_raw = (data[2]<<8)|data[3];
        int16_t az_raw = (data[4]<<8)|data[5];

        int16_t gx_raw = (data[8]<<8)|data[9];
        int16_t gy_raw = (data[10]<<8)|data[11];
        int16_t gz_raw = (data[12]<<8)|data[13];

        int16_t temp_raw = (data[6]<<8)|data[7];

        ax = (ax_raw * 1000) / 16384;
        ay = (ay_raw * 1000) / 16384;
        az = (az_raw * 1000) / 16384;

        gx = (gx_raw * 100) / 131;
        gy = (gy_raw * 100) / 131;
        gz = (gz_raw * 100) / 131;

        temp = (temp_raw * 100) / 340 + 3653;

        /* ===== LCD UPDATE ===== */
        if (lcd_enabled) {

          lcd_clear(lcd);

          lcd_draw_string(lcd,20,0,"MPU6050 DASH");

          lcd_draw_string(lcd,0,15,"AX:");
          lcd_draw_int(lcd,30,15,ax);

          lcd_draw_string(lcd,0,25,"AY:");
          lcd_draw_int(lcd,30,25,ay);

          lcd_draw_string(lcd,0,35,"AZ:");
          lcd_draw_int(lcd,30,35,az);

          lcd_draw_string(lcd,80,15,"GX:");
          lcd_draw_int(lcd,110,15,gx);

          lcd_draw_string(lcd,80,25,"GY:");
          lcd_draw_int(lcd,110,25,gy);

          lcd_draw_string(lcd,80,35,"GZ:");
          lcd_draw_int(lcd,110,35,gz);

          lcd_draw_string(lcd,0,60,"TEMP:");
          lcd_draw_int(lcd,50,60,temp);
      }

        /* ===== COMMAND EXECUTION ===== */
        if (cmd_ready) {

    /* ===== TRIM INPUT ===== */
    for (int i = strlen(rx_buf) - 1; i >= 0; i--) {
        if (rx_buf[i] == '\r' || rx_buf[i] == '\n' || rx_buf[i] == ' ')
            rx_buf[i] = '\0';
        else
            break;
    }

    /* ===== EXECUTE COMMAND ===== */
    execute_command(lcd);

    cmd_ready = 0;

    printk(PROMPT);
}

        k_sleep(K_MSEC(sample_rate));
    }
}
