#ifndef INPUT_H
#define INPUT_H

#include "types.h"

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
  u16 polling_freq;
  u16 ticks_since_last_poll;
  u8 status;
  u8 btns_pressed;
  u8 t_cycles_to_read;

  struct interrupt_controller *interrupt_c;
};

void input_tick(struct input_controller *);
void input_write_P1(struct input_controller *, u8);
int input_read_P1(struct input_controller *, u8 *);

#endif
