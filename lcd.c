#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include "lcd.h"


// X11 variables
// TODO: put these in the lcd struct later
Display *display = NULL;
static Window window;
//static XEvent event;
static GC gc;
static int screen;

static uint64_t colors[4] = {0xFFFFFF, 0xa9a9a9, 0x545454, 0x000000};

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
    XSelectInput(display, window, KeyReleaseMask | KeyPressMask);


    // create graphics context
    gc = XCreateGC(display, window, 0, NULL);
    XSetForeground(display, gc, BlackPixel(display, screen)); // Set foreground color for drawing

    // map to make visible
    XMapWindow(display, window);
    XFlush(display);
  }
}

static void frame_put_chr(uint8_t frame[256][256], uint8_t x, uint8_t y, uint8_t *chr, uint8_t palette) {
  for (int c = 0; c < 16; c+=2) {
    for (int b = 7; b >= 0; b--) {
      int upper = (chr[c] & (1 << b)) != 0;
      int lower = (chr[c+1] & (1 << b)) != 0;
      int color = (upper << 1) + lower;

      int row = c >> 1;
      int col = 7-b;
      frame[y+row][x+col] = (palette >> (color<<1)) & 0x3;
    }
  }
}

static void lcd_refresh_frame(struct lcd_controller *lcd) {
  uint8_t chr[16];
  uint8_t frame[256][256];

  // first: background
  if (is_bit_set(lcd->LCDC, 0)) {
    int bg_code_select_addr = is_bit_set(lcd->LCDC, 3) ? 0x9C00 : 0x9800;
    int bg_char_select_addr = is_bit_set(lcd->LCDC, 4) ? 0x8000 : 0x8800;

    // TODO: do the modes properly
    int tile_idx = 0;
    while (tile_idx < 1024) {
      // get character code
      int tile_addr = bg_code_select_addr + tile_idx;
      uint16_t chr_code = mem_read(lcd->mc, tile_addr);
      for (int c = 0; c < 16; c++) {
	chr[c] = mem_read(lcd->mc, bg_char_select_addr + (chr_code<<4) + c);
      }

      int blockX = tile_addr % 32;
      int blockY = tile_addr / 32 - 1216;
      uint8_t palette = lcd->BGP;

      frame_put_chr(frame, blockX*8, blockY*8, chr, palette);
      tile_idx++;
    }
  } else {
    XSetForeground(display, gc, 0xAAAAAA);
    XFillRectangle(display, window, gc, 0, 0, 1280, 1280);
  }

  // second: objects
  if (is_bit_set(lcd->LCDC, 1)) {
    int obj_8_by_8 = is_bit_set(lcd->LCDC, 3) ? 0 : 1;
    if (!obj_8_by_8) {
      printf("DEBUG: unimplemented: 8 x 16 objects\n");
      exit(1);
    }

    for (int oam_addr = 0xFE00; oam_addr < 0xFEA0; oam_addr+=4) {
      uint16_t y_pos = mem_read(lcd->mc, oam_addr);
      uint16_t x_pos = mem_read(lcd->mc, oam_addr+1);
      uint16_t chr_code = mem_read(lcd->mc, oam_addr+2);
      uint16_t attributes = mem_read(lcd->mc, oam_addr+3);

      // TOOD: handle priority
      /*      if (is_bit_set(attributes, 7)) {
	continue;
	}*/

      uint8_t palette = is_bit_set(attributes, 4) ? lcd->OBP1 : lcd->OBP0;

      for (int c = 0; c < 16; c++) {
	chr[c] = mem_read(lcd->mc, 0x8000 + (chr_code<<4) + c);
      }

      // NOTE: for some reason the (0,0) top left corner of the LCD is represented by (8, 16)
      frame_put_chr(frame, x_pos-8, y_pos-16, chr, palette);
    }
  }

  // third: window
  /*  if (is_bit_set(lcd->LCDC, 1)) {
    int win_code_select_addr = is_bit_set(lcd->LCDC, 6) ? 0x9C00 : 0x9800;
    int win_char_select_addr = is_bit_set(lcd->LCDC, 4) ? 0x8000 : 0x8800;

    int tile_idx = 0;
    while (tile_idx < 1024) {
      int tile_addr = win_code_select_addr + tile_idx;
      uint16_t chr_code = mem_read(lcd->mc, tile_addr);
      for (int c = 0; c < 16; c++) {
	chr[c] = mem_read(lcd->mc, win_char_select_addr + (chr_code<<4) + c);
      }

      int blockX = tile_addr % 32;
      int blockY = tile_addr / 32 - 1216;
      uint8_t palette = lcd->BGP;

      frame_put_chr(frame, blockX*8, blockY*8, chr, palette);
      tile_idx++;
    }
    }*/

  for (int y = 0; y < 256; y++) {
    for (int x = 0; x < 256; x++) {
      XSetForeground(display, gc, colors[frame[y][x]]);
      XFillRectangle(display, window, gc, x*5, y*5, 5, 5);
    }
  }

  XFlush(display);
}


void lcd_LCDC_write(struct lcd_controller *lcd, uint8_t value) {
  // TODO: detect the changes and act accordinglingly
  if (is_bit_set(lcd->LCDC,0) != is_bit_set(value, 0)) {
    if (is_bit_set(value, 0))
      printf("DEBUG: LCDC change -> BG display turning on\n");
    else
      printf("DEBUG: LCDC change -> BG display turning off\n");
  }

  if (is_bit_set(lcd->LCDC,1) != is_bit_set(value, 1)) {
    if (is_bit_set(value, 1))
      printf("DEBUG: LCDC change -> OBJ turning on\n");
    else
      printf("DEBUG: LCDC change -> OBJ turning off\n");
  }
  if (is_bit_set(lcd->LCDC,2) != is_bit_set(value, 2)) {
    if (is_bit_set(value, 2))
      printf("DEBUG: LCDC change -> OBJ block selection turning on\n");
    else
      printf("DEBUG: LCDC change -> OBJ block selection turning off\n");
  }
  if (is_bit_set(lcd->LCDC,3) != is_bit_set(value, 3)) {
    if (is_bit_set(value, 3))
      printf("DEBUG: LCDC change -> BG code selection turning on\n");
    else
      printf("DEBUG: LCDC change -> BG code selection selection turning off\n");
  }

  if (is_bit_set(lcd->LCDC,4) != is_bit_set(value, 4)) {
    if (is_bit_set(value, 4))
      printf("DEBUG: LCDC change -> BG char selection turning on\n");
    else
      printf("DEBUG: LCDC change -> BG char selection selection turning off\n");
  }

  if (is_bit_set(lcd->LCDC,5) != is_bit_set(value, 5)) {
    if (is_bit_set(value, 5))
      printf("DEBUG: LCDC change -> windowing turning on\n");
    else
      printf("DEBUG: LCDC change -> windowing turning off\n");
  }

  if (is_bit_set(lcd->LCDC,6) != is_bit_set(value, 6)) {
    if (is_bit_set(value, 6))
      printf("DEBUG: LCDC change -> window code area turning on\n");
    else
      printf("DEBUG: LCDC change -> window code area turning off\n");
  }

  if (is_bit_set(lcd->LCDC,7) != is_bit_set(value, 7)) {
    if (is_bit_set(value, 7)) {
      printf("DEBUG: LCDC change -> LCD turning on\n");
    } else {
      printf("DEBUG: LCDC change -> LCD turning off\n");
      lcd->LY = 0;
      lcd->t_cycles = 0;

      XSetForeground(display, gc, 0x222222);
      XFillRectangle(display, window, gc, 0, 0, 1280, 1280);
      XFlush(display);
    }

  }
  lcd->LCDC = value;
}


void lcd_tick(struct lcd_controller *lcd) {
  /* DEBUG: dev notes
     - cpu clock is 4.1943MHz
         - lcd frame frequency is 59.7Hz
     - 144 lines by 160 columns
         - implies 108.7 microseconds/line
         - implies 108.7/160 == .679375 microseconds/pixel moving left to right
         - is horizontal "blanking" instantaneous?
    - takes 10 lines for vertical blanking period
         - 4571.787 cycles, round to 4572 for now
	 - round to 457/line for now
  */

  // skip if off
  if (!is_bit_set(lcd->LCDC, 7))
    return;


  lcd->t_cycles++;
  lcd->LY = lcd->t_cycles / 457;

  if (lcd->LY == 154) {
    lcd->LY = 0;
    lcd->t_cycles = 0;
  } else if (lcd->LY == 144 && lcd->t_cycles % 457 == 0) { // TODO: this whole function is hacky
    // entered vblank period, so refresh and trigger interrupt
    printf("DEBUG: refreshing frame with LCDC ->  0x%02X\n", lcd->LCDC);
    lcd_refresh_frame(lcd);
    interrupt(lcd->ic, VBLANK);
  }
}
