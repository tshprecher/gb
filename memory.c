#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "memory.h"
#include "display.h"
#include "sound.h"

enum sound_reg map_index_to_sound_reg[] = {
  rNR10, rNR11, rNR12, rNR13, rNR14,
  rNR21, rNR22, rNR23, rNR24,
  rNR30, rNR31, rNR32, rNR33, rNR34,
  rNR41, rNR42, rNR43, rNR44,
  rNR50, rNR51, rNR52
};

// TODO: handle interrupt
void port_tick(struct port_controller *pc) {
  if (pc->t_cycles_to_read)
    pc->t_cycles_to_read--;
}

void port_btn_press(struct port_controller *pc, enum btn btn) {
  pc->btns_pressed |= (1 << btn);
}

void port_btn_unpress(struct port_controller *pc, enum btn btn) {
  pc->btns_pressed &= ~(1 << btn);
}

void port_write_P1(struct port_controller *pc, uint8_t value) {
  switch (value) {
  case 0x10:
    pc->status = value;
    //    pc->t_cycles_to_read = 4;
    break;
  case 0x20:
    pc->status = value;
    //    pc->t_cycles_to_read = 16;
    break;
  case 0x30:
    pc->status = value;
    pc->t_cycles_to_read = 0;
    break;
  default:
    fprintf(stderr, "error: no-op, writing value 0x%02x to reg $P1", value);
  }
}

int port_read_P1(struct port_controller *pc, uint8_t *result) {
  if (pc->status == 0x30)
    return 0;
  if (pc->t_cycles_to_read)
    return 0;

  if (pc->status == 0x10) {
    *result = (~pc->btns_pressed >> 4) & 0x0F;
  } else if (pc->status == 0x20) {
    *result = (~pc->btns_pressed) & 0x0F;
  }
  return 1;
}


// TODO: rename this whole module, perhaps to memory.c

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

// TODO: handle banking
static struct inst cached_insts[0x10000];
static uint8_t cached_bitmap[0x10000 >> 3];

uint8_t mem_read(struct mem_controller * mc, uint16_t addr) {
  // TODO: this looks uglier than necessary. consolidate into one switch?
  if (addr == 0xFF00) {
    uint8_t ports = 0;
    port_read_P1(mc->pc, &ports);
    return ports;
  } else if (addr >= 0xFF04 && addr <= 0xFF07) {
    //    printf("DEBUG: reading timer register @ 0x%04X\n", addr);
    switch (addr - 0xFF04) {
    case 0:
      //printf("DEBUG: read DIV -> 0x%02X\n", (uint8_t) ((mc->tc->div_t_cycles >> 9) & 0xFF));
      return (uint8_t) ((mc->tc->div_t_cycles >> 9) & 0xFF);
    case 1:
      return mc->tc->TIMA;
    case 2:
      return mc->tc->TMA;
    case 3:
      return mc->tc->TAC;
    };
  }else if (addr == 0xFF0F) {
    //printf("DEBUG: reading interrupt register IF\n");
    return mc->ic->IF;
  } else if (addr == 0xFFFF) {
    //printf("DEBUG: reading interrupt register IE\n");
    return mc->ic->IE;
  } else if (addr >= 0xFF40 && addr <= 0xFF4B) {
    //printf("DEBUG: reading lcd register @ 0x%04X\n", addr);
    switch (addr-0xFF40) {
    case 0:
      return mc->lcd->LCDC;
    case 1:
      return mc->lcd->STAT;
    case 2:
      return mc->lcd->SCY;
    case 3:
      return mc->lcd->SCX;
    case 4:
      return mc->lcd->LY;
    case 5:
      return mc->lcd->LYC;
    case 6:
      return mc->lcd->DMA;
    case 7:
      return mc->lcd->BGP;
    case 8:
      return mc->lcd->OBP0;
    case 9:
      return mc->lcd->OBP1;
    case 0xA:
      return mc->lcd->WY;
    case 0xB:
      return mc->lcd->WX;
    };
  } else if (addr >= 0xFF10 && addr <= 0xFF26 &&
	     addr != 0xFF15 && addr != 0xFF1F) {
    int reg_index = addr - 0xFF10;
    if (addr > 0xFF15) { // first gap
      reg_index--;
    }
    if (addr > 0xFF1F) { // second gap
      reg_index--;
    }
    //printf("DEBUG: reading sound register @ 0x%04X\n", addr);
    return audio_read_reg(mc->sc,  map_index_to_sound_reg[reg_index]);
  }
  return mc->ram[addr];
}

void mem_write(struct mem_controller *mc, uint16_t addr, uint8_t byte) {
  if (addr < 0x8000) {
    return;
  } else if (addr == 0xFF00) { // P1 register
    port_write_P1(mc->pc, byte);
  } else if (addr >= 0xFF04 && addr <= 0xFF07) {
    //printf("DEBUG: writing 0x%02X to timer register @ 0x%04X\n", byte, addr);
    switch (addr - 0xFF04) {
    case 0:
      // TODO: delegate to underlying controller is better
      mc->tc->div_t_cycles = 0;
    case 1:
      mc->tc->TIMA = byte;
    case 2:
      mc->tc->TMA = byte;
    case 3:
      //printf("DEBUG: writing TAC: 0x%02X\n", byte);
      mc->tc->TAC = byte;
    };
  } else if (addr == 0xFF0F) {
    //printf("DEBUG: writing 0x%02X to interrupt register IF\n", byte);
    mc->ic->IF = byte;
  } else if (addr == 0xFFFF) {
    //printf("DEBUG: writing 0x%02X to interrupt register IE\n", byte);
    mc->ic->IE = byte;
  } else if (addr >= 0xFF40 && addr <= 0xFF4B) {
    //printf("DEBUG: writing 0x%02X to lcd register @ 0x%04X\n", byte, addr);
    // TODO: handle the permissioning here so no improper writes occur
    switch (addr-0xFF40) {
    case 0:
      lcd_LCDC_write(mc->lcd, byte);
      break;
    case 1:
      mc->lcd->STAT = byte;
      break;
    case 2:
      mc->lcd->SCY = byte;
      break;
    case 3:
      mc->lcd->SCX = byte;
      break;
    case 5:
      mc->lcd->LYC = byte;
      break;
    case 6:
      for (int b = 0; b <= 0x9F; b++) {
	mem_write(mc, 0xFE00 + b, mem_read(mc, (byte<<8)+b));
      }
      //printf("DEBUG: executed DMA transfer from 0x%02X00\n", byte);
      mc->lcd->DMA = byte;
      break;
    case 7:
      mc->lcd->BGP = byte;
      break;
    case 8:
      mc->lcd->OBP0 = byte;
      break;
    case 9:
      mc->lcd->OBP1 = byte;
      break;
    case 0xA:
      mc->lcd->WY = byte;
      break;
    case 0xB:
      mc->lcd->WX = byte;
      break;
    };
  } else if (addr >= 0xFF10 && addr <= 0xFF26 &&
	     addr != 0xFF15 && addr != 0xFF1F) {
    int reg_index = addr - 0xFF10;
    if (addr > 0xFF15) { // first gap
      reg_index--;
    }
    if (addr > 0xFF1F) { // second gap
      reg_index--;
    }
    //printf("DEBUG: writing 0x%02X to sound register 0x%04X, reg_index -> %d\n", byte, addr, reg_index);
    audio_write_reg(mc->sc,  map_index_to_sound_reg[reg_index], byte);
  } else {
    mc->ram[addr] = byte;
  }
}

struct inst* mem_read_inst(struct mem_controller *mc, uint16_t addr) {
  uint8_t byte_map = cached_bitmap[addr >> 3];
  uint8_t bit_mask = 1 << (7-(addr & 7));
  if (!(byte_map & bit_mask)) {
    int success = init_inst_from_bytes(&cached_insts[addr], &mc->ram[addr]);
    if (!success)
      return NULL;
    byte_map |= bit_mask;
    cached_bitmap[addr >> 3] = byte_map;
  }
  return &cached_insts[addr];
}

uint8_t* mem_ptr(struct mem_controller *mc, uint16_t addr) {
  return &mc->ram[addr];
}
