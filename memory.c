#include "memory.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "display.h"
#include "input.h"
#include "inst.h"
#include "sound.h"

/**
 * GAMEPAK
 */

#define TYPES_LEN 4

// TODO: fill in
static u8 gpk_types[][4] = {
    // type, mbc, sram, backup battery
    {0, 0, 0, 0},
    {1, 1, 0, 0},
    {2, 1, 1, 0},
    {3, 1, 1, 1},
};

void init_gamepak(struct gamepak* gpk, char* filename) {
  printf("info: loading gamepak '%s'...\n", filename);
  FILE* fin = fopen(filename, "rb");
  if (NULL == fin) {
    printf("error: opening file: %s\n", filename);
    exit(1);
  }
  u8 c;

  // read and validate metadata
  if (fseek(fin, 0x143, SEEK_SET) || !fread(&c, 1, 1, fin)) {
    printf("error: loading gamepak: could not read support code @ 0x143\n");
    exit(1);
  }
  if (c == 0xC0) {
    printf(
        "error: loading gamepak: invalid support code 0x%02X. GBC games not "
        "supported.\n",
        c);
    exit(1);
  }
  if (fseek(fin, 0x147, SEEK_SET) || !fread(&c, 1, 1, fin)) {
    printf("error: loading gamepak: could not read type @ 0x147\n");
    exit(1);
  }
  if (c > 3) {
    printf("error: loading gamepak: unsuppported gamepak type %d\n", c);
    exit(1);
  }
  printf("info: gamepak type: 0x%02X\n", c);

  u8* gpk_type;
  for (int t = 0; t < TYPES_LEN; t++) {
    if (gpk_types[t][0] == c) {
      //      printf("(debug): found type record {%d, %d, %d, %d}\n",
      //      gpk_types[t][0], gpk_types[t][1], gpk_types[t][2],
      //      gpk_types[t][3]);
      gpk_type = gpk_types[t];
      gpk->mbc_type = gpk_type[1];
    }
  }
  printf("info: gamepak mbc: %d\n", gpk->mbc_type);

  if (!fread(&c, 1, 1, fin)) {
    printf("error: loading gamepak: could not read rom size @ 0x148\n");
    exit(1);
  }
  if (c > 8) {
    printf("error: loading gamepak: invalid rom size code: %d\n", c);
    exit(1);
  }
  int rom_mapping[] = {0x8000,   0x10000,  0x20000,  0x40000, 0x80000,
                       0x100000, 0x200000, 0x400000, 0x800000};
  gpk->rom_size = rom_mapping[c];
  printf("info: gamepak rom size: %d bytes\n", gpk->rom_size);

  if (!fread(&c, 1, 1, fin)) {
    printf("error: loading gamepak: could not read ram size @ 0x149\n");
    exit(1);
  }
  if (c > 4) {
    printf("error: loading gamepak: invalid ram size code: %d\n", c);
    exit(1);
  }
  int ram_mapping[] = {0, 0, 0x2000, 0x8000, 0x20000};
  gpk->ram_size = ram_mapping[c];
  printf("info: gamepak ram size: %d bytes\n", gpk->ram_size);

  // read rom into gamepak
  fseek(fin, 0, SEEK_SET);
  gpk->rom = malloc(gpk->rom_size);
  gpk->rom_bank = gpk->rom + 0x4000;
  gpk->mbc_regs[1] = 1;  // TODO: remove this, put in a mbc-related logic
  gpk->cached_insts = malloc(gpk->rom_size * sizeof(struct inst));
  memset(gpk->cached_insts, 0, gpk->rom_size * sizeof(struct inst));

  int count = 0;
  while (count < gpk->rom_size) {
    if (!fread(&c, 1, 1, fin)) break;
    gpk->rom[count++] = c;
  }
  if (count != gpk->rom_size) {
    printf("error: loading gamepak: expected 0x%04X bytes, read 0x%04X",
           gpk->rom_size, count);
    exit(1);
  }

  // initialize ram
  if (gpk->ram_size) {
    if (gpk_type[3]) {
      char sav_filename[40];
      strncpy(sav_filename, filename, 40);
      strncpy(sav_filename + strlen(sav_filename), ".sav", 4);

      int fd = open(sav_filename, O_RDWR | O_CREAT, 0666);
      if (fd == -1) {
        perror("error opening .sav file");
        exit(1);
      }
      if (ftruncate(fd, gpk->ram_size) == -1) {
        perror("error: truncating .sav file");
        exit(1);
      }
      gpk->ram = mmap(NULL, gpk->ram_size, PROT_READ | PROT_WRITE,
                      MAP_FILE | MAP_SHARED, fd, 0);
      if (gpk->ram == MAP_FAILED) {
        perror("error: memory mapping .sav file");
        exit(1);
      }
      printf("info: initialized persistent ram backed by %s\n", sav_filename);
    } else {
      gpk->ram = malloc(gpk->ram_size);
      printf("info: initialized ram\n");
    }
    gpk->ram_bank = gpk->ram;
  }
  fclose(fin);
  printf("info: gamepak loaded!\n");
}

struct inst* gpk_read_inst(struct gamepak* gpk, u16 addr) {
  // TODO: handle unusual case where instruction may cut across banks
  if (addr < 0x8000) {
    int linear_addr =
        addr < 0x4000 ? addr : (gpk->mbc_regs[1] - 1) * 0x4000 + addr;
    struct inst* cached_inst = &gpk->cached_insts[linear_addr];
    if (!cached_inst->type) {  // no type means not seen
      u8* base = addr < 0x4000 ? gpk->rom : gpk->rom_bank;
      addr = addr < 0x4000 ? addr : addr - 0x4000;
      if (!init_inst_from_bytes(cached_inst, &base[addr])) return NULL;
    }
    return cached_inst;
  } else {
    // TODO: consolidate logic with mem_read_inst
    init_inst_from_bytes(&gpk->_inst_in_ram, &gpk->ram[addr - 0xA000]);
    return &gpk->_inst_in_ram;
  }
}

u8 gpk_read(struct gamepak* gpk, u16 addr) {
  if (addr >= 0xA000) {  // ram
    return gpk->ram ? gpk->ram_bank[addr - 0xA000] : 0;
  } else if (addr < 0x8000) {
    switch (gpk->mbc_type) {  // rom
      case 1:
        return addr < 0x4000 ? gpk->rom[addr] : gpk->rom_bank[addr - 0x4000];
      default:
        return gpk->rom[addr];
    }
  }
  abort();  // improper memory routing
}

void gpk_write(struct gamepak* gpk, u16 addr, u8 value) {
  // TODO: implement for later MBCs
  if (addr >= 0xA000 && gpk->ram) {  // ram
    gpk->ram_bank[addr - 0xA000] = value;
    return;
  } else if (addr < 0x8000) {
    switch (gpk->mbc_type) {
      case 1:
        if (addr < 0x2000) {
          //	printf("(debug): writing MBC reg 0: 0x%02X\n", value);
          gpk->mbc_regs[0] = value;
        } else if (addr < 0x4000) {
          //	printf("(debug): writing MBC reg 1: 0x%02X\n", value);
          gpk->mbc_regs[1] = value;
          gpk->rom_bank = gpk->rom + (0x4000 * (gpk->mbc_regs[1]));
        } else if (addr < 0x6000) {
          //	printf("(debug): writing MBC reg 2: 0x%02X\n", value);
          gpk->mbc_regs[2] = value;
        } else if (addr < 0x8000) {
          //	printf("(debug): writing MBC reg 3: 0x%02X\n", value);
          gpk->mbc_regs[3] = value;
        }
        return;
      default:
        return;
    }
  }
  // NOTE: should abort but some games write to ram that doesn't exist (bug)
  return;
}

/**
 * MEMORY CONTROLLER
 */

void init_mem_controller(struct mem_controller* mc, struct gamepak* gpk,
                         struct input_controller* inp_c,
                         struct interrupt_controller* int_c,
                         struct lcd_controller* lc, struct sound_controller* sc,
                         struct timing_controller* tc) {
  mc->gpk = gpk;
  mc->input_c = inp_c;
  mc->interrupt_c = int_c;
  mc->lcd_c = lc;
  mc->sound_c = sc;
  mc->timing_c = tc;
}

static enum sound_reg map_index_to_sound_reg[] = {
    rNR10, rNR11, rNR12, rNR13, rNR14, rNR21, rNR22, rNR23, rNR24, rNR30, rNR31,
    rNR32, rNR33, rNR34, rNR41, rNR42, rNR43, rNR44, rNR50, rNR51, rNR52};

static enum lcd_reg map_index_to_lcd_reg[] = {
    rLCDC, rSTAT, rSCY, rSCX, rLY, rLYC, rDMA, rBGP, rOBP0, rOBP1, rWY, rWX};

static enum timing_reg map_index_to_timing_reg[] = {rDIV, rTIMA, rTMA, rTAC};

// DO NOT CHANGE ORDER WITHOUT CHANGING mem_read() AND
// mem_write()
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
    "_err_NR15",  // does not exist, 0xFF15 isn't
                  // allocated

    "$NR21",
    "$NR22",
    "$NR23",
    "$NR24",

    "$NR30",
    "$NR31",
    "$NR32",
    "$NR33",
    "$NR34",
    "_err_NR35",  // does not exist, 0xFF1F isn't
                  // allocated, TODO: find out why
                  // it's used

    "$NR41",
    "$NR42",
    "$NR43",
    "$NR44",

    "$NR50",
    "$NR51",
    "$NR52",
};

char* mmapped_reg_to_str(u16 addr) {
  // fail fast in the commmon case
  if (addr < 0xFE00) return NULL;

  if (addr >= 0xFF00 && addr <= 0xFF02) {
    return mmapped_registers[addr - 0xFF00 + 1];
  } else if (addr >= 0xFF04 && addr <= 0xFF07) {
    return mmapped_registers[addr - 0xFF04 + 4];
  } else if (addr == 0xFF0F) {
    return "$IF";
  } else if (addr == 0xFFFF) {
    return "$IE";
  } else if (addr >= 0xFF40 && addr <= 0xFF4B) {
    return mmapped_registers[addr - 0xFF40 + 10];
  } else if (addr >= 0xFF10 && addr <= 0xFF26) {
    return mmapped_registers[addr - 0xFF10 + 22];
  }

  return NULL;
}

u8 mem_read(struct mem_controller* mc, u16 addr) {
  // TODO: put the ram hot case first once serial port is implemented
  if (addr < 0x8000 || (addr >= 0xA000 && addr < 0xC000)) {  // route to gamepak
    return gpk_read(mc->gpk, addr);
  } else if (addr < 0xA000 ||
             (addr >= 0xFE00 && addr < 0xFEA0)) {  // route to bg or oam
                                                   // vram
    return lcd_read_vram(mc->lcd_c, addr);
  }

  // memory mapped registers over switch for perf instead of if/else ranges
  // (overkill?)
  switch (addr) {
    case 0xFF00: {
      u8 inputs = 0;
      input_read_P1(mc->input_c, &inputs);
      return inputs;
    }
    case 0xFF01:
    case 0xFF02:
      // TODO: handle serial port
      printf("debug(mem_read): reading serial port\n");
      fflush(stdout);
      return 0;
    case 0xFF04:
    case 0xFF05:
    case 0xFF06:
    case 0xFF07:
      return timing_read_reg(mc->timing_c,
                             map_index_to_timing_reg[addr - 0xFF04]);
    case 0xFF10:
    case 0xFF11:
    case 0xFF12:
    case 0xFF13:
    case 0xFF14:
      return sound_read_reg(mc->sound_c, map_index_to_sound_reg[addr - 0xFF10]);
      //    case 0xFF15: unused
    case 0xFF16:
    case 0xFF17:
    case 0xFF18:
    case 0xFF19:
    case 0xFF1A:
    case 0xFF1B:
    case 0xFF1C:
    case 0xFF1D:
    case 0xFF1E:
      return sound_read_reg(mc->sound_c,
                            map_index_to_sound_reg[addr - 0xFF10 - 1]);
      //    case 0xFF1F: unused

    case 0xFF20:
    case 0xFF21:
    case 0xFF22:
    case 0xFF23:
    case 0xFF24:
    case 0xFF25:
    case 0xFF26:
      return sound_read_reg(mc->sound_c,
                            map_index_to_sound_reg[addr - 0xFF10 - 2]);
    case 0xFF30:
    case 0xFF31:
    case 0xFF32:
    case 0xFF33:
    case 0xFF34:
    case 0xFF35:
    case 0xFF36:
    case 0xFF37:
    case 0xFF38:
    case 0xFF39:
    case 0xFF3A:
    case 0xFF3B:
    case 0xFF3C:
    case 0xFF3D:
    case 0xFF3E:
    case 0xFF3F:
      return sound_read_wram(mc->sound_c, addr);
    case 0xFF0F:
      return mc->interrupt_c->IF;
    case 0xFFFF:
      return mc->interrupt_c->IE;
    case 0xFF40:
    case 0xFF41:
    case 0xFF42:
    case 0xFF43:
    case 0xFF44:
    case 0xFF45:
    case 0xFF46:
    case 0xFF47:
    case 0xFF48:
    case 0xFF49:
    case 0xFF4A:
    case 0xFF4B:
      return lcd_read_reg(mc->lcd_c, map_index_to_lcd_reg[addr - 0xFF40]);
  }

  // if not in rom, vram, or memory mapped address
  // space, go to ram
  return mc->ram[addr - 0xC000];
}

void mem_write(struct mem_controller* mc, u16 addr, u8 value) {
  if (addr < 0x8000 || (addr >= 0xA000 && addr < 0xC000)) {  // route to gamepak
    gpk_write(mc->gpk, addr, value);
    return;
  } else if (addr < 0xA000 || (addr >= 0xFE00 && addr < 0xFEA0)) {
    lcd_write_vram(mc->lcd_c, addr, value);
    return;
  }

  // memory mapped registers over switch for perf instead of if/else ranges
  // (overkill?)
  switch (addr) {
    case 0xFF00: {
      input_write_P1(mc->input_c, value);
      return;
    }
    case 0xFF01:
    case 0xFF02:
      // TODO: handle serial port
      mc->ram[addr - 0xC000] = value;
      return;
    case 0xFF04:
    case 0xFF05:
    case 0xFF06:
    case 0xFF07:
      timing_write_reg(mc->timing_c, map_index_to_timing_reg[addr - 0xFF04],
                       value);
      return;
    case 0xFF10:
    case 0xFF11:
    case 0xFF12:
    case 0xFF13:
    case 0xFF14:
      sound_write_reg(mc->sound_c, map_index_to_sound_reg[addr - 0xFF10],
                      value);
      return;
    //    case 0xFF15: unused
    case 0xFF16:
    case 0xFF17:
    case 0xFF18:
    case 0xFF19:
    case 0xFF1A:
    case 0xFF1B:
    case 0xFF1C:
    case 0xFF1D:
    case 0xFF1E:
      sound_write_reg(mc->sound_c, map_index_to_sound_reg[addr - 0xFF10 - 1],
                      value);
      return;
    //    case 0xFF1F: unused
    case 0xFF20:
    case 0xFF21:
    case 0xFF22:
    case 0xFF23:
    case 0xFF24:
    case 0xFF25:
    case 0xFF26:
      sound_write_reg(mc->sound_c, map_index_to_sound_reg[addr - 0xFF10 - 2],
                      value);
      return;
    case 0xFF30:
    case 0xFF31:
    case 0xFF32:
    case 0xFF33:
    case 0xFF34:
    case 0xFF35:
    case 0xFF36:
    case 0xFF37:
    case 0xFF38:
    case 0xFF39:
    case 0xFF3A:
    case 0xFF3B:
    case 0xFF3C:
    case 0xFF3D:
    case 0xFF3E:
    case 0xFF3F:
      sound_write_wram(mc->sound_c, addr, value);
      return;
    case 0xFF0F:
      mc->interrupt_c->IF = value;
      return;
    case 0xFFFF:
      mc->interrupt_c->IE = value;
      return;
    case 0xFF40:
    case 0xFF41:
    case 0xFF42:
    case 0xFF43:
    case 0xFF44:
    case 0xFF45:
    case 0xFF46:
    case 0xFF47:
    case 0xFF48:
    case 0xFF49:
    case 0xFF4A:
    case 0xFF4B:
      // TODO: handle the permissioning here so no
      // improper writes occur
      if (addr - 0xFF40 == rDMA) {
        // TODO: put this in a case too
        for (int b = 0; b <= 0x9F; b++) {
          lcd_write_vram(mc->lcd_c, 0xFE00 + b, mem_read(mc, (value << 8) + b));
        }
      } else {
        lcd_write_reg(mc->lcd_c, map_index_to_lcd_reg[addr - 0xFF40], value);
      }
      return;
  }

  // if not in rom or memory mapped address space, go
  // to ram
  mc->ram[addr - 0xC000] = value;
}

struct inst* mem_read_inst(struct mem_controller* mc, u16 addr) {
  // TODO: this doesn't technically allow for
  // instructions read in vram
  if (addr < 0x8000 || (addr >= 0xA000 && addr < 0xC000)) {  // route to gamepak
    return gpk_read_inst(mc->gpk, addr);
  } else {
    // TODO: why does this branch exist again? Explain
    // here
    init_inst_from_bytes(&mc->_inst_in_ram, &mc->ram[addr - 0xC000]);
    return &mc->_inst_in_ram;
  }
}
