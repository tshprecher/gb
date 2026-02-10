#ifndef MEMORY_H
#define MEMORY_H

#include "inst.h"
#include "display.h"
#include "cpu.h"

char * mmapped_reg_to_str(uint16_t);

struct mem_controller {
  uint8_t ram[0x10000];
  struct interrupt_controller *interrupt_c;
  struct lcd_controller *lcd_c;
  struct timing_controller *timing_c;
  struct input_controller *input_c;
  struct sound_controller *sound_c;
};

uint8_t mem_read(struct mem_controller *, uint16_t);
void mem_write(struct mem_controller *, uint16_t, uint8_t);
// TODO: is mem_read_inst necessary?
struct inst* mem_read_inst(struct mem_controller *, uint16_t);

// TODO: remove this to make sure all write go through mem_write
uint8_t* mem_ptr(struct mem_controller *, uint16_t);

#endif
