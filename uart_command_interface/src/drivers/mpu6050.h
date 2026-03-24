#pragma once
#include <zephyr/device.h>
#include "../sensor.h"

void mpu6050_init(const struct device *i2c);
int mpu6050_read(const struct device *i2c, struct mpu_data *data);
