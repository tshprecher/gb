#include <stdio.h>
#include <stdint.h>
#include "mem_controller.h"


uint8_t mem_read(struct mem_controller * mc, uint16_t addr) {
  return mc->ram[addr];
}

void mem_write(struct mem_controller *mc, uint16_t addr, uint8_t byte) {
  mc->ram[addr] = byte;
}

uint8_t* mem_ptr(struct mem_controller *mc, uint16_t addr) {
  return &mc->ram[addr];
}
