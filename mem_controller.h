#ifndef MEM_CONTROLLER_H
#define MEM_CONTROLLER_H

#include "inst.h"
#include "lcd.h"
#include "cpu.h"

char * mmapped_reg_to_str(uint16_t addr);

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

struct port_controller {
  uint8_t status;
  uint8_t btns_pressed;
  uint8_t t_cycles_to_read;
};

void port_tick(struct port_controller *pc);
void port_btn_press(struct port_controller *pc, enum btn btn);
void port_btn_unpress(struct port_controller *pc, enum btn btn);
void port_write_P1(struct port_controller *, uint8_t);
int port_read_P1(struct port_controller *, uint8_t *);

struct mem_controller {
  uint8_t ram[0x10000];
  struct interrupt_controller *ic;
  struct lcd_controller *lcd;
  struct timer_controller *tc;
  struct port_controller *pc;
};

uint8_t mem_read(struct mem_controller *, uint16_t);
void mem_write(struct mem_controller *, uint16_t, uint8_t);
struct inst* mem_read_inst(struct mem_controller *, uint16_t);

// TODO: remove this to make sure all write go through mem_write
uint8_t* mem_ptr(struct mem_controller *, uint16_t);


#endif
