#ifndef MEMORY_H
#define MEMORY_H

#include "cpu.h"
#include "display.h"
#include "inst.h"
#include "timing.h"

char * mmapped_reg_to_str(uint16_t);

struct rom {
  uint8_t mbc_type;
  uint8_t num_banks;
  uint8_t *mem;

  // cache for decoded instructions
  struct inst *cached_insts;
  uint8_t *is_cached_bitmap;
};

struct rom load_rom(char *);
uint8_t rom_read(struct rom *, uint16_t);
void rom_write(struct rom *, uint16_t, uint8_t); // used by some memory banked controllers (MBCs)

struct mem_controller {
  // general memory space mapping
  struct rom *rom; // lower 32K is rom
  uint8_t ram[0x8000]; // upper 32K is ram (including VRAM, memory mapped registers)

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

uint8_t mem_read(struct mem_controller *, uint16_t);
void mem_write(struct mem_controller *, uint16_t, uint8_t);
struct inst* mem_read_inst(struct mem_controller *, uint16_t);

// TODO: remove this to make sure all write go through mem_write
uint8_t* mem_ptr(struct mem_controller *, uint16_t);

#endif
