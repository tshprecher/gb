#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include "macros.h"
#include "types.h"
#include "display.h"

#define PIXEL_SCALAR 3
#define SCREEN_X 160
#define SCREEN_Y 144
#define SCALED_SCREEN_X SCREEN_X * PIXEL_SCALAR
#define SCALED_SCREEN_Y SCREEN_Y * PIXEL_SCALAR

#define max(a, b) (a > b ? a : b)

enum lcd_mode {
  MODE_HBLANK = 0,
  MODE_VBLANK,
  MODE_OAM_IN_USE,
  MODE_OAM_AND_VRAM_IN_USE
};

// X11 variables
// TODO: where possible  put these in the lcd struct later
Display *display = NULL;
static Window window;
static Pixmap pixmap;
static XImage *image;
static u32 (*fb)[SCALED_SCREEN_X];

static GC gc;
static int screen;

//static uint64_t colors[4] = {0xFFFFFF, 0xa9a9a9, 0x545454, 0x000000};
//static uint64_t colors[4] = {0x000000, 0x545454, 0xa9a9a9, 0xFFFFFF};
static uint64_t colors[4] = {0x9bbc0f, 0x8bac0f, 0x306230, 0x0f380f};
//static uint64_t colors[4] = {0xb5af42, 0x919b3a, 0x5d782e, 0x5d782e};

void init_lcd() {
    if (!display) {
      display = XOpenDisplay(NULL);
      if (!display) {
	printf("error: could not initialize display\n");
	exit(1);
      }

      screen = DefaultScreen(display);
      window = XCreateSimpleWindow(display,
				   RootWindow(display, screen),
				   10,
				   10,
				   SCALED_SCREEN_X,
				   SCALED_SCREEN_Y,
				   1,
				   BlackPixel(display, screen),
				   WhitePixel(display, screen));

      // input events
      XSelectInput(display, window, KeyReleaseMask | KeyPressMask);

      // map to make visible
      XMapWindow(display, window);

      pixmap =  XCreatePixmap(display, window, SCALED_SCREEN_X, SCALED_SCREEN_Y, 24);

      fb = malloc(SCALED_SCREEN_X * SCALED_SCREEN_Y * 4);

      image = XCreateImage(display, DefaultVisual(display, screen), DefaultDepth(display, screen),
			   ZPixmap, 0, (char*)fb, SCALED_SCREEN_X, SCALED_SCREEN_Y, 32, 0);


      // create graphics context
      gc = XCreateGC(display, pixmap, 0, NULL);
      XFlush(display);
    }
}

//
// TODO: do the modes properly

static inline void set_mode(struct lcd_controller *lcd_c,  enum lcd_mode m) {
  lcd_c->regs[rSTAT] &= 0xFC;
  lcd_c->regs[rSTAT] |= m;
}

static inline int is_bg_on(struct lcd_controller *lcd_c) {
  return b_is_on(lcd_c->regs[rLCDC], 0);
}

static inline int is_obj_on(struct lcd_controller *lcd_c) {
  return b_is_on(lcd_c->regs[rLCDC], 1);
}

static inline int is_obj_8x8(struct lcd_controller *lcd_c) {
  return !b_is_on(lcd_c->regs[rLCDC], 2);
}

static inline int is_bg_code_area_upper(struct lcd_controller *lcd_c) {
  return b_is_on(lcd_c->regs[rLCDC], 3);
}

static inline int is_char_area_upper(struct lcd_controller *lcd_c) {
  return !b_is_on(lcd_c->regs[rLCDC], 4);
}

static inline int is_wdw_on(struct lcd_controller *lcd_c) {
  return b_is_on(lcd_c->regs[rLCDC], 5);
}

static inline int is_wdw_code_area_upper(struct lcd_controller *lcd_c) {
  return b_is_on(lcd_c->regs[rLCDC], 6);
}

static inline int is_on(struct lcd_controller *lcd_c) {
  return b_is_on(lcd_c->regs[rLCDC], 7);
}

static void paint(struct lcd_controller *lcd_c) {
  XPutImage(display, pixmap, gc, image, 0, 0, 0, 0, SCALED_SCREEN_X, SCALED_SCREEN_Y);
  XCopyArea(display, pixmap, window, gc, 0, 0, SCALED_SCREEN_X, SCALED_SCREEN_Y, 0, 0);
  XFlush(display);
}

static inline void clear_screen(struct lcd_controller *lcd_c) {
  for (int p = 0; p < SCALED_SCREEN_X * SCALED_SCREEN_Y; p++)
    ((int*)fb)[p] = colors[0];
  paint(lcd_c);
}

static inline u16 get_chr_line(struct lcd_controller *lcd_c, u16 chr_addr, u8 y, u8 attr) {
  int len = is_obj_8x8(lcd_c) ? 8 : 16;
  y &= (len-1);
  if (b_is_on(attr, 6)) { // flip y
    y = len-1-y;
  }
  u16 chr_line = lcd_c->vram[chr_addr - 0x8000 + y*2] << 8;
  chr_line |= lcd_c->vram[chr_addr - 0x8000 + y*2 + 1];
  return chr_line;
}

static inline u8 get_chr_line_color_idx(u16 chr_line, u8 x, u8 attr) {
  x &= 7;
  if (!b_is_on(attr, 5)) {
    x = 7-x;
  }

  int lower_bit = (chr_line>>(8+x)) & 1;
  int upper_bit = (chr_line>>x) & 1;
  return (upper_bit << 1) + lower_bit;
}

static inline u8 get_color_id_from_palette(u8 color_idx, u8 palette) {
  return (palette >> (color_idx<<1)) & 0x3;
}

static void load_tiles_by_line(struct lcd_controller *lcd_c,
			       u8 layer[][256],
			       u8 line,
			       int is_upper_code_area,
			       int is_upper_chr_area) {
  int code_select_addr = is_upper_code_area ? 0x9C00 : 0x9800;
  int char_select_addr = is_upper_chr_area ? 0x8800 : 0x8000;

  int x = 0;
  while (x < 256) {
    int tile_idx = (line/8*32) + x/8;
    int tile_addr = code_select_addr + tile_idx;
    u8 chr_code = lcd_c->vram[tile_addr-0x8000];

    if (char_select_addr == 0x8800) { // NOTE: not documented in official dev manual
      chr_code = (chr_code + 128) % 256;
    }
    u16 chr_line = get_chr_line(lcd_c, char_select_addr + (chr_code<<4), line%8, 0);
    int tx = (tile_idx % 32)*8;
    u8 palette = lcd_c->regs[rBGP];

    for (int p = 0; p < 8; p++) {
      int color_idx = get_chr_line_color_idx(chr_line, p, 0);
      layer[line][tx+p] = get_color_id_from_palette(color_idx, palette) | (color_idx << 2);
    }
    x+=8;
  }
}

static void load_bg_line(struct lcd_controller *lcd_c) {
  if (is_bg_on(lcd_c)) {
    load_tiles_by_line(lcd_c,
		       lcd_c->bg,
		       (lcd_c->regs[rLY] + lcd_c->regs[rSCY]) % 256,
		       is_bg_code_area_upper(lcd_c),
		       is_char_area_upper(lcd_c));
  }
}

static void load_wdw_line(struct lcd_controller *lcd_c) {
  if (is_wdw_on(lcd_c)) {
    load_tiles_by_line(lcd_c,
		       lcd_c->wdw,
		       lcd_c->regs[rLY],
		       is_wdw_code_area_upper(lcd_c),
		       is_char_area_upper(lcd_c));
  }
}


static void load_oam(struct lcd_controller *lcd_c) {
  lcd_c->oam.length = 0;
  u8 *oaddr = &lcd_c->vram[0x2000];
  int is_8x8 = is_obj_8x8(lcd_c);
  u8 num_per_line[256] = {0}; // TODO: nit, make this a bitfield?
  for (int i = 0; i < 160; i+=4) {
    int y = oaddr[i];
    if ((is_8x8 && y < 8) || (!is_8x8 && y == 0) || num_per_line[y] == 10)
      continue;
    num_per_line[y]++;

    struct obj obj = {
      .y = y,
      .x = oaddr[i+1],
      .chr_code = oaddr[i+2],
      .attr =  oaddr[i+3],
    };

    lcd_c->oam.objs[lcd_c->oam.length++] = obj;
    for (int l = lcd_c->oam.length-1; l > 0 && (lcd_c->oam.objs[l].x < lcd_c->oam.objs[l-1].x); l--) {
      struct obj temp = lcd_c->oam.objs[l-1];
      lcd_c->oam.objs[l-1] = lcd_c->oam.objs[l];
      lcd_c->oam.objs[l] = temp;
    }
  }
}

static void scan_line(struct lcd_controller *lcd_c) {
  // TODO: cache these
  load_bg_line(lcd_c);
  load_wdw_line(lcd_c);
  load_oam(lcd_c);

  int y = lcd_c->regs[rLY];
  int scx = lcd_c->regs[rSCX];
  int scy = lcd_c->regs[rSCY];
  int wx = lcd_c->regs[rWX];
  int wy = lcd_c->regs[rWY];

  u8 oam_pixels[160] = {0};
  int obj_on = is_obj_on(lcd_c);
  if (obj_on) { // compute oam line pixels
    int height = is_obj_8x8(lcd_c) ? 8 : 16;
    for (int oi = 0; oi < lcd_c->oam.length; oi++) {
      struct obj *obj  = &lcd_c->oam.objs[oi];
      if (obj->y-16+height <= y || obj->y-16 > y)
	continue;

      u8 palette = lcd_c->regs[b_is_on(obj->attr, 4) ? rOBP1 : rOBP0];
      u16 chr_line = get_chr_line(lcd_c,
				  0x8000 + (obj->chr_code * 16),
				  y-(obj->y-16),
				  obj->attr);

      for (int p = 0, x = obj->x-8; p < 8; p++, x++) {
	if (x >= 0 && x < 160) {
	  int color_idx = get_chr_line_color_idx(chr_line, p, obj->attr);
	  if (b_is_off(oam_pixels[x], 5) /* first time written */ ||
	      (!(oam_pixels[x] & (0x3<<2)) && color_idx) /* pixel already written but we override color index when 0 */ ) {

	    int pixel = get_color_id_from_palette(color_idx, palette);
	    pixel |= (color_idx << 2);
	    b_set_on(pixel, 5); // indicate color has been set by a previous obj
	    if (b_is_on(obj->attr, 7))
	      b_set_on(pixel, 4); // non-zero bg/wdw color indices drawn over this obj

	    oam_pixels[x] = pixel;
	  }
	}
      }
    }
  }

  for (int x = scx; x < SCREEN_X + scx; x++) {
    int color_idx;
    int color_id;
    int fx = x-scx;

    // base is background or window
    if (is_wdw_on(lcd_c) &&
	y >= wy &&
	fx >= wx-7) {
      color_id = lcd_c->wdw[y-wy][fx-(wx-7)];
    } else {
      color_id = lcd_c->bg[(y+scy)%256][x%256];
    }
    color_idx = color_id >> 2;
    color_id &= 0x3;

    if (obj_on &&
	b_is_on(oam_pixels[fx], 5) && // oam pixel exists
	(oam_pixels[fx] & (0x3<<2)) && // color index is not zero
	!(b_is_on(oam_pixels[fx], 4) && color_idx)) {
      color_id = oam_pixels[fx] & 0x3;
    }

    // write pixel
    for (int dx = 0, sx = fx * PIXEL_SCALAR; dx < PIXEL_SCALAR; dx++) {
      for (int dy = 0, sy = y * PIXEL_SCALAR; dy < PIXEL_SCALAR; dy++) {
	fb[sy+dy][sx+dx] = colors[color_id];
      }
    }
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
  if (!is_on(lcd_c))
    return;

  if (lcd_c->regs[rLY] < 144){
    if (lcd_c->t_cycles_since_last_line_refresh == 0) {
      set_mode(lcd_c, MODE_OAM_IN_USE);
      if (b_is_on(lcd_c->regs[rSTAT], 5))
	interrupt(lcd_c->interrupt_c, LCDC_STAT);
      load_oam(lcd_c);
    } else if (lcd_c->t_cycles_since_last_line_refresh == 80) {
      set_mode(lcd_c, MODE_OAM_AND_VRAM_IN_USE);
      scan_line(lcd_c);
    } else if (lcd_c->t_cycles_since_last_line_refresh == 252) {
      set_mode(lcd_c, MODE_HBLANK);
      if (b_is_on(lcd_c->regs[rSTAT], 3))
	interrupt(lcd_c->interrupt_c, LCDC_STAT);
    }
  }
  lcd_c->t_cycles_since_last_line_refresh++;
  if (lcd_c->t_cycles_since_last_line_refresh == 457) {
    lcd_c->t_cycles_since_last_line_refresh = 0;
    lcd_c->regs[rLY]++;
    if (b_is_on(lcd_c->regs[rSTAT], 6) && lcd_c->regs[rLY] == lcd_c->regs[rLYC])
      interrupt(lcd_c->interrupt_c, LCDC_STAT);
    if (lcd_c->regs[rLY] == 144) {
      paint(lcd_c);
      set_mode(lcd_c, MODE_VBLANK);
      if (b_is_on(lcd_c->regs[rSTAT], 4))
      	interrupt(lcd_c->interrupt_c, LCDC_STAT);
      interrupt(lcd_c->interrupt_c, VBLANK);
    } else if (lcd_c->regs[rLY] > 153) {
      lcd_c->regs[rLY] = 0;
    }
  }
}

u8 inline lcd_vram_read(struct lcd_controller* lcd_c, u16 addr) {
  int mode = lcd_c->regs[rSTAT] & 3;
  if (addr < 0xA000) {
    return mode < 3 ? lcd_c->vram[addr-0x8000] : 0;
  } else {
    return mode < 2 ? lcd_c->vram[addr-0xFE00+0x2000] : 0;
  }
}

void inline lcd_vram_write(struct lcd_controller* lcd_c, u16 addr, u8 value) {
  int mode = lcd_c->regs[rSTAT] & 3;
  if (addr < 0xA000 && mode < 3) {
    lcd_c->vram[addr-0x8000] = value;
  } else if (addr >= 0xA000 && mode < 2) {
    lcd_c->vram[addr-0xFE00+0x2000] = value;
  }
}

u8 inline lcd_reg_read(struct lcd_controller* lcd_c, enum lcd_reg reg) {
  return lcd_c->regs[reg];
}

void lcd_reg_write(struct lcd_controller* lcd_c, enum lcd_reg reg, u8 value) {
  u8 old_val = lcd_c->regs[reg];
  switch (reg) {
  case rLCDC:
    lcd_c->regs[reg] = value;
    if (!b_is_on(value, 7)) { // turning off
      lcd_c->regs[rLY] = 0;
      lcd_c->t_cycles_since_last_line_refresh = 0;
      clear_screen(lcd_c);
      set_mode(lcd_c, MODE_HBLANK); // TODO: is this necessary?
    }
    break;
  case rSTAT:
    old_val &= 0x3;
    value &= 0xFC;
    lcd_c->regs[rSTAT] = old_val | value;
    break;
  default:
    lcd_c->regs[reg] = value;
    break;
  }
}
