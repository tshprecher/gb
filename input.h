#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

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
  uint16_t ticks_since_last_poll;
  uint8_t status;
  uint8_t btns_pressed;
  uint8_t t_cycles_to_read;
};

void input_tick(struct input_controller *);
void input_write_P1(struct input_controller *, uint8_t);
int input_read_P1(struct input_controller *, uint8_t *);

#endif
