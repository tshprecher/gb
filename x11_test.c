#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <X11/Xlib.h>

bool quited = false;

void on_delete(Display * display, Window window)
{
  XDestroyWindow(display, window);
  quited = true;
}

extern int main(int argc, char *argv[])
{

  // SETUP WINDOW

  Display *display;
  Window window;
  XEvent event;
  GC gc;
  int screen;

  /* 1. Connect to the X server */
  display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "Cannot open display\n");
    exit(1);
  }

  screen = DefaultScreen(display);

  /* 2. Create a simple window */
  window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, 1000, 1000, 1,
			       BlackPixel(display, screen), WhitePixel(display, screen));

  /* 3. Select input events (we need Expose events to know when to draw) */
  XSelectInput(display, window, ExposureMask | KeyPressMask);

  /* 4. Create a graphics context (GC) */
  gc = XCreateGC(display, window, 0, NULL);
  XSetForeground(display, gc, BlackPixel(display, screen)); // Set foreground color for drawing

  /* 5. Map the window (make it visible) */
  XMapWindow(display, window);


  // READ CHAR DATA AND DISPLAY FIRST TILES

  // read from a dump file and draw rectangles based on the character data

  char *filename = "dump.bin";
  FILE *fin = fopen(filename, "rb");
  if (NULL == fin) {
    fprintf(stderr, "error opening file: %s\n", filename);
    exit(1);
  }

  // ordering here seems right
  uint64_t colors[4] = {0xFFFFFF, 0xa9a9a9, 0x545454, 0x000000};
  /* 6. Event loop */
  while (1) {
    XNextEvent(display, &event);
    if (event.type == Expose) {

      int addr = 0x9800;
      while (addr < 0x9C00) {
	fseek(fin, addr, SEEK_SET);
	// get char code
	int chr_code;
	fread(&chr_code, 1, 1, fin);
	printf("DEBUG: chr_code -> 0x%02X\n", chr_code);

	// print character
	fseek(fin, 0x8000 + (chr_code << 4), SEEK_SET);
	char chr[16];
	size_t count = fread(&chr, 1, 16, fin);

	int blockX = addr % 32;
	int blockY = addr / 32 - 1216;

	printf("DEBUG: blockX -> %d, blockY -> %d\n", blockX, blockY);

	for (int c = 0; c < 16; c+=2) {
	  for (int b = 7; b >= 0; b--) {
	    int upper = (chr[c] & (1 << b)) != 0;
	    int lower = (chr[c+1] & (1 << b)) != 0;
	    int color = (upper << 1) + lower;
	    printf("DEBUG: color -> %d\n", color);

	    // set the pixel color
	    XSetForeground(display, gc, colors[color]);
	    int row = c >> 1;
	    int column = 7-b;
	    //XFillRectangle(display, window, gc, column * 20, (chrs*160) + row*20, 20, 20);
	    XFillRectangle(display, window, gc, blockX*40 + column*5, blockY*40 + row*5, 5, 5);
	  }
	}

	addr++;
      }

      /* Flush the drawing commands to the X server immediately */
      XFlush(display);
    }

    if (event.type == KeyPress) {
      break; // Exit loop on any key press
    }
  }

  /* 7. Clean up and close connection */
  XDestroyWindow(display, window);
  XCloseDisplay(display);

  return 0;
}
