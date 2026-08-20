#ifndef MEMORY_H
#define MEMORY_H

#include "cpu.h"
#include "display.h"
#include "inst.h"
#include "sound.h"
#include "timing.h"

char * mmapped_reg_to_str(u16);

struct gamepak {
  u8 mbc_type;
  u8 mbc_regs[4];

  int rom_size;
  int ram_size;

  // stores all the rom starting with 16K of residence rom
  // followed by 16K for each bank of rom.
  u8 *rom;
  u8 *rom_bank;

  u8 *ram;
  u8 *ram_bank;

  struct inst *cached_insts;  // cache decoded rom instructions

  // used as the location for return values to gpk_read_inst
  // where the address is in writable ram.
  // TODO: consolidate with mc's version?
  struct inst _inst_in_ram;
};

void init_gamepak(struct gamepak *, char *);
struct inst * gpk_read_inst(struct gamepak *, u16);
u8 gpk_read(struct gamepak *, u16);
void gpk_write(struct gamepak *, u16, u8); // used by MBCs

struct mem_controller {
  u8 ram[0x4000];
  struct gamepak *gpk;

  // memory mapped registers, vram ops routed to controllers
  struct interrupt_controller *interrupt_c;
  struct lcd_controller *lcd_c;
  struct timing_controller *timing_c;
  struct input_controller *input_c;
  struct sound_controller *sound_c;

  // used as the location for return values to mem_read_inst
  // where the address is in writable ram.
  struct inst _inst_in_ram;
};

u8 mem_read(struct mem_controller *, u16);
void mem_write(struct mem_controller *, u16, u8);
struct inst* mem_read_inst(struct mem_controller *, u16);

#endif
