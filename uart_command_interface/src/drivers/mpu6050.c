#include "mpu6050.h"
#include <zephyr/drivers/i2c.h>

#define MPU_ADDR 0x68
#define PWR_MGMT_1 0x6B
#define DATA_START 0x3B

void mpu6050_init(const struct device *i2c)
{
    uint8_t wake[2] = {PWR_MGMT_1, 0x01};
    i2c_write(i2c, wake, 2, MPU_ADDR);
}

int mpu6050_read(const struct device *i2c, struct mpu_data *d)
{
    uint8_t reg = DATA_START;
    uint8_t data[14];

    if (i2c_write_read(i2c, MPU_ADDR, &reg, 1, data, 14) != 0)
        return -1;

    d->ax = ((data[0]<<8)|data[1]) * 1000 / 16384;
    d->ay = ((data[2]<<8)|data[3]) * 1000 / 16384;
    d->az = ((data[4]<<8)|data[5]) * 1000 / 16384;

    d->gx = ((data[8]<<8)|data[9]) * 100 / 131;
    d->gy = ((data[10]<<8)|data[11]) * 100 / 131;
    d->gz = ((data[12]<<8)|data[13]) * 100 / 131;

    d->temp = ((data[6]<<8)|data[7]) * 100 / 340 + 3653;

    return 0;
}
