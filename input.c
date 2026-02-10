#include "input.h"
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

extern Display *display; // for X window button events

static inline void input_btn_press(struct input_controller *ic, enum btn btn) {
  ic->btns_pressed |= (1 << btn);
}

static inline void input_btn_release(struct input_controller *ic, enum btn btn) {
  ic->btns_pressed &= ~(1 << btn);
}

static inline void input_poll(struct input_controller *ic) {
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
	input_btn_press(ic, btn);
      } else if (event.type == KeyRelease) {
	input_btn_release(ic, btn);
      }

      break;
    default:
      break;
    }
  }
}

// TODO: handle interrupt
void input_tick(struct input_controller *ic) {
  ic->ticks_since_last_poll++;
  if (ic->t_cycles_to_read) // TODO: handle this logic properly
    ic->t_cycles_to_read--;
  if (ic->ticks_since_last_poll % 1024 == 0) {
    ic->ticks_since_last_poll = 0;
    input_poll(ic);
  }
}

void input_write_P1(struct input_controller *ic, uint8_t value) {
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
    fprintf(stderr, "error: no-op, writing value 0x%02x to reg $P1", value);
  }
}

int input_read_P1(struct input_controller *ic, uint8_t *result) {
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
