#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include "lcd.h"


// X11 variables
// TODO: put these in the lcd struct later
static Display *display = NULL;
static Window window;
//static XEvent event;
static GC gc;
static int screen;

void init_lcd() {
  if (!display) {
    printf("DEBUG: inside lcd init\n");
    display = XOpenDisplay(NULL);
    if (!display) {
      fprintf(stderr, "could not initialize display\n");
      exit(1);
    }

    screen = DefaultScreen(display);
    window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, 1500, 1500, 1,
				 BlackPixel(display, screen), WhitePixel(display, screen));


    /* 3. Select input events (we need Expose events to know when to draw) */
    XSelectInput(display, window, ExposureMask | KeyPressMask);


    // create graphics context
    gc = XCreateGC(display, window, 0, NULL);
    XSetForeground(display, gc, BlackPixel(display, screen)); // Set foreground color for drawing

    // map to make visible
    XMapWindow(display, window);
    XFlush(display);
  }
}

void lcd_tick(struct lcd_controller *lcd) {
  /* DEBUG: dev notes
     - cpu clock is 4.1943MHz
         - lcd frame frequency is 59.7Hz
     - 144 lines by 160 columns
         - implies 108.7 microseconds/line
         - implies 108.7/160 == .679375 microseconds/pixel moving left to right
         - is horizontal "blJanking" instantaneous?
    - takes 10 lines for vertical blanking period
         - 4571.787 cycles, round to 4572 for now
	 - round to 457/line for now
  */
  if (!is_bit_set(lcd->LCDC, 7))
    return;

  lcd->t_cycles++;
  lcd->LY = lcd->t_cycles / 457;
  if (lcd->t_cycles % 457 == 0) {
    lcd->LY++;
    if (lcd->LY == 144) {
      interrupt_vblank(lcd->ic);
    }
    if (lcd->LY == 154) {
      lcd->t_cycles = 0;
      lcd->LY = 0;
    }
    printf("DEBUG: incremented LY to %d\n", lcd->LY);
  }

}
