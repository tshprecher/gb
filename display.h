#ifndef DISPLAY_H
#define DISPLAY_H

#include "types.h"
#include "cpu.h"
#include "memory.h"

#define is_bit_set(v, b) ((v >> b) & 1)
#define is_bit_eq(v1, v2, b) ((v1 >> b) == (v2 >> b))

enum lcd_reg {
  rLCDC = 0, rSTAT, rSCY, rSCX, rLY, rLYC, rDMA,
  rBGP, rOBP0, rOBP1, rWY, rWX
};

struct lcd_controller {
  u8 vram[0x2000 /* bg/char data*/ + 0xA0 /* oam data */];

  // registers
  u8 regs[12];

  u32 t_cycles_since_last_line_refresh; // TODO: rename

  u8 bg[256][256];
  u8 wdw[256][256]; // TODO: revisit resizing to 144x160

  // compact format for oam objects
  u8 oam[1 + // num active columns
	  2*40 + // 2 bytes overhead per column (x, # elements), max 40
	  40*4 // 4 bytes per (id, y, chr_code, attr) tuple, max 40 tuples
	  ];

  struct interrupt_controller *interrupt_c;
};

void init_lcd();
void lcd_tick(struct lcd_controller *);

u8 lcd_vram_read(struct lcd_controller*, u16);
void lcd_vram_write(struct lcd_controller*, u16, u8);
u8 lcd_reg_read(struct lcd_controller*, enum lcd_reg);
void lcd_reg_write(struct lcd_controller*, enum lcd_reg, u8);

#endif
