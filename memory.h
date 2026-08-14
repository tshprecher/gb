#ifndef MEMORY_H
#define MEMORY_H

#include "cpu.h"
#include "display.h"
#include "inst.h"
#include "sound.h"
#include "timing.h"

char * mmapped_reg_to_str(u16);

struct gamepak {
  u8 mbc;
  int rom_size;
  int ram_size;

  u8 cur_bank;

  // stores all the rom starting with 16K of residence rom
  // followed by 16K for each bank of rom. therefore, the length is
  // 16K * (1 + num_banks)
  u8 *rom;
  // TODO: is rom_bank necessary?
  u8 *rom_bank;   // pointer to beginning of current 16K bank, mapped to address 0x4000

  // cache for decoded instructions
  struct inst *cached_insts; // length 16K * (num_banks + 1)
  u8 *is_cached_bitmap; // bit for each inst to see if already cached
};

void init_gamepak(struct gamepak *, char *);
struct inst * gpk_read_inst(struct gamepak *, u16);
u8 gpk_read(struct gamepak *, u16);
void gpk_write(struct gamepak *, u16, u8); // used by some memory banked controllers (MBCs)

struct mem_controller {
  struct gamepak *gpk; // rom
  u8 ram[0x10000-0xA000]; // ram, excluding vram

  // memory mapped registers routed to controllers
  struct interrupt_controller *interrupt_c;
  struct lcd_controller *lcd_c;
  struct timing_controller *timing_c;
  struct input_controller *input_c;
  struct sound_controller *sound_c;

  // used as the location for return values to mem_read_inst
  // where the address is in writable ram, not rom.
  struct inst _inst_in_ram;
};

u8 mem_read(struct mem_controller *, u16);
void mem_write(struct mem_controller *, u16, u8);
struct inst* mem_read_inst(struct mem_controller *, u16);

#endif
