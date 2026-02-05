#ifndef MEMORY_H
#define MEMORY_H

#include "inst.h"
#include "display.h"
#include "cpu.h"

char * mmapped_reg_to_str(uint16_t);

enum btn {
  BTN_RIGHT = 0,
  BTN_LEFT,
  BTN_UP,
  BTN_DOWN,
  BTN_A,
  BTN_B,
  BTN_SELECT,
  BTN_START
};

struct input_controller {
  uint16_t polling_freq;
  uint8_t status;
  uint8_t btns_pressed;
  uint8_t t_cycles_to_read;
};

void input_tick(struct input_controller *);
void input_btn_press(struct input_controller *, enum btn);
void input_btn_release(struct input_controller *, enum btn);
void input_write_P1(struct input_controller *, uint8_t);
int input_read_P1(struct input_controller *, uint8_t *);

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
