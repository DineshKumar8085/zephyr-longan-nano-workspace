#include <zephyr/kernel.h>
#include "cli.h"
#include "sensor.h"

int main(void)
{
    sensor_init();
    cli_init();

    return 0;
}
