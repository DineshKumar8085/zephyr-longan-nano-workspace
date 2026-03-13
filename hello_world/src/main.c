#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
    printk("Hello from my custom Zephyr app!\n");
    return 0;
}

