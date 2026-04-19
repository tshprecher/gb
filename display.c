#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
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

static uint64_t colors[4] = {0xFFFFFF, 0xa9a9a9, 0x545454, 0x000000};
//static uint64_t colors[4] = {0x000000, 0x545454, 0xa9a9a9, 0xFFFFFF};
//static uint64_t colors[4] = {0x9bbc0f, 0x8bac0f, 0x306230, 0x0f380f};
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
			   ZPixmap, 0, fb, SCALED_SCREEN_X, SCALED_SCREEN_Y, 32, 0);


      // create graphics context
      gc = XCreateGC(display, pixmap, 0, NULL);
      XFlush(display);
    }
}

//
// TODO: do the modes properly

static inline void set_mode(struct lcd_controller *lcd_c,  enum lcd_mode m) {
  //  printf("debug (display) STAT before: 0x%02X\n", lcd_c->regs[rSTAT]);
  lcd_c->regs[rSTAT] &= 0xFC;
  lcd_c->regs[rSTAT] |= m;
  //  printf("debug (display) STAT after: 0x%02X, mode set: %d\n", lcd_c->regs[rSTAT], m);
}

static inline int is_bg_on(struct lcd_controller *lcd_c) {
  return is_bit_set(lcd_c->regs[rLCDC], 0);
}

static inline int is_obj_on(struct lcd_controller *lcd_c) {
  return is_bit_set(lcd_c->regs[rLCDC], 1);
}

static inline int is_obj_8x8(struct lcd_controller *lcd_c) {
  return !is_bit_set(lcd_c->regs[rLCDC], 2);
}

static inline int is_bg_code_area_upper(struct lcd_controller *lcd_c) {
  return is_bit_set(lcd_c->regs[rLCDC], 3);
}

static inline int is_char_area_upper(struct lcd_controller *lcd_c) {
  return !is_bit_set(lcd_c->regs[rLCDC], 4);
}

static inline int is_wdw_on(struct lcd_controller *lcd_c) {
  return is_bit_set(lcd_c->regs[rLCDC], 5);
}

static inline int is_wdw_code_area_upper(struct lcd_controller *lcd_c) {
  return is_bit_set(lcd_c->regs[rLCDC], 6);
}

static inline int is_on(struct lcd_controller *lcd_c) {
  return is_bit_set(lcd_c->regs[rLCDC], 7);
}

static inline void clear_screen(struct lcd_controller *lcd_c) {
  memset((void*)fb, 0, SCALED_SCREEN_X * SCALED_SCREEN_Y * 4);
  // TODO: consolidate these x11 lines into helper
  XPutImage(display, pixmap, gc, image, 0, 0, 0, 0, SCALED_SCREEN_X, SCALED_SCREEN_Y);
  XCopyArea(display, pixmap, window, gc, 0, 0, SCALED_SCREEN_X, SCALED_SCREEN_Y, 0, 0);
  XFlush(display);
}

static void paint(struct lcd_controller *lcd_c) {
  XPutImage(display, pixmap, gc, image, 0, 0, 0, 0, SCALED_SCREEN_X, SCALED_SCREEN_Y);
  XCopyArea(display, pixmap, window, gc, 0, 0, SCALED_SCREEN_X, SCALED_SCREEN_Y, 0, 0);
  XFlush(display);
}


static inline u16 get_chr_line(struct lcd_controller *lcd_c, u16 chr_addr, u8 y, u8 attributes) {
  int len = is_obj_8x8(lcd_c) ? 8 : 16;
  y &= (len-1);
  if (is_bit_set(attributes, 6)) { // flip y
    y = len-1-y;
  }
  u16 chr_line = lcd_c->vram[chr_addr - 0x8000 + y*2] << 8;
  chr_line |= lcd_c->vram[chr_addr - 0x8000 + y*2 + 1];
  return chr_line;
}

static inline u8 get_chr_line_color_idx(u16 chr_line, u8 x, u8 attributes) {
  x &= 7;
  if (!is_bit_set(attributes, 5)) {
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
  //  printf("debug (display): load_tiles_by_line line -> %d\n", line);
  int code_select_addr = is_upper_code_area ? 0x9C00 : 0x9800;
  int char_select_addr = is_upper_chr_area ? 0x8800 : 0x8000;

  int x = 0;
  while (x < 256) {
    int tile_idx = (line/8*32) + x/8;
    int tile_addr = code_select_addr + tile_idx;
    u8 chr_code = lcd_c->vram[tile_addr-0x8000];
    /*    printf("debug (display): tile index -> %d, char_sel_addr -> 0x%04X, line -> %d, x -> %d, chr_code -> %d\n",
	   tile_idx,
	   char_select_addr,
	   line,
	   x,
	   chr_code);*/
    if (char_select_addr == 0x8800) { // NOTE: not documented in official dev manual
      chr_code = (chr_code + 128) % 256;
    }
    u16 chr_line = get_chr_line(lcd_c, char_select_addr + (chr_code<<4), line%8, 0);
    int tx = (tile_idx % 32)*8;
    u8 palette = lcd_c->regs[rBGP];

    for (int p = 0; p < 8; p++) {
      int color_idx = get_chr_line_color_idx(chr_line, p, 0);
      layer[line][tx+p] = get_color_id_from_palette(color_idx, palette) | (color_idx << 6);
    }
    x+=8;
  }
}

static void load_bg_line(struct lcd_controller *lcd_c) {
  // TODO: when off set the background to the proper null color, not always black
  if (is_bg_on(lcd_c)) {
    //    printf("debug(display): load_bg_line\n");
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
  printf("debug (display): inside load_oam\n");
  // TODO: skip if objects are unchanged
  lcd_c->oam[0] = 0;

  u8 active_cols[40] = {0};
  u8 num_per_col[40] = {0};
  u8 num_per_line[256] = {0}; // TODO: nit, make this a bitfield?
  int is_8x8 = is_obj_8x8(lcd_c);
  u8 *oaddr = &lcd_c->vram[0x2000];
  for (int i = 0; i < 160; i+=4) {
    int y = oaddr[i];
    if ((is_8x8 && y < 8) || (!is_8x8 && y == 0) || num_per_line[y] == 10)
      continue;
    num_per_line[y]++;

    int x = oaddr[i+1];
    int a = 0;
    while (a < lcd_c->oam[0] && active_cols[a] != x)
      a++;
    num_per_col[a]++;

    if (a == lcd_c->oam[0]) { // insertion sort descending
      active_cols[lcd_c->oam[0]] = x;
      for (int s = lcd_c->oam[0]; s > 0 && active_cols[s] > active_cols[s-1]; s--) {
	// swap col
	active_cols[s] ^= active_cols[s-1];
	active_cols[s-1] ^= active_cols[s];
	active_cols[s] ^= active_cols[s-1];

	// swap num per col
	num_per_col[s] ^= num_per_col[s-1];
	num_per_col[s-1] ^= num_per_col[s];
	num_per_col[s] ^= num_per_col[s-1];
      }
      lcd_c->oam[0]++;
    }
  }

  u8 *cur_col = &lcd_c->oam[1];
  for (int a = 0; a < lcd_c->oam[0]; a++) {
    cur_col[0] = active_cols[a];
    cur_col[1] = num_per_col[a];
    cur_col += (2 + num_per_col[a]*4);
  }

  u8 filled_by_col[40] = {0};
  memset(num_per_line, 0, 256);
  for (int i = 0; i < 160; i+=4) {
    int y = oaddr[i];
    if ((is_8x8 && y < 8) || (!is_8x8 && y == 0) || num_per_line[y] == 10)
      continue;
    num_per_line[y]++;

    int x = oaddr[i+1];
    int f = 0;
    cur_col = &lcd_c->oam[1];
    while (cur_col[0] != x) {
      cur_col += 2 + cur_col[1]*4;
      f++;
    }

    u8 *obj = cur_col + 2 + filled_by_col[f]*4;
    obj[0] = i>>2;
    obj[1] = oaddr[i];
    obj[2] = oaddr[i+2];
    obj[3] = oaddr[i+3];
    filled_by_col[f]++;
    //printf("debug (display): loaded object at y -> %d, x -> %d\n", y, x);
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
  int fy = y; // TODO: not necessary variable

  u8 oam_pixels[160] = {0};
  if (is_obj_on(lcd_c)) { // compute oam line pixels
    //printf("debug (display): obj on\n");
    int height = is_obj_8x8(lcd_c) ? 8 : 16;
    u8 *col = &lcd_c->oam[1];
    for (int c = 0; c < lcd_c->oam[0]; c++) {
      for (int b = 0; b < col[1]; b++) {
	u8 *obj = &col[2+b*4];
	if (obj[1]-16+height <= y || obj[1]-16 > y)
	  continue;

	u8 attr = obj[3];
	u8 palette = lcd_c->regs[is_bit_set(attr, 4) ? rOBP1 : rOBP0];
	u16 chr_line = get_chr_line(lcd_c, 0x8000 + (obj[2] * 16), y-(obj[1]-16) , attr);
	for (int p = 0; p < 8; p++) {
	  if (col[0]-8+p >= 0 && col[0]-8+p < 160) {
	    int color_idx = get_chr_line_color_idx(chr_line, p, attr);
	    if (!(oam_pixels[col[0]-8+p] & 0x80) ||
		(!(oam_pixels[col[0]-8+p] & (0x3<<4)) && color_idx)) {
	      //printf("debug (display): found object to display, attr -> 0x%02X, color_idx -> %d\n", attr, color_idx);

	      int pixel = get_color_id_from_palette(color_idx, palette);
	      pixel |= 0x80; // upper bit indicates value is set
	      if (is_bit_set(attr, 7))
		pixel |= 0x40;
	      pixel |= color_idx << 4;

	      oam_pixels[col[0]-8+p] = pixel;
	    }

	  }
	}
      }
      col += 2 + col[1]*4;
    }
  }

  //printf("debug (display): scy -> %d, scx -> %d, fy -> %d\n", scy, scx, fy);

  for (int x = scx; x < SCREEN_X + scx; x++) {
    int color_idx;
    int color_id;
    int fx = x-scx;

    // base is background or window
    if (is_wdw_on(lcd_c) &&
	y >= wy &&
	fx >= wx-7) {
      color_id = lcd_c->wdw[fy-wy][fx-(wx-7)];
    } else {
      color_id = lcd_c->bg[(fy+scy)%256][x%256];
    }
    color_idx = color_id >> 6;
    color_id &= 0x3;

    if (is_obj_on(lcd_c) &&
	(oam_pixels[fx] & 0x80) &&
	(oam_pixels[fx] & (0x3 << 4)) &&
	!((oam_pixels[fx] & 0x40) && color_idx)) {
      //printf("debug (display): writing object pixel\n");
      color_id = oam_pixels[fx] & 0x3;
    }

    // write pixel
    for (int dx = 0, sx = fx * PIXEL_SCALAR; dx < PIXEL_SCALAR; dx++) {
      for (int dy = 0, sy = fy * PIXEL_SCALAR; dy < PIXEL_SCALAR; dy++) {
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
      if (lcd_c->regs[rSTAT] & (1 << 5))
	interrupt(lcd_c->interrupt_c, LCDC_STAT);
      load_oam(lcd_c);
    } else if (lcd_c->t_cycles_since_last_line_refresh == 80) {
      set_mode(lcd_c, MODE_OAM_AND_VRAM_IN_USE);
      scan_line(lcd_c);
    } else if (lcd_c->t_cycles_since_last_line_refresh == 252) {
      set_mode(lcd_c, MODE_HBLANK);
      if (lcd_c->regs[rSTAT] & (1 << 3))
	interrupt(lcd_c->interrupt_c, LCDC_STAT);
    }
  }
  lcd_c->t_cycles_since_last_line_refresh++;
  if (lcd_c->t_cycles_since_last_line_refresh == 457) {
    lcd_c->t_cycles_since_last_line_refresh = 0;
    lcd_c->regs[rLY]++;
    if ((lcd_c->regs[rSTAT] & (1 << 6)) && lcd_c->regs[rLY] == lcd_c->regs[rLYC])
      interrupt(lcd_c->interrupt_c, LCDC_STAT);
    if (lcd_c->regs[rLY] == 144) {
      paint(lcd_c);
      set_mode(lcd_c, MODE_VBLANK);
      if (lcd_c->regs[rSTAT] & (1 << 4))
      	interrupt(lcd_c->interrupt_c, LCDC_STAT);
      interrupt(lcd_c->interrupt_c, VBLANK);
    } else if (lcd_c->regs[rLY] > 153) {
      lcd_c->regs[rLY] = 0;
    }
  }
}

u8 inline lcd_vram_read(struct lcd_controller* lcd_c, u16 addr) {
  int mode = lcd_c->regs[rSTAT] & 3;
  //  printf("debug (display): vram read mode -> %d\n", mode);
  if (addr < 0xA000) {
    return mode < 3 ? lcd_c->vram[addr-0x8000] : 0;
  } else {
    return mode < 2 ? lcd_c->vram[addr-0xFE00+0x2000] : 0;
  }
}

void inline lcd_vram_write(struct lcd_controller* lcd_c, u16 addr, u8 value) {
  int mode = lcd_c->regs[rSTAT] & 3;
  //  printf("debug (display): vram write mode -> %d\n", mode);
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
    if (!is_bit_eq(old_val, value, 7)) { // TODO: put this diff logic in macros
      if (!is_bit_set(value, 7)) {
	lcd_c->regs[rLY] = 0;
	lcd_c->t_cycles_since_last_line_refresh = 0;
	clear_screen(lcd_c);
	set_mode(lcd_c, MODE_HBLANK);
      }
    }
    break;
  case rSTAT:
    //    printf("debug (display): tried to write 0x%02X to rSTAT\n", value);
    old_val &= 0x3;
    value &= 0xFC;
    lcd_c->regs[rSTAT] = old_val | value;
    //printf("debug (display): completed write to rSTAT: %d\n", lcd_c->regs[rSTAT]);
    break;
  default:
    lcd_c->regs[reg] = value;
    break;
  }
}
