#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "mem_controller.h"

// TODO: handle banking
static struct inst cached_insts[0x10000];
static uint8_t cached_bitmap[0x10000 >> 3];

uint8_t mem_read(struct mem_controller * mc, uint16_t addr) {
  return mc->ram[addr];
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

void mem_write(struct mem_controller *mc, uint16_t addr, uint8_t byte) {
  if (addr < 0x8000) {
    return;
    //printf("ERROR: writing value 0x%02X to ROM address 0x%04X\n", byte, addr);
    //    exit(1);
  } else {
    mc->ram[addr] = byte;
  }
}

uint8_t* mem_ptr(struct mem_controller *mc, uint16_t addr) {
  return &mc->ram[addr];
}
