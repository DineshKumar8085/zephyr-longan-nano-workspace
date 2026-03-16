#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>

#include "lcd_text.h"

#define I2C_NODE DT_NODELABEL(i2c0)
#define DISPLAY_NODE DT_CHOSEN(zephyr_display)

#define MPU_ADDR 0x68
#define PWR_MGMT_1 0x6B
#define DATA_START 0x3B

int main(void)
{

    const struct device *i2c = DEVICE_DT_GET(I2C_NODE);
    const struct device *lcd = DEVICE_DT_GET(DISPLAY_NODE);

    if(!device_is_ready(i2c) || !device_is_ready(lcd))
        return 0;

    lcd_clear(lcd);

    lcd_draw_string(lcd,20,0,"MPU6050 DASH");

    uint8_t wake[2] = {PWR_MGMT_1,0x01};
    i2c_write(i2c,wake,2,MPU_ADDR);

    while(1)
{
    lcd_clear(lcd);

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

    /* Convert to physical units */

    int ax = (ax_raw * 1000) / 16384;
    int ay = (ay_raw * 1000) / 16384;
    int az = (az_raw * 1000) / 16384;

    int gx = (gx_raw * 100) / 131;
    int gy = (gy_raw * 100) / 131;
    int gz = (gz_raw * 100) / 131;

    int temp = (temp_raw * 100) / 340 + 3653;

    /* UART Output */

    printk("\n");
    printk("ACCEL [mg]  X:%d  Y:%d  Z:%d\n", ax, ay, az);
    printk("GYRO  [dps] X:%d  Y:%d  Z:%d\n", gx, gy, gz);
    printk("TEMP  [C*100]: %d\n", temp);

    /* LCD Output */

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

    k_sleep(K_MSEC(800));
}
}
