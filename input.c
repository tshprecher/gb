#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "input.h"
#include "cpu.h"
#include "macros.h"

extern Display *display; // for X window button events

static inline void input_poll(struct input_controller *ic) {
  // check if there are any button events pending
  XEvent event;
  KeySym ks;
  char buf;

  if (XPending(display)) {
    XNextEvent(display, &event);
    enum btn btn;
    switch (event.type) {
    case KeyPress:
    case KeyRelease:
      btn = BTN_START; // default is start
      XLookupString(&event.xkey, &buf, 1, &ks, NULL);

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
      case XK_f:
	btn = BTN_A;
	break;
      case XK_d:
	btn = BTN_B;
	break;
      case XK_space:
	btn = BTN_SELECT;
	break;
      }

      if (event.type == KeyPress) {
	b_set_on(ic->btns_pressed, btn);
	interrupt(ic->interrupt_c, PORT);
      } else if (event.type == KeyRelease) {
	b_set_off(ic->btns_pressed, btn);
      }
      break;
    default:
      break;
    }
  }
}

void input_tick(struct input_controller *ic) {
  ic->ticks_since_last_poll++;
  if (ic->t_cycles_to_read) // TODO: handle this logic properly
    ic->t_cycles_to_read--;
  if (ic->ticks_since_last_poll % 1024 == 0) {
    ic->ticks_since_last_poll = 0;
    input_poll(ic);
  }
}

void input_write_P1(struct input_controller *ic, u8 value) {
  switch (value) {
  case 0x10:
    ic->status = value;
    //    ic->t_cycles_to_read = 4;
    break;
  case 0x20:
    ic->status = value;
    //    ic->t_cycles_to_read = 16;
    break;
  case 0x30:
    ic->status = value;
    ic->t_cycles_to_read = 0;
    break;
  default:
    fprintf(stderr, "error: no-op, writing value 0x%02x to reg $P1\n", value);
  }
}

int input_read_P1(struct input_controller *ic, u8 *result) {
  if (ic->status == 0x30)
    return 0;
  if (ic->t_cycles_to_read)
    return 0;

  if (ic->status == 0x10) {
    *result = (~ic->btns_pressed >> 4) & 0x0F;
  } else if (ic->status == 0x20) {
    *result = (~ic->btns_pressed) & 0x0F;
  }
  return 1;
}
