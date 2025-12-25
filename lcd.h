#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include "cpu.h"

#define is_bit_set(v, b) ((v >> b) & 1)

struct lcd_controller {
  uint8_t LCDC,
    STAT,
    SCY,
    SCX,
    LY,
    LYC,
    DMA,
    BGP,
    OBP0,
    OBP1,
    WY,
    WX;

  // cycle since last line vertical line refresh
  uint32_t t_cycles;

  struct interrupt_controller *ic;
};

void init_lcd();
void lcd_tick(struct lcd_controller *lcd);


#endif
