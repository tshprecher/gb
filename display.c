#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include "display.h"

#define PIXEL_SCALAR 3

// X11 variables
// TODO: where possible  put these in the lcd struct later
Display *display = NULL;
static Window window;
static Pixmap pixmap;
static XImage *image;
static char *framebuf;

static GC gc;
static int screen;

static uint64_t colors[4] = {0xFFFFFF, 0xa9a9a9, 0x545454, 0x000000};

void init_lcd() {
    if (!display) {
      display = XOpenDisplay(NULL);
      if (!display) {
	printf("error: could not initialize display\n");
	exit(1);
      }

      screen = DefaultScreen(display);
      window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, 770, 770, 1,
				   BlackPixel(display, screen), WhitePixel(display, screen));

      /* select input events */
      XSelectInput(display, window, KeyReleaseMask | KeyPressMask);

      // map to make visible
      XMapWindow(display, window);

      pixmap =  XCreatePixmap(display, window, 800, 800, 24);
      framebuf = malloc(200 * 200 * 4);
      printf("DEBUG: pixmap result -> %lu\n", pixmap);

      image = XCreateImage(display, DefaultVisual(display, screen), DefaultDepth(display, screen),
			   ZPixmap, 0, framebuf, 256*PIXEL_SCALAR, 256*PIXEL_SCALAR, 32, 0);


      // create graphics context
      gc = XCreateGC(display, pixmap, 0, NULL);
      XFlush(display);
    }
}

static void frame_to_X11_frame_buffer(struct lcd_controller *lcd) {
  for (int y = 0; y < 256 * PIXEL_SCALAR; y++) {
    for (int x = 0; x < 256 * PIXEL_SCALAR; x++) {
      *(unsigned int *)(framebuf + ((y*256*PIXEL_SCALAR +x) * 4)) = colors[lcd->frame[y/PIXEL_SCALAR][x/PIXEL_SCALAR]];
    }
  }
}

static void X11_refresh() {
  XPutImage(display, pixmap, gc, image, 0, 0, 0, 0, 256*PIXEL_SCALAR, 256*PIXEL_SCALAR);
  XCopyArea(display, pixmap, window, gc, 0, 0, 800, 800, 0, 0);
  XFlush(display);
}

static void frame_put_chr(uint8_t frame[256][256],
			  uint8_t x,
			  uint8_t y,
			  uint8_t *chr,
			  uint8_t palette,
			  uint8_t flip_horizontal,
			  uint8_t flip_vertical,
			  uint8_t override_priority) {
  for (int c = 0; c < 16; c+=2) {
    for (int b = 7; b >= 0; b--) {
      int upper_bit = (chr[c] & (1 << b)) != 0;
      int lower_bit = (chr[c+1] & (1 << b)) != 0;
      int color_index = (upper_bit << 1) + lower_bit;

      int row = c >> 1;
      int col = 7-b;

      if (flip_horizontal) {
	col = 7-col;
      }
      if (flip_vertical) {
	row = 7-row;
      }
      int color = (palette >> (color_index<<1)) & 0x3;
      if (color || (!color && override_priority)) {
	frame[y+row][x+col] = color;
      }
    }
  }
}

static void lcd_refresh_frame(struct lcd_controller *lcd_c) {
  uint8_t chr[16];

  // first: background
  if (is_bit_set(lcd_c->regs[rLCDC], 0)) {
    int bg_code_select_addr = is_bit_set(lcd_c->regs[rLCDC], 3) ? 0x9C00 : 0x9800;
    int bg_char_select_addr = is_bit_set(lcd_c->regs[rLCDC], 4) ? 0x8000 : 0x8800;

    // TODO: do the modes properly
    int tile_idx = 0;
    while (tile_idx < 1024) {
      // get character code
      int tile_addr = bg_code_select_addr + tile_idx;
      uint16_t chr_code = mem_read(lcd_c->memory_c, tile_addr);
      for (int c = 0; c < 16; c++) {
	chr[c] = mem_read(lcd_c->memory_c, bg_char_select_addr + (chr_code<<4) + c);
      }

      int blockX = tile_addr % 32;
      int blockY = tile_addr / 32 - 1216;
      uint8_t palette = lcd_c->regs[rBGP];

      frame_put_chr(lcd_c->frame, blockX*8, blockY*8, chr, palette, 0, 0, 1);
      tile_idx++;
    }
  } else {
    XSetForeground(display, gc, 0xFFFFFF);
    XFillRectangle(display, pixmap, gc, 0, 0, 800, 800);
    XFlush(display);
  }

  // second: objects
  if (is_bit_set(lcd_c->regs[rLCDC], 1)) {
    // TODO: does 8x16 mean different logic or was is just a hardware/software optimization?
    //   int obj_8_by_8 = is_bit_set(lcd_c->regs[rLCDC], 3) ? 0 : 1;

    for (int oam_addr = 0xFE00; oam_addr < 0xFEA0; oam_addr+=4) {
      uint8_t y_pos = mem_read(lcd_c->memory_c, oam_addr);
      uint8_t x_pos = mem_read(lcd_c->memory_c, oam_addr+1);
      uint8_t chr_code = mem_read(lcd_c->memory_c, oam_addr+2);
      uint8_t attributes = mem_read(lcd_c->memory_c, oam_addr+3);

      // TODO: handle OBJ priority
      uint8_t palette = is_bit_set(attributes, 4) ? lcd_c->regs[rOBP1] : lcd_c->regs[rOBP0];
      uint8_t flip_horizontal = is_bit_set(attributes, 5);
      uint8_t flip_vertical = is_bit_set(attributes, 6);
      uint8_t override_priority = !is_bit_set(attributes, 7);

      for (int c = 0; c < 16; c++) {
	chr[c] = mem_read(lcd_c->memory_c, 0x8000 + (chr_code<<4) + c);
      }

      // NOTE: for some reason the (0,0) top left corner of the LCD is represented by (8, 16)
      frame_put_chr(lcd_c->frame, x_pos-8, y_pos-16, chr, palette, flip_horizontal, flip_vertical, override_priority);
    }
  }

  // third: window
  if (is_bit_set(lcd_c->regs[rLCDC], 5)) {
    printf("debug: LCD window set\n");
    int win_code_select_addr = is_bit_set(lcd_c->regs[rLCDC], 6) ? 0x9C00 : 0x9800;
    int win_char_select_addr = is_bit_set(lcd_c->regs[rLCDC], 4) ? 0x8000 : 0x8800;

    int tile_idx = 0;
    while (tile_idx < 1024) {
      int tile_addr = win_code_select_addr + tile_idx;
      uint16_t chr_code = mem_read(lcd_c->memory_c, tile_addr);
      for (int c = 0; c < 16; c++) {
	chr[c] = mem_read(lcd_c->memory_c, win_char_select_addr + (chr_code<<4) + c);
      }

      int blockX = tile_addr % 32;
      int blockY = tile_addr / 32 - 1216;
      uint8_t palette = lcd_c->regs[rBGP];

      frame_put_chr(lcd_c->frame, blockX*8, blockY*8, chr, palette, 0, 0, 1);
      tile_idx++;
    }
  }

  frame_to_X11_frame_buffer(lcd_c);
  X11_refresh();
}

void lcd_tick(struct lcd_controller *lcd_c) {
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
  if (!is_bit_set(lcd_c->regs[rLCDC], 7))
    return;


  lcd_c->t_cycles_since_last_line_refresh++;
  lcd_c->regs[rLY] = lcd_c->t_cycles_since_last_line_refresh / 457;

  if (lcd_c->regs[rLY] == 154) {
    lcd_c->regs[rLY] = 0;
    lcd_c->t_cycles_since_last_line_refresh = 0;
  } else if (lcd_c->regs[rLY] == 144 && lcd_c->t_cycles_since_last_line_refresh % 457 == 0) { // TODO: this whole function is hacky
    // entered vblank period, so refresh and trigger interrupt
    printf("debug: refreshing frame with LCDC ->  0x%02X\n", lcd_c->regs[rLCDC]);
    lcd_refresh_frame(lcd_c);
    interrupt(lcd_c->interrupt_c, VBLANK);
  }
}

uint8_t lcd_reg_read(struct lcd_controller* lcd_c, enum lcd_reg reg) {
  return lcd_c->regs[reg];
}

void lcd_reg_write(struct lcd_controller* lcd_c, enum lcd_reg reg, uint8_t value) {
  uint8_t original_value = lcd_c->regs[reg];
  lcd_c->regs[reg] = value;
  switch (reg) {
  case rLCDC:
    if (is_bit_set(original_value,7) != is_bit_set(value, 7)) {
      if (is_bit_set(value, 7)) {
	printf("debug: LCDC change -> LCD turning on\n");
      } else {
	printf("debug: LCDC change -> LCD turning off\n");
	lcd_c->regs[rLY] = 0;
	lcd_c->t_cycles_since_last_line_refresh = 0;

	XSetForeground(display, gc, 0xFFFFFF);
	XFillRectangle(display, pixmap, gc, 0, 0, 800, 800);
	XCopyArea(display, pixmap, window, gc, 0, 0, 800, 800, 0, 0);
	XFlush(display);
      }
    }
    break;
  case rDMA:
    for (int b = 0; b <= 0x9F; b++) {
      mem_write(lcd_c->memory_c, 0xFE00 + b, mem_read(lcd_c->memory_c, (value<<8)+b));
    }
    break;
  default:
    break;
  }
}
