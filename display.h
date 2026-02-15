#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <X11/Xlib.h>
#include "cpu.h"
#include "memory.h"

#define is_bit_set(v, b) ((v >> b) & 1)

enum lcd_reg {
  rLCDC = 0, rSTAT, rSCY, rSCX, rLY, rLYC, rDMA,
  rBGP, rOBP0, rOBP1, rWY, rWX
};

struct lcd_controller {
  // registers
  uint8_t regs[12];

  // cycle since last vertical line refresh
  uint32_t t_cycles_since_last_line_refresh;

  // current frame state where each entry is one of the 4 DMG colors
  uint8_t frame[256][256];

  struct mem_controller *memory_c;
  struct interrupt_controller *interrupt_c;
};

void init_lcd();
void lcd_tick(struct lcd_controller *);

uint8_t lcd_reg_read(struct lcd_controller*, enum lcd_reg);
void lcd_reg_write(struct lcd_controller*, enum lcd_reg, uint8_t);

#endif
