#pragma once

struct mpu_data {
    int ax, ay, az;
    int gx, gy, gz;
    int temp;
};

void sensor_init(void);
struct mpu_data sensor_get_data(void);

/* NEW CONTROL FUNCTIONS */
void sensor_set_rate(int rate);
void sensor_lcd_enable(int enable);
