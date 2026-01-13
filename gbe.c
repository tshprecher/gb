#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "audio.h"
#include "inst.h"
#include "cpu.h"
#include "mem_controller.h"

#define CLOCK_FREQ 4194304

extern Display *display; // for X window button events

static void sleep_ns(int64_t ns) {
  const struct timespec ts = {tv_nsec: ns};
  struct timespec rem;
  int res;
  printf("DEBUG: ns -> %ld\n", ns);
  if (res = nanosleep(&ts, &rem)) { // TODO: change to clock_nanosleep
    printf("DEBUG: res -> %d\n", res);
    perror("nanosleep() failed");
    exit(1);
  }
}

static int64_t get_time_ns() {
  struct timespec ts = {0};
  if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
    perror("clock_gettime() failed");
    exit(1);
  }
  printf("DEBUG: returning get_time_ns -> %ld\n", ts.tv_nsec);
  return ts.tv_nsec;
}

struct gb
{
  struct cpu *cpu;
  struct mem_controller *mc;
  struct interrupt_controller *ic;
  struct lcd_controller *lcd;
  struct timing_controller *tc;
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
  KeySym ks;
  char buf[1];

  if (XPending(display)) {
    XNextEvent(display, &event);

    switch (event.type) {
    case KeyPress:
    case KeyRelease:
      enum btn btn = BTN_START;
      XLookupString(&event.xkey, buf, sizeof(buf), &ks, NULL);

      switch (ks) {
      case XK_Right:
	btn = BTN_RIGHT;
	break;
      case XK_Left:
	btn = BTN_LEFT;
	break;
      case XK_Up:
	btn = BTN_UP;
	break;
      case XK_Down:
	btn = BTN_DOWN;
	break;
      case XK_a:
	btn = BTN_A;
	break;
      case XK_b:
	btn = BTN_B;
	break;
      case XK_space:
	btn = BTN_SELECT;
	break;
      }

      if (event.type == KeyPress) {
	port_btn_press(gb->pc, btn);
      } else if (event.type == KeyRelease) {
	port_btn_unpress(gb->pc, btn);
      }

      break;
    default:
      break;
    }
  }
}

void gb_run(struct gb *gb)
{
  init_lcd();
  init_audio();

  int t_cycles = 0;
  int64_t last_cycle_time_ns = get_time_ns();

  while (1) {
    t_cycles++;

    cpu_tick(gb->cpu);
    lcd_tick(gb->lcd);
    port_tick(gb->pc);
    timing_tick(gb->tc);
    if (t_cycles % 1024 == 0)
      gb_poll_buttons(gb);

    if (t_cycles % ((1<<16) + 1) == 0) {
      int64_t cur_cycle_time_ns = get_time_ns();
      int64_t cur_period_time_ns;
      if (cur_cycle_time_ns > last_cycle_time_ns) {
	 cur_period_time_ns = cur_cycle_time_ns - last_cycle_time_ns;
      } else {
	cur_period_time_ns = 999999999 - last_cycle_time_ns + cur_cycle_time_ns;
      }

      printf("DEBUG: period in ns for 65K clock cycles: %ld\n", cur_period_time_ns);
      last_cycle_time_ns = cur_cycle_time_ns;
      int sleep_time_ns = 15625000 > cur_period_time_ns ? (15625000 - cur_period_time_ns) : 0;
      sleep_ns(sleep_time_ns);
    }

  }
  printf("ran %d clock cycles\n", t_cycles);
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
    struct timing_controller tc = {0};
    struct sound_controller sc = {0};

    lcd.mc = &mc;
    lcd.ic = &ic;
    lcd.LCDC = 0x83; // TODO: put in an init?

    mc.ic = &ic;
    mc.lcd = &lcd;
    mc.tc = &tc;
    mc.pc = &pc;
    mc.sc = &sc;

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
