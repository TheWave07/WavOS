#include <io/screen.h>
#include <common/tools.h>
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg)
{
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

size_t strlen(const char* str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;

void terminal_init(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(COLOR_GREEN, COLOR_BLACK);
    terminal_buffer = (uint16_t*) 0xB8000;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t idx = y * VGA_WIDTH + x;
            terminal_buffer[idx] = vga_entry(0, terminal_color);
        }
    }
}

void terminal_set_color(uint8_t color)
{
    terminal_color = color;
}

void terminal_put_entry_at(char c, uint8_t color, size_t x, size_t y)
{
    const size_t idx = y * VGA_WIDTH + x;
    terminal_buffer[idx] = vga_entry(c, color);
}

void terminal_scroll(void)
{
    for (size_t i = VGA_WIDTH; i < VGA_WIDTH * VGA_HEIGHT; i++)
        terminal_buffer[i - VGA_WIDTH] = terminal_buffer[i];
    for (size_t i = 0; i < VGA_WIDTH; i++)
    {
        terminal_buffer[i + (VGA_HEIGHT - 1) * (VGA_WIDTH)] = vga_entry(0, terminal_color);;
    }
    
    --terminal_row;
}

void terminal_put_char(char c)
{
    //handles newlines
    if(c == '\n') {
        terminal_column = 0;
        if(++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
    } else {
        terminal_put_entry_at(c, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            if(++terminal_row == VGA_HEIGHT) {
                terminal_scroll();
            }
        }
    }
}

void terminal_write(const char* data, size_t size)
{
    for (size_t i = 0; i < size ; i++)
        terminal_put_char(data[i]);
}

void terminal_write_string(const char* data)
{
    terminal_write(data, strlen(data));
}

void terminal_write_int(int i, int base)
{
    char buf[100];
	itoa(i,buf,base);
    terminal_write_string(buf);
}

void terminal_rem(void)
{
    if (terminal_column-- == 0) {
        terminal_column = VGA_WIDTH;
        if(terminal_row > 0) {
            --terminal_row;
        }
        while(((terminal_buffer[terminal_column + terminal_row * VGA_WIDTH] & 0xFF) == 0) && ((terminal_column + terminal_row * VGA_WIDTH) > 0))
        {
            if (--terminal_column == 0) {
                terminal_column = VGA_WIDTH;
                if(terminal_row > 0) {
                    --terminal_row;
                }
            }
        }
    }
    terminal_put_entry_at(0, terminal_color, terminal_column, terminal_row);
}
