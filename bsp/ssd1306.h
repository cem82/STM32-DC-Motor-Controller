#ifndef SSD1306_H
#define SSD1306_H
#include <stdbool.h>
#include <stdint.h>

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64

bool ssd1306_init(void);
void ssd1306_fill(bool on);
bool ssd1306_update(void);

void ssd1306_set_cursor(uint8_t x, uint8_t line);
void ssd1306_clear_line(uint8_t line);
void ssd1306_write_string(const char *s);

#endif /* SSD1306_H */
