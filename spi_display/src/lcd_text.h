#pragma once

#include <zephyr/device.h>

void lcd_clear(const struct device *dev);

void lcd_draw_char(const struct device *dev,int x,int y,char c);

void lcd_draw_string(const struct device *dev,int x,int y,const char *str);

void lcd_draw_int(const struct device *dev,int x,int y,int value);
