#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <X11/Xlib.h>
#include "cpu.h"
#include "memory.h"

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

  // cycle since last vertical line refresh
  uint32_t t_cycles;

  struct mem_controller *mc;
  struct interrupt_controller *ic;
};

void init_lcd();
void lcd_tick(struct lcd_controller *);
void lcd_LCDC_write(struct lcd_controller *, uint8_t);


#endif
