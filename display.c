#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include "display.h"

#define PIXEL_SCALAR 3
#define SCREEN_SIZE 256 * PIXEL_SCALAR

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
      window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, SCREEN_SIZE, SCREEN_SIZE, 1,
				   BlackPixel(display, screen), WhitePixel(display, screen));

      /* select input events */
      XSelectInput(display, window, KeyReleaseMask | KeyPressMask);

      // map to make visible
      XMapWindow(display, window);

      pixmap =  XCreatePixmap(display, window, SCREEN_SIZE, SCREEN_SIZE, 24);
      framebuf = malloc(256 * PIXEL_SCALAR * 256 * PIXEL_SCALAR * 4);
      //printf("DEBUG: pixmap result -> %lu\n", pixmap);

      image = XCreateImage(display, DefaultVisual(display, screen), DefaultDepth(display, screen),
			   ZPixmap, 0, framebuf, SCREEN_SIZE, SCREEN_SIZE, 32, 0);


      // create graphics context
      gc = XCreateGC(display, pixmap, 0, NULL);
      XFlush(display);
    }
}


//
// TODO: do the modes properly
//


static inline int is_background_on(struct lcd_controller *lcd_c) {
  return is_bit_set(lcd_c->regs[rLCDC], 0);
}

static inline int is_obj_on(struct lcd_controller *lcd_c) {
  return is_bit_set(lcd_c->regs[rLCDC], 1);
}

static inline int is_bg_code_area_upper(struct lcd_controller *lcd_c) {
  return is_bit_set(lcd_c->regs[rLCDC], 3);
}

static inline int is_char_area_upper(struct lcd_controller *lcd_c) {
  return !is_bit_set(lcd_c->regs[rLCDC], 4);
}

static inline int is_windowing_on(struct lcd_controller *lcd_c) {
  return is_bit_set(lcd_c->regs[rLCDC], 5);
}

static inline int is_window_code_area_upper(struct lcd_controller *lcd_c) {
  return is_bit_set(lcd_c->regs[rLCDC], 6);
}

static inline int is_on(struct lcd_controller *lcd_c) {
  return is_bit_set(lcd_c->regs[rLCDC], 7);
}

static inline int interrupt_mode(struct lcd_controller *lcd_c) {
  return (lcd_c->regs[rSTAT] >> 3) & 3;
}

static void frame_to_X11_frame_buffer(struct lcd_controller *lcd) {
  for (int y = 0; y < SCREEN_SIZE; y++) {
    for (int x = 0; x < SCREEN_SIZE; x++) {
      *(unsigned int *)(framebuf + ((y*SCREEN_SIZE + x) * 4)) = colors[lcd->frame[y/PIXEL_SCALAR][x/PIXEL_SCALAR]];
    }
  }
}

static void X11_refresh() {
  XPutImage(display, pixmap, gc, image, 0, 0, 0, 0, SCREEN_SIZE, SCREEN_SIZE);
  XCopyArea(display, pixmap, window, gc, 0, 0, SCREEN_SIZE, SCREEN_SIZE, 0, 0);
  XFlush(display);
}

static void lcd_put_chr(struct lcd_controller *lcd_c,
			uint8_t x,
			uint8_t y,
			uint8_t *chr,
			uint8_t palette,
			uint8_t flip_horizontal,
			uint8_t flip_vertical,
			uint8_t override_priority) { // TODO: rename to just override?
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
	lcd_c->frame[y+row][x+col] = color;
      }
    }
  }
}

static void lcd_refresh_tiles_by_line(struct lcd_controller *lcd_c,
				      int code_select_addr,
				      int char_select_addr) {
  // TODO: priorities, SCY and SCX

  // TODO: fix
  char_select_addr = 0x8000;

  // tiles are each 8x8, but show the full tile once a new one is seen, forgo
  // perfect line refresh accuracy in favor of simplicity.
  uint8_t line = lcd_c->regs[rLY];
  if (line % 8 != 0) {
    return;
  }

  uint8_t chr[16];
  int x_scan = 0;
  while (x_scan < 160) {
    int tile_idx = (line/8)*32 + x_scan/8;

    // get character code
    int tile_addr = code_select_addr + tile_idx;
    uint16_t chr_code = mem_read(lcd_c->memory_c, tile_addr);
    for (int c = 0; c < 16; c++) {
      chr[c] = mem_read(lcd_c->memory_c, char_select_addr + (chr_code<<4) + c);
    }

    int blockX = tile_addr % 32;
    int blockY = tile_addr / 32;
    uint8_t palette = lcd_c->regs[rBGP];

    lcd_put_chr(lcd_c, blockX*8, blockY*8, chr, palette, 0, 0, 1);
    x_scan+=8;
  }
}

static void lcd_refresh_objs_by_line(struct lcd_controller *lcd_c) {
  // TODO: does 8x16 mean different logic or was is just a hardware/software optimization?
  //int obj_8_by_8 = is_bit_set(lcd_c->regs[rLCDC], 3) ? 0 : 1;

  uint8_t line = lcd_c->regs[rLY];

  // TODO: handle SCX, SCY
  if (line >= 144)
    return;

  uint8_t chr[16];
  int obj_ids_on_line[10]; // max 10 on a given line
  int obj_x_pos_on_line[10];
  int num_objs = 0;

  // find the objects on the line, sorted by x coordinate
  for (int oam_id = 0; oam_id < 40 && num_objs < 10; oam_id++) {
    int oam_addr = 0xFE00 + oam_id*4;
    uint8_t y_pos = mem_read(lcd_c->memory_c, oam_addr);
    // NOTE: object positions are offset by 16 y-pixels and 8 x-pixels (see comment below)
    if (y_pos-16 != line) {
      continue;
    }

    uint8_t x_pos = mem_read(lcd_c->memory_c, oam_addr+1);
    obj_ids_on_line[num_objs] = oam_id;
    obj_x_pos_on_line[num_objs] = x_pos;

    for (int i = num_objs; i > 0 && obj_x_pos_on_line[i-1] > obj_x_pos_on_line[i]; i--) {
      obj_ids_on_line[i] = obj_ids_on_line[i-1];
      obj_x_pos_on_line[i] = obj_x_pos_on_line[i-1];

      obj_ids_on_line[i-1] = oam_id;
      obj_x_pos_on_line[i-1] = x_pos;
    }
    num_objs++;
  }

  // display the objects in descending order by x
  // TODO: revisit the priority logic of objects
  for (int i = num_objs-1; i >= 0; i--) {
    int oam_addr = 0xFE00 + obj_ids_on_line[i]*4;
    uint8_t y_pos = mem_read(lcd_c->memory_c, oam_addr);
    uint8_t x_pos = mem_read(lcd_c->memory_c, oam_addr+1);
    uint8_t chr_code = mem_read(lcd_c->memory_c, oam_addr+2);
    uint8_t attributes = mem_read(lcd_c->memory_c, oam_addr+3);

    uint8_t palette = is_bit_set(attributes, 4) ? lcd_c->regs[rOBP1] : lcd_c->regs[rOBP0];
    uint8_t flip_horizontal = is_bit_set(attributes, 5);
    uint8_t flip_vertical = is_bit_set(attributes, 6);
    uint8_t override_priority = !is_bit_set(attributes, 7);

    for (int c = 0; c < 16; c++) {
      chr[c] = mem_read(lcd_c->memory_c, 0x8000 + (chr_code<<4) + c);
    }

    // NOTE: for some reason the (0,0) top left corner of the LCD is represented by (8, 16)
    lcd_put_chr(lcd_c, x_pos-8, y_pos-16, chr, palette, flip_horizontal, flip_vertical, override_priority);
  }
}

static void lcd_refresh_frame_line(struct lcd_controller *lcd_c) { // TODO: naming consistency "by_line" vs. just "_line"?
  // first: background
  if (is_background_on(lcd_c)) {
    lcd_refresh_tiles_by_line(lcd_c,
			      is_bg_code_area_upper(lcd_c) ? 0x9C00 : 0x9800,
			      is_char_area_upper(lcd_c) ? 0x8800 : 0x8000);
  } else {
    XSetForeground(display, gc, 0xAAAAAA);
    XFillRectangle(display, pixmap, gc, 0, 0, SCREEN_SIZE, SCREEN_SIZE);
    XFlush(display);
  }

  // second: objects
  // TODO: do this by line for better accuracy
  if (is_obj_on(lcd_c)) {
    lcd_refresh_objs_by_line(lcd_c);
  }

  // third: window
  if (is_windowing_on(lcd_c)) {
    lcd_refresh_tiles_by_line(lcd_c,
			      is_window_code_area_upper(lcd_c) ? 0x9C00 : 0x9800,
			      is_char_area_upper(lcd_c) ? 0x8000 : 0x8800);
  }
}


/**
   DEBUG: dev notes
     - cpu clock is 4.1943MHz
         - lcd frame frequency is 59.7Hz
     - 144 lines by 160 columns
         - implies 108.7 microseconds/line
         - implies 108.7/160 == .679375 microseconds/pixel moving left to right
         - is horizontal "blanking" instantaneous?
	    - looks like now, reading more about it online. it's such a small window (~200 clock cycles)
	      it doesn't seem to be included in the manual
    - takes 10 lines for vertical blanking period
         - 4571.787 cycles, round to 4572 for now
	 - round to 457/line for now
*/
void lcd_tick(struct lcd_controller *lcd_c) {
  // skip if off
  if (!is_on(lcd_c))
    return;

  lcd_c->t_cycles_since_last_line_refresh++;
  if (lcd_c->t_cycles_since_last_line_refresh % 457 == 0) {
    lcd_refresh_frame_line(lcd_c);
    lcd_c->regs[rLY]++;
    if (lcd_c->regs[rLY] == 144) {
      // entered vblank period, so refresh and trigger interrupt
      //printf("debug: refreshing frame with LCDC ->  0x%02X\n", lcd_c->regs[rLCDC]);
      frame_to_X11_frame_buffer(lcd_c);
      X11_refresh();
      interrupt(lcd_c->interrupt_c, VBLANK);
    }
  }

  // TODO: handle the proper delay on Mode 00 (horizontal blanking) interrupt
  if (lcd_c->regs[rLY] == 154) {
    lcd_c->regs[rLY] = 0;
    lcd_c->t_cycles_since_last_line_refresh = 0;
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
	//printf("debug: LCDC change -> LCD turning on\n");
      } else {
	//printf("debug: LCDC change -> LCD turning off\n");
	lcd_c->regs[rLY] = 0;
	lcd_c->t_cycles_since_last_line_refresh = 0;

	XSetForeground(display, gc, 0xAAAAAA);
		       XFillRectangle(display, pixmap, gc, 0, 0, SCREEN_SIZE, SCREEN_SIZE);
		       XCopyArea(display, pixmap, window, gc, 0, 0, SCREEN_SIZE, SCREEN_SIZE, 0, 0);
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
