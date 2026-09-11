#ifndef DISPLAY_H
#define DISPLAY_H

#include "types.h"
#include "cpu.h"
#include "memory.h"

enum lcd_reg {
  rLCDC = 0, rSTAT, rSCY, rSCX, rLY, rLYC, rDMA,
  rBGP, rOBP0, rOBP1, rWY, rWX
};

struct obj {
  u8 y;
  u8 x;
  u8 chr_code;
  u8 attr;
};

struct oam {
  int length;
  struct obj objs[40];
};

struct lcd_controller {
  u8 vram[0x2000 /* bg/char data*/ + 0xA0 /* oam data */];

  // registers
  u8 regs[12];

  u32 t_cycles_since_last_line_refresh; // TODO: rename

  u8 bg[256][256];
  u8 wdw[256][256]; // TODO: revisit resizing to 144x160

  struct oam oam;
  struct interrupt_controller *interrupt_c;
};

void init_lcd();
void lcd_tick(struct lcd_controller *);

u8 lcd_vram_read(struct lcd_controller*, u16);
void lcd_vram_write(struct lcd_controller*, u16, u8);
u8 lcd_reg_read(struct lcd_controller*, enum lcd_reg);
void lcd_reg_write(struct lcd_controller*, enum lcd_reg, u8);

#endif
