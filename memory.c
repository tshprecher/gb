#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "input.h"
#include "inst.h"
#include "memory.h"
#include "display.h"
#include "sound.h"

/**
 * GAMEPAK
 */
struct gamepak load_gamepak(char * filename) {
  printf("info: loading gamepak '%s'...\n", filename);
  FILE *fin = fopen(filename, "rb");
  if (NULL == fin)
    {
      printf("error: opening file: %s\n", filename);
      exit(1);
    }

  struct gamepak gpk = {0};
  gpk.num_banks = 1;
  int rom_size = 0x4000*(gpk.num_banks+1);
  gpk.rom = malloc(rom_size);
  gpk.cached_insts = (struct inst*) malloc(rom_size * sizeof(struct inst));
  gpk.is_cached_bitmap = (u8*) malloc(rom_size >> 3);

  char b;
  int count = 0;
  while (count < rom_size) {
    size_t c = fread(&b, 1, 1, fin);
    if (c == 0)
      break;
    gpk.rom[count++] = b;
  }
  if (count != rom_size) {
    printf("error: reading gamepak: expected 0x%04X bytes, read 0x%04X", rom_size, count);
    exit(1);
  }
  printf("info: rom type: 0x%02X\n", gpk.rom[0x147]);
  printf("info: gamepak loaded.\n");
  fclose(fin);
  return gpk;
}

struct inst * gpk_read_inst(struct gamepak * gpk, u16 addr) {
  if (addr < 0x8000) {
    if (addr >= 0x4000) {
      addr = gpk->cur_bank*0x4000+addr;
    }

    u8 byte_map = gpk->is_cached_bitmap[addr >> 3];
    u8 bit_mask = 1 << (addr & 7);
    if (!(byte_map & bit_mask)) {
      if (!init_inst_from_bytes(&gpk->cached_insts[addr], &gpk->rom[addr]))
	return NULL;
      byte_map |= bit_mask;
      gpk->is_cached_bitmap[addr >> 3] = byte_map;
    }
    return &gpk->cached_insts[addr];
  }
  return NULL;
}

u8 gpk_read(struct gamepak * gpk, u16 addr) {
  return gpk->rom[addr];
}

void gpk_write(struct gamepak *gpk, u16 addr, u8 value) {
  // TODO: implement for later MBCs
  return;
}

/**
 * MEMORY CONTROLLER
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

char * mmapped_reg_to_str(u16 addr) {
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

u8 mem_read(struct mem_controller * mc, u16 addr) {
  if (addr < 0x8000) { // route to rom
    return gpk_read(mc->gpk, addr);
  } else if (addr < 0xA000 || (addr >= 0xFE00 && addr < 0xFEA0)) {
    return lcd_vram_read(mc->lcd_c, addr);
  } else if (addr == 0xFF00) { // check memory mapped registers
    u8 inputs = 0;
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
  } else if (addr >= 0xFF30 && addr <= 0xFF3F) {
    return sound_wram_read(mc->sound_c, addr);
  }

  // if not in rom, vram, or memory mapped address space, go to ram
  return mc->ram[addr-0xA000];
}

void mem_write(struct mem_controller *mc, u16 addr, u8 value) {
  if (addr < 0x8000) { // route to rom
    gpk_write(mc->gpk, addr, value);
  } else if (addr < 0xA000 || (addr >= 0xFE00 && addr < 0xFEA0)) {
    lcd_vram_write(mc->lcd_c, addr, value);
  } else if (addr == 0xFF00) { // check memory mapped registers
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
    if (addr-0xFF40 == rDMA) {
      for (int b = 0; b <= 0x9F; b++) {
	lcd_vram_write(mc->lcd_c, 0xFE00 + b, mem_read(mc, (value<<8)+b));
      }
    } else {
      lcd_reg_write(mc->lcd_c, map_index_to_lcd_reg[addr-0xFF40], value);
    }
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
  } else if (addr >= 0xFF30 && addr <= 0xFF3F) {
    sound_wram_write(mc->sound_c, addr, value);
  } else {
    // if not in rom or memory mapped address space, go to ram
    mc->ram[addr-0xA000] = value;
  }
}

struct inst* mem_read_inst(struct mem_controller *mc, u16 addr) {
  if (addr < 0x8000) {
    return gpk_read_inst(mc->gpk, addr);
  } else {
    // TODO: why does this branch exist again? Explain here
    init_inst_from_bytes(&mc->_inst_in_ram, &mc->ram[addr-0xA000]);
    return &mc->_inst_in_ram;
  }
}
