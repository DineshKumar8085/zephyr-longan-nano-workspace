#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

#include "sensor.h"
#include "drivers/mpu6050.h"
#include "lcd_text.h"

#define I2C_NODE DT_NODELABEL(i2c0)
#define DISPLAY_NODE DT_CHOSEN(zephyr_display)

static struct mpu_data mpu;
K_MUTEX_DEFINE(mpu_mutex);

static const struct device *i2c_dev;
static const struct device *lcd_dev;

static int lcd_enabled = 1;
static int sample_rate = 800;

void sensor_init(void)
{
    i2c_dev = DEVICE_DT_GET(I2C_NODE);
    lcd_dev = DEVICE_DT_GET(DISPLAY_NODE);

    if (!device_is_ready(i2c_dev)) {
        printk("I2C NOT READY\n");
        return;
    }

    mpu6050_init(i2c_dev);
}

void sensor_set_rate(int rate)
{
    sample_rate = rate;
}

void sensor_lcd_enable(int enable)
{
    lcd_enabled = enable;
    /* Immediately clear when turning OFF */
    if (!enable && lcd_dev && device_is_ready(lcd_dev)) {
        lcd_clear(lcd_dev);
    }
}

struct mpu_data sensor_get_data(void)
{
    struct mpu_data copy;

    k_mutex_lock(&mpu_mutex, K_FOREVER);
    copy = mpu;
    k_mutex_unlock(&mpu_mutex);

    return copy;
}

/* THREAD */
void sensor_thread(void *a, void *b, void *c)
{
    while (1)
    {
        struct mpu_data temp;

        if (mpu6050_read(i2c_dev, &temp) == 0) {

            k_mutex_lock(&mpu_mutex, K_FOREVER);
            mpu = temp;
            k_mutex_unlock(&mpu_mutex);

            if (lcd_enabled && device_is_ready(lcd_dev)) {

                lcd_clear(lcd_dev);

                lcd_draw_string(lcd_dev,20,0,"MPU6050 DASH");

                lcd_draw_string(lcd_dev,0,15,"AX:");
                lcd_draw_int(lcd_dev,30,15,temp.ax);

                lcd_draw_string(lcd_dev,0,25,"AY:");
                lcd_draw_int(lcd_dev,30,25,temp.ay);

                lcd_draw_string(lcd_dev,0,35,"AZ:");
                lcd_draw_int(lcd_dev,30,35,temp.az);

                lcd_draw_string(lcd_dev,80,15,"GX:");
                lcd_draw_int(lcd_dev,110,15,temp.gx);

                lcd_draw_string(lcd_dev,80,25,"GY:");
                lcd_draw_int(lcd_dev,110,25,temp.gy);

                lcd_draw_string(lcd_dev,80,35,"GZ:");
                lcd_draw_int(lcd_dev,110,35,temp.gz);

                lcd_draw_string(lcd_dev,0,60,"TEMP:");
                lcd_draw_int(lcd_dev,50,60,temp.temp);
            }
            else {
            if (lcd_dev && device_is_ready(lcd_dev)) {
            lcd_clear(lcd_dev);
            }
            }
        }

        k_sleep(K_MSEC(sample_rate));
    }
}

K_THREAD_DEFINE(sensor_tid, 4096, sensor_thread, NULL, NULL, NULL, 5, 0, 0);
