#ifndef DISPLAY_H
#define DISPLAY_H

#include "cpu.h"
#include "memory.h"
#include "types.h"

void global_init_lcd();

enum lcd_reg {
  rLCDC = 0,
  rSTAT,
  rSCY,
  rSCX,
  rLY,
  rLYC,
  rDMA,
  rBGP,
  rOBP0,
  rOBP1,
  rWY,
  rWX
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

  u32 t_cycles_since_last_line_refresh;  // TODO: rename

  u8 bg[256][256];
  u8 wdw[256][256];  // TODO: revisit resizing to 144x160

  struct oam oam;
  struct interrupt_controller* interrupt_c;
};

void lcd_tick(struct lcd_controller*);

u8 lcd_read_vram(struct lcd_controller*, u16);
void lcd_write_vram(struct lcd_controller*, u16, u8);
u8 lcd_read_reg(struct lcd_controller*, enum lcd_reg);
void lcd_write_reg(struct lcd_controller*, enum lcd_reg, u8);

#endif
