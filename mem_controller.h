#ifndef MEM_CONTROLLER_H
#define MEM_CONTROLLER_H

#include "inst.h"

struct mem_controller {
  uint8_t ram[0x10000];
};

struct inst* mem_read_inst(struct mem_controller *, uint16_t);
// TODO: inline these?
uint8_t mem_read(struct mem_controller *, uint16_t);
void mem_write(struct mem_controller *, uint16_t, uint8_t);

// TODO: remove this to make sure all write go through mem_write
uint8_t* mem_ptr(struct mem_controller *, uint16_t);

#endif
