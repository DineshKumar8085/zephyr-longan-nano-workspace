cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mpu_uart_cmd)

target_sources(app PRIVATE src/main.c src/lcd_text.c src/font5x7.c)
