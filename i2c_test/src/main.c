#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

/* ---------------- MPU DEFINITIONS ---------------- */

#define I2C_NODE DT_NODELABEL(i2c0)

#define MPU_ADDR   0x68
#define PWR_MGMT_1 0x6B
#define DATA_START 0x3B


/* ---------------- THREAD STACKS ---------------- */

#define STACK_SIZE 1024

K_THREAD_STACK_DEFINE(stack1, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack2, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack3, STACK_SIZE);
K_THREAD_STACK_DEFINE(stack4, STACK_SIZE);

struct k_thread t1, t2, t3, t4;


/* ---------------- MPU DRIVER ---------------- */

int mpu6050_init(const struct device *i2c_dev)
{
    uint8_t reset_cmd[2] = {0x6B, 0x80};
    i2c_write(i2c_dev, reset_cmd, 2, MPU_ADDR);
    k_sleep(K_MSEC(100));

    uint8_t wake_cmd[2] = {0x6B, 0x01};
    i2c_write(i2c_dev, wake_cmd, 2, MPU_ADDR);
    k_sleep(K_MSEC(10));

    /* Accelerometer ±2g */
    uint8_t accel_cfg[2] = {0x1C, 0x00};
    i2c_write(i2c_dev, accel_cfg, 2, MPU_ADDR);

    /* Gyroscope ±250 deg/s */
    uint8_t gyro_cfg[2] = {0x1B, 0x00};
    i2c_write(i2c_dev, gyro_cfg, 2, MPU_ADDR);

    return 0;
}


int mpu6050_read(const struct device *i2c_dev,
                 float *ax, float *ay, float *az,
                 float *gx, float *gy, float *gz,
                 float *temp)
{
    uint8_t reg = DATA_START;
    uint8_t data[14];

    int ret = i2c_write_read(i2c_dev, MPU_ADDR, &reg, 1, data, 14);

    if (ret != 0)
        return ret;

    int16_t ax_raw = (data[0] << 8) | data[1];
    int16_t ay_raw = (data[2] << 8) | data[3];
    int16_t az_raw = (data[4] << 8) | data[5];

    int16_t temp_raw = (data[6] << 8) | data[7];

    int16_t gx_raw = (data[8] << 8) | data[9];
    int16_t gy_raw = (data[10] << 8) | data[11];
    int16_t gz_raw = (data[12] << 8) | data[13];

    *ax = ax_raw / 16384.0;
    *ay = ay_raw / 16384.0;
    *az = az_raw / 16384.0;

    *gx = gx_raw / 131.0;
    *gy = gy_raw / 131.0;
    *gz = gz_raw / 131.0;

    *temp = (temp_raw / 340.0) + 36.53;

    return 0;
}


/* ---------------- THREAD 1 : MPU SENSOR ---------------- */

void mpu_thread(void *a, void *b, void *c)
{
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

    float ax, ay, az;
    float gx, gy, gz;
    float temp;

    while (1)
    {
        int ret = mpu6050_read(i2c_dev,
                               &ax, &ay, &az,
                               &gx, &gy, &gz,
                               &temp);

        if (ret != 0) {
            printk("MPU read error\n");
            k_sleep(K_MSEC(500));
            continue;
        }

        printk("\n");
        int ax_i = ax * 100;
        int ay_i = ay * 100;
        int az_i = az * 100;

        int gx_i = gx * 100;
        int gy_i = gy * 100;
        int gz_i = gz * 100;

        int temp_i = temp * 100;

        printk("ACCEL [g*100]      X:%d Y:%d Z:%d\n", ax_i, ay_i, az_i);
        printk("GYRO  [deg/s*100]  X:%d Y:%d Z:%d\n", gx_i, gy_i, gz_i);
        printk("TEMP  [C*100]      %d\n", temp_i);

        k_sleep(K_MSEC(500));
    }
}


/* ---------------- THREAD 2 ---------------- */

void thread2_func(void *a, void *b, void *c)
{
    int counter = 0;

    while (1)
    {
        counter++;
        printk("Thread 2 counter: %d\n", counter);

        k_sleep(K_SECONDS(1));
    }
}


/* ---------------- THREAD 3 ---------------- */

void thread3_func(void *a, void *b, void *c)
{
    while (1)
    {
        printk("Thread 3 running\n");

        k_sleep(K_SECONDS(2));
    }
}


/* ---------------- THREAD 4 ---------------- */

void thread4_func(void *a, void *b, void *c)
{
    while (1)
    {
        printk("System monitor thread\n");

        k_sleep(K_SECONDS(3));
    }
}


/* ---------------- MAIN ---------------- */

int main(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

    if (!device_is_ready(i2c_dev)) {
        printk("I2C device not ready\n");
        return 0;
    }

    printk("MPU6050 Init\n");

    mpu6050_init(i2c_dev);


    /* Create Threads */

    k_thread_create(&t1, stack1, STACK_SIZE,
                    mpu_thread,
                    NULL,NULL,NULL,
                    1,0,K_NO_WAIT);

    k_thread_create(&t2, stack2, STACK_SIZE,
                    thread2_func,
                    NULL,NULL,NULL,
                    2,0,K_NO_WAIT);

    k_thread_create(&t3, stack3, STACK_SIZE,
                    thread3_func,
                    NULL,NULL,NULL,
                    3,0,K_NO_WAIT);

    k_thread_create(&t4, stack4, STACK_SIZE,
                    thread4_func,
                    NULL,NULL,NULL,
                    4,0,K_NO_WAIT);

    return 0;
}
