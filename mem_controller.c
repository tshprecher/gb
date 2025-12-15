#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "mem_controller.h"


uint8_t mem_read(struct mem_controller * mc, uint16_t addr) {
  return mc->ram[addr];
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
