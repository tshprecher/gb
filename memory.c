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

#define TYPES_LEN 4

// TODO: fill in
static u8 gpk_types[][4] = {
  //type, mbc, SRAM, backup battery
  {0, 0, 0, 0},
  {1, 1, 0, 0},
  {2, 1, 1, 0},
  {3, 1, 1, 1},
};

void init_gamepak(struct gamepak *gpk, char * filename) {
  printf("info: loading gamepak '%s'...\n", filename);
  FILE *fin = fopen(filename, "rb");
  if (NULL == fin)
    {
      printf("error: opening file: %s\n", filename);
      exit(1);
    }
  u8 c;

  // read metadata
  if (fseek(fin, 0x147, SEEK_SET) || !fread(&c, 1, 1, fin))  {
    printf("error: loading gamepak: could not read type @ 0x147\n");
    exit(1);
  }

  if (c > 3) {
    printf("error: loading gamepak: unsuppported gamepak type %d\n", c);
    exit(1);
  }

  printf("info: gamepak type: 0x%02X\n", c);
  for (int t = 0; t < TYPES_LEN; t++) {
    if (gpk_types[t][0] == c) {
      printf("(debug): found type record {%d, %d, %d, %d}\n", gpk_types[t][0], gpk_types[t][1], gpk_types[t][2], gpk_types[t][3]);
      gpk->mbc = gpk_types[t][1];
    }
  }
  printf("info: gamepak mbc: %d\n", gpk->mbc);

  if (!fread(&c, 1, 1, fin))  {
    printf("error: loading gamepak: could not read ROM size @ 0x148\n");
    exit(1);
  }
  if (c > 8) {
    printf("error: loading gamepak: invalid ROM size code: %d\n", c);
    exit(1);
  }
  int rom_mapping[] = {0x8000, 0x10000, 0x20000, 0x40000,
		       0x80000, 0x100000, 0x200000, 0x400000, 0x800000};
  gpk->rom_size = rom_mapping[c];
  printf("info: gamepak ROM size: %d bytes\n", gpk->rom_size);

  if (!fread(&c, 1, 1, fin))  {
    printf("error: loading gamepak: could not read RAM size @ 0x149\n");
    exit(1);
  }
  if (c > 4) {
    printf("error: loading gamepak: invalid RAM size code: %d\n", c);
    exit(1);
  }
  int ram_mapping[] = {0, 0, 0x2000, 0x8000, 0x20000};
  gpk->ram_size = ram_mapping[c];
  printf("info: gamepak RAM size: %d bytes\n", gpk->ram_size);

  // build struct

  fseek(fin, 0, SEEK_SET);

  gpk->rom = malloc(gpk->rom_size);
  gpk->rom_bank = gpk->rom+0x4000;
  gpk->rom_bank_id = 1;
  gpk->ram = malloc(gpk->ram_size);
  gpk->cached_insts = (struct inst*) malloc(gpk->rom_size * sizeof(struct inst));
  gpk->is_cached_bitmap = (u8*) malloc(gpk->rom_size >> 3);

  int count = 0;
  while (count < gpk->rom_size) {
    if (!fread(&c, 1, 1, fin))
      break;
    gpk->rom[count++] = c;
  }
  if (count != gpk->rom_size) {
    printf("error: loading gamepak: expected 0x%04X bytes, read 0x%04X", gpk->rom_size, count);
    exit(1);
  }

  printf("info: gamepak loaded, read %d bytes.\n", count);
  fclose(fin);
}

struct inst * gpk_read_inst(struct gamepak * gpk, u16 addr) {
  // TODO: handle unusual case where instruction may cut across banks
  if (addr < 0x8000) {
    int linear_addr = addr < 0x4000 ? addr : (gpk->rom_bank_id-1)*0x4000+addr;
    u8 byte_map = gpk->is_cached_bitmap[linear_addr >> 3];
    u8 bit_mask = 1 << (linear_addr & 7);
    if (!(byte_map & bit_mask)) {
      u8 *base = addr < 0x4000 ? gpk->rom : gpk->rom_bank;
      addr = addr < 0x4000 ? addr : addr-0x4000;
      if (!init_inst_from_bytes(&gpk->cached_insts[linear_addr], &base[addr]))
	return NULL;
      byte_map |= bit_mask;
      gpk->is_cached_bitmap[linear_addr >> 3] = byte_map;
    }
    return &gpk->cached_insts[linear_addr];
  } else {
    // TODO: consolidate logic with mem_read_inst
    init_inst_from_bytes(&gpk->_inst_in_ram, &gpk->ram[addr-0xA000]);
    return &gpk->_inst_in_ram;
  }
}

u8 gpk_read(struct gamepak * gpk, u16 addr) {
  switch (gpk->mbc) {
  case 1:
    return addr < 0x4000 ? gpk->rom[addr] : gpk->rom_bank[addr-0x4000];
  default:
    return gpk->rom[addr];
  }
}

void gpk_write(struct gamepak *gpk, u16 addr, u8 value) {
  // TODO: implement for later MBCs
  switch (gpk->mbc) {
  case 1:
    if (addr >= 0x2000 && addr < 0x4000) {
      printf("(debug): writing MBC reg 1:: 0x%02X\n", value);
      gpk->rom_bank_id = value;
      gpk->rom_bank = gpk->rom+(0x4000*gpk->rom_bank_id);
    } else if (addr >= 0x0000 && addr < 0x1FFF) {
      printf("(debug): writing MBC reg 0: 0x%02X\n", value);
    } else if (addr >= 0x4000 && addr < 0x5FFF) {
      printf("(debug): writing MBC reg 2: 0x%02X\n", value);
    } else if (addr >= 0x6000 && addr < 0x7FFF) {
      printf("(debug): writing MBC reg 3: 0x%02X\n", value);
    }
    break;
  default:
    break;
  }
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
  if (addr < 0x8000 || (addr >= 0xA000 && addr < 0xC000)) { // route to gamepak
    return gpk_read(mc->gpk, addr);
  } else if (addr < 0xA000 || (addr >= 0xFE00 && addr < 0xFEA0)) { // route to bg or oam vram
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
  return mc->ram[addr-0xC000];
}

void mem_write(struct mem_controller *mc, u16 addr, u8 value) {
  if (addr < 0x8000 || (addr >= 0xA000 && addr < 0xC000)) { // route to gamepak
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
    mc->ram[addr-0xC000] = value;
  }
}

struct inst* mem_read_inst(struct mem_controller *mc, u16 addr) {
  // TODO: this doesn't technically allow for instructions read in VRAM
  if (addr < 0x8000 || (addr >= 0xA000 && addr < 0xC000)) { // route to gamepak
    return gpk_read_inst(mc->gpk, addr);
  } else {
    // TODO: why does this branch exist again? Explain here
    init_inst_from_bytes(&mc->_inst_in_ram, &mc->ram[addr-0xC000]);
    return &mc->_inst_in_ram;
  }
}
