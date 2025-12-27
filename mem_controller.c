#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "mem_controller.h"
#include "lcd.h"

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
  }

  return NULL;
}

// TODO: handle banking
static struct inst cached_insts[0x10000];
static uint8_t cached_bitmap[0x10000 >> 3];

uint8_t mem_read(struct mem_controller * mc, uint16_t addr) {
  // TODO: this looks uglier than necessary. consolidate into one switch
  if (addr >= 0xFF04 && addr <= 0xFF07) {
    printf("DEBUG: writing timer register @ 0x%04X\n", addr);
    switch (addr - 0xFF04) {
    case 0:
      return mc->tc->DIV;
    case 1:
      return mc->tc->TIMA;
    case 2:
      return mc->tc->TMA;
    case 3:
      return mc->tc->TAC;
    };
  }else if (addr == 0xFF0F) {
    printf("DEBUG: reading interrupt register IF\n");
    return mc->ic->IF;
  } else if (addr == 0xFFFF) {
    printf("DEBUG: reading interrupt register IE\n");
    return mc->ic->IE;
  } else if (addr >= 0xFF40 && addr <= 0xFF4B) {
    printf("DEBUG: reading lcd register @ 0x%04X\n", addr);
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
   }

   return mc->ram[addr];
}

void mem_write(struct mem_controller *mc, uint16_t addr, uint8_t byte) {
  if (addr < 0x8000) {
    return;
  } else if (addr == 0xFF00) { // P1 register
    return;
  } else if (addr >= 0xFF04 && addr <= 0xFF07) {
    printf("DEBUG: writing 0x%02X to timer register @ 0x%04X\n", byte, addr);
    switch (addr - 0xFF04) {
    case 0:
      // TODO: delegate to underlying controller is better
      mc->tc->div_t_cycles = 0;
      mc->tc->DIV = 0;
    case 1:
      mc->tc->TIMA = byte;
    case 2:
      mc->tc->TMA = byte;
    case 3:
      mc->tc->TAC = byte;
    };
  } else if (addr == 0xFF0F) {
    printf("DEBUG: writing 0x%02X to interrupt register IF\n", byte);
    mc->ic->IF = byte;
  } else if (addr == 0xFFFF) {
    printf("DEBUG: writing 0x%02X to interrupt register IE\n", byte);
    mc->ic->IE = byte;
  } else if (addr >= 0xFF40 && addr <= 0xFF4B) {
    printf("DEBUG: writing 0x%02X to lcd register @ 0x%04X\n", byte, addr);
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
  } else {
    if (addr == 0xDFE9)
      printf("DEBUG: writing value 0x%02X to  0xDFE9\n", byte);
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
