#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "inst.h"
#include "cpu.h"
#include "mem_controller.h"


extern Display *display; // for X window button events

// sleep at after every frame refresh at 59.7Hz, the inverse of which
// approximates to 16750418 nanoseconds.
// TOOD: handle the fractional nanoseconds for better
// precision, dynamically set the period based on work done.
static const struct timespec tick_timespec = {tv_nsec: 16750418};

static void sleep_frame() {
  // TODO: handle error case, remain
  int res = clock_nanosleep(CLOCK_MONOTONIC, 0, &tick_timespec, NULL);
  if (res) {
    printf("DEBUG: error in sleep: %d\n", res);
  }
}

struct gb
{
  struct cpu *cpu;
  struct mem_controller *mc;
  struct interrupt_controller *ic;
  struct lcd_controller *lcd;
  struct timer_controller *tc;
  struct port_controller *pc;
};

ssize_t gb_dump(struct gb *gb) {
  FILE *dump;
  if ((dump = fopen("dump.bin", "wb")) == NULL) {
    return 0;

  }
  size_t result = fwrite(gb->mc->ram, 1, 0x10000, dump);
  printf("DEBUG: dumped total %ld bytes\n", result);
  fclose(dump);
  return result;
}

void gb_poll_buttons(struct gb *gb) {
  // Check if there are any events pending
  XEvent event;
  char buf[128];
  KeySym keysym;

  if (XPending(display)) {
    XNextEvent(display, &event);

    switch (event.type) {
    case KeyPress:
    case KeyRelease:
      XLookupString(&event.xkey, buf, sizeof(buf), &keysym, NULL);
      if (event.type == KeyPress) {
	port_btn_press(gb->pc, BTN_SELECT);
      } else if (event.type == KeyRelease) {
	port_btn_unpress(gb->pc, BTN_SELECT);
      }

      printf("DEBUG: Key %s: %s\n", (event.type == KeyPress) ? "pressed" : "released", XKeysymToString(keysym));
      break;
    default:
      break;
    }
  }
}

void gb_run(struct gb *gb)
{
  init_lcd();

  // run until error, dump core on error
  int t_cycles = 0;
  int LIMIT = 4194304 * 20;
  //int LIMIT = 800000;
  while (t_cycles < LIMIT) {
    t_cycles++;

    cpu_tick(gb->cpu);
    lcd_tick(gb->lcd);
    port_tick(gb->pc);
    if (t_cycles % 1024 == 0)
      gb_poll_buttons(gb);

    /*    if (t_cycles % 4194304 == 0) {
      sleep(1);
      }*/
  }
  printf("ran %d clock cycles\n", t_cycles);
  if (LIMIT % 4000000 == 0) {
    sleep_frame();
  }
  sleep(5);
  if (gb_dump(gb) < LIMIT) {
    fprintf(stderr, "error: could not write dump file");
  }
}


// TODO: make this a method of memory controller
void load_rom(struct mem_controller *mc, char *filename)
{
    FILE *fin = fopen(filename, "rb");
    if (NULL == fin)
    {
        fprintf(stderr, "error opening file: %s\n", filename);
        exit(1);
    }

    char b;
    int addr = 0;
    while (addr < 0x8000)
    {
        size_t c = fread(&b, 1, 1, fin);
        if (c == 0)
            break;
	mc->ram[addr++] = b;
    }
    if (addr != 0x8000)
    {
        fprintf(stderr, "error reading ROM: cannot read full 32K of ROM: %x\n", addr);
        exit(1);
    }
    fclose(fin);
}


// TODO: use getopt
int main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "error: missing arguments.\n");
    return 1;
  } else if (argc == 2) { // run game
    struct cpu cpu = {0};
    init_cpu(&cpu);

    struct gb gb = {0};
    struct mem_controller mc = {0};
    struct port_controller pc = {0};
    struct interrupt_controller ic = {0};
    struct lcd_controller lcd = {0};
    struct timer_controller tc = {0};

    lcd.mc = &mc;
    lcd.ic = &ic;
    lcd.LCDC = 0x83; // TODO: put in an init?

    mc.ic = &ic;
    mc.lcd = &lcd;
    mc.tc = &tc;
    mc.pc = &pc;

    cpu.mc = &mc;
    cpu.ic = &ic;

    gb.cpu = &cpu;
    gb.mc = &mc;
    gb.lcd = &lcd;
    gb.tc = &tc;
    gb.pc = &pc;

    load_rom(gb.mc, argv[1]);
    gb_run(&gb);
  } else if (argc == 3) { // disassemble
    if (strcmp(argv[1], "-d") != 0) {
      fprintf(stderr, "error: unknown argument %s, use '-d'.\n", argv[1]);
      return 1;
    }

    struct mem_controller mc = {0};
    load_rom(&mc, argv[2]);

    int addr = 0x0;
    struct inst *decoded;
    char buf[16];
    while (addr < 0x8000) {
      decoded = mem_read_inst(&mc, addr);
      if (decoded) {
	inst_to_str(buf, decoded);
	printf("0x%02X\t%s\n", addr, buf);
	addr+=decoded->bytelen;
      } else {
	printf("0x%02X\tDB 0x%02X\n", addr, mc.ram[addr]);
	addr++;
      }
    }
  } else {
    fprintf(stderr, "error: too many arguments.\n");
    return 1;
  }

  return 0;
}
