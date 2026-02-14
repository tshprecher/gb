#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "input.h"
#include "inst.h"
#include "memory.h"
#include "display.h"
#include "sound.h"


/**
 * ROM FUNCTIONS
 */

struct rom load_rom(char *filename)
{
  printf("info: loading rom '%s'...\n", filename);
  FILE *fin = fopen(filename, "rb");
  if (NULL == fin)
    {
      printf("error: opening file: %s\n", filename);
      exit(1);
    }

  struct rom rom = {0};
  rom.num_banks = 1;
  rom.mem = (uint8_t*) malloc(0x8000);
  rom.cached_insts = (struct inst*) malloc(0x8000 * sizeof(struct inst));
  rom.is_cached_bitmap = (uint8_t*) malloc(0x8000 >> 3);
  char b;
  int addr = 0;
  while (addr < 0x8000)
    {
      size_t c = fread(&b, 1, 1, fin);
      if (c == 0)
	break;
      rom.mem[addr++] = b;
    }
  if (addr != 0x8000)
    {
      printf("error: reading ROM: cannot read full 32K of ROM: %x\n", addr);
      exit(1);
    }
  printf("info: rom loaded.\n");
  fclose(fin);
  return rom;
}

uint8_t rom_read(struct rom * rom, uint16_t addr) {
  return rom->mem[addr];
};

void rom_write(struct rom * rom, uint16_t addr, uint8_t value) {
  // TODO: implement for later MBCs
  return;
};

/**
 * MEMORY CONTROLLER DATA AND FUNCTIONS
 */

static enum sound_reg map_index_to_sound_reg[] = {
  rNR10, rNR11, rNR12, rNR13, rNR14,
  rNR21, rNR22, rNR23, rNR24,
  rNR30, rNR31, rNR32, rNR33, rNR34,
  rNR41, rNR42, rNR43, rNR44,
  rNR50, rNR51, rNR52
};

static enum lcd_reg map_index_to_lcd_reg[] = {
  rLCDC, rSTAT, rSCY, rSCX, rLY, rLYC, rDMA, rBGP,
  rOBP0, rOBP1, rWY, rWX
};

static enum timing_reg map_index_to_timing_reg[] = {
  rDIV, rTIMA, rTMA, rTAC
};

// DO NOT CHANGE ORDER WITHOUT CHANGING mem_read() AND mem_write()
static char* mmapped_registers[] = {
  // 0xFE00
  "$OAM",

  // [0xFF00, 0xFF02]
  "$P1",
  "$SB",
  "$SC",

  // [0xFF04, 0xFF07]
  "$DIV",
  "$TIMA",
  "$TMA",
  "$TAC",

  // 0xFF0F
  "$IF",
  // 0xFFFF
  "$IE",

  // [0xFF40, 0xFF4B]
  "$LCDC",
  "$STAT",
  "$SCY",
  "$SCX",
  "$LY",
  "$LYC",
  "$DMA",
  "$BGP",
  "$OBP0",
  "$OBP1",
  "$WY",
  "$WX",

  // [0xFF10, 0xFF26]
  "$NR10",
  "$NR11",
  "$NR12",
  "$NR13",
  "$NR14",
  "_err_NR15", // does not exist, 0xFF15 isn't allocated

  "$NR21",
  "$NR22",
  "$NR23",
  "$NR24",

  "$NR30",
  "$NR31",
  "$NR32",
  "$NR33",
  "$NR34",
  "_err_NR35", // does not exist, 0xFF1F isn't allocated, TODO: find out why it's used

  "$NR41",
  "$NR42",
  "$NR43",
  "$NR44",

  "$NR50",
  "$NR51",
  "$NR52",
};

char * mmapped_reg_to_str(uint16_t addr) {
  // fail fast in the commmon case
  if (addr < 0xFE00)
    return NULL;

  if (addr >= 0xFF00 && addr <= 0xFF02) {
    return mmapped_registers[addr-0xFF00+1];
  } else if (addr >= 0xFF04 && addr <= 0xFF07) {
    return mmapped_registers[addr-0xFF04+4];
  } else if (addr == 0xFF0F) {
    return "$IF";
  } else if (addr == 0xFFFF) {
    return "$IE";
  } else if (addr >= 0xFF40 && addr <= 0xFF4B) {
    return mmapped_registers[addr-0xFF40+10];
  } else if (addr >= 0xFF10 && addr <= 0xFF26) {
    return mmapped_registers[addr-0xFF10+22];
  }

  return NULL;
}

uint8_t mem_read(struct mem_controller * mc, uint16_t addr) {
  if (addr < 0x8000) { // route to rom
    return rom_read(mc->rom, addr);
  } else if (addr == 0xFF00) { // try memory mapped registers
    uint8_t inputs = 0;
    input_read_P1(mc->input_c, &inputs);
    return inputs;
  } else if (addr >= 0xFF04 && addr <= 0xFF07) {
    int reg_index = addr - 0xFF04;
    return timing_reg_read(mc->timing_c, map_index_to_timing_reg[reg_index]);
  } else if (addr == 0xFF0F) {
    return mc->interrupt_c->IF;
  } else if (addr == 0xFFFF) {
    return mc->interrupt_c->IE;
  } else if (addr >= 0xFF40 && addr <= 0xFF4B) {
    return lcd_reg_read(mc->lcd_c, map_index_to_lcd_reg[addr-0xFF40]);
  } else if (addr >= 0xFF10 && addr <= 0xFF26 &&
	     addr != 0xFF15 && addr != 0xFF1F) {
    int reg_index = addr - 0xFF10;
    if (addr > 0xFF15) { // first gap
      reg_index--;
    }
    if (addr > 0xFF1F) { // second gap
      reg_index--;
    }
    return sound_reg_read(mc->sound_c,  map_index_to_sound_reg[reg_index]);
  }

  // if not in rom or memory mapped address space, go to ram
  return mc->ram[addr-0x8000];
}

void mem_write(struct mem_controller *mc, uint16_t addr, uint8_t value) {
  if (addr < 0x8000) { // route to rom
    rom_write(mc->rom, addr, value);
  } else if (addr == 0xFF00) { // try memory mapped registers
    input_write_P1(mc->input_c, value);
  } else if (addr >= 0xFF04 && addr <= 0xFF07) {
    int reg_index = addr - 0xFF04;
    timing_reg_write(mc->timing_c,  map_index_to_timing_reg[reg_index], value);
  } else if (addr == 0xFF0F) {
    mc->interrupt_c->IF = value;
  } else if (addr == 0xFFFF) {
    mc->interrupt_c->IE = value;
  } else if (addr >= 0xFF40 && addr <= 0xFF4B) {
    // TODO: handle the permissioning here so no improper writes occur
    lcd_reg_write(mc->lcd_c, map_index_to_lcd_reg[addr-0xFF40], value);
  } else if (addr >= 0xFF10 && addr <= 0xFF26 &&
	     addr != 0xFF15 && addr != 0xFF1F) {
    int reg_index = addr - 0xFF10;
    if (addr > 0xFF15) { // first gap
      reg_index--;
    }
    if (addr > 0xFF1F) { // second gap
      reg_index--;
    }
    sound_reg_write(mc->sound_c,  map_index_to_sound_reg[reg_index], value);
  } else {
    // if not in rom or memory mapped address space, go to ram
    mc->ram[addr-0x8000] = value;
  }
}

struct inst* mem_read_inst(struct mem_controller *mc, uint16_t addr) {
  if (addr < 0x8000) {
    uint8_t byte_map = mc->rom->is_cached_bitmap[addr >> 3];
    uint8_t bit_mask = 1 << (7-(addr & 7));
    if (!(byte_map & bit_mask)) {
      int ok = init_inst_from_bytes(&mc->rom->cached_insts[addr], &mc->rom->mem[addr]);
      if (!ok)
	return NULL;
      byte_map |= bit_mask;
      mc->rom->is_cached_bitmap[addr >> 3] = byte_map;
    }
    return &mc->rom->cached_insts[addr];
  } else {
    init_inst_from_bytes(&mc->_inst_in_ram, &mc->ram[addr-0x8000]);
    return &mc->_inst_in_ram;
  }
}

uint8_t* mem_ptr(struct mem_controller *mc, uint16_t addr) {
  if (addr < 0x8000) {
    printf("error: returning a memory pointer outside of ram with addr: 0x%04X\n", addr);
  }
  return &mc->ram[addr-0x8000];
}
