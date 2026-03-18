#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#include "lcd_text.h"
#include "font5x7.h"

#define LCD_W 160
#define CHAR_W 6
#define CHAR_H 8

static uint16_t pixel_buf[CHAR_W * CHAR_H];

void lcd_draw_char(const struct device *dev, int x, int y, char c)
{

    int idx = -1;

    /* numbers */
    if (c >= '0' && c <= '9')
        idx = c - '0' + 1;

    /* letters */
    else if (c >= 'A' && c <= 'Z')
        idx = c - 'A' + 11;

    /* colon */
    else if (c == ':')
        idx = 37;

    /* space */
    else if (c == ' ')
        idx = 0;

    if (idx < 0)
        return;

    memset(pixel_buf, 0, sizeof(pixel_buf));

    for (int col = 0; col < 5; col++)
    {

        uint8_t bits = font5x7[idx][col];

        for (int row = 0; row < 7; row++)
        {

            if (bits & (1 << row))
                pixel_buf[row * CHAR_W + col] = 0xFFFF;

        }
    }

    struct display_buffer_descriptor desc =
    {
        .width = CHAR_W,
        .height = CHAR_H,
        .pitch = CHAR_W,
        .buf_size = sizeof(pixel_buf)
    };

    display_write(dev, x, y, &desc, pixel_buf);
}


void lcd_draw_string(const struct device *dev, int x, int y, const char *str)
{

    int cursor = x;

    while (*str)
    {
        lcd_draw_char(dev, cursor, y, *str);
        cursor += CHAR_W;
        str++;
    }

}


void lcd_draw_int(const struct device *dev, int x, int y, int value)
{

    char buf[16];

    snprintk(buf, sizeof(buf), "%d", value);

    lcd_draw_string(dev, x, y, buf);
}


void lcd_clear(const struct device *dev)
{

    static uint16_t line[LCD_W];

    memset(line, 0, sizeof(line));

    struct display_buffer_descriptor desc =
    {
        .width = LCD_W,
        .height = 1,
        .pitch = LCD_W,
        .buf_size = sizeof(line)
    };

    for (int y = 0; y < 80; y++)
    {
        display_write(dev, 0, y, &desc, line);
    }
}
