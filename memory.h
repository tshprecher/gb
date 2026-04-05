#ifndef MEMORY_H
#define MEMORY_H

#include "cpu.h"
#include "display.h"
#include "inst.h"
#include "timing.h"

char * mmapped_reg_to_str(u16);

struct rom {
  u8 mbc_type;
  u8 num_banks;
  u8 *mem;

  // cache for decoded instructions
  struct inst *cached_insts;
  u8 *is_cached_bitmap;
};

struct rom load_rom(char *);
u8 rom_read(struct rom *, u16);
void rom_write(struct rom *, u16, u8); // used by some memory banked controllers (MBCs)

struct mem_controller {
  // general memory space mapping
  struct rom *rom; // lower 32K is rom
  u8 ram[0x8000]; // upper 32K is ram (including VRAM, memory mapped registers)

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
