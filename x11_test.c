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

  char *filename = "tmp/dump_with_char_data.bin";
  FILE *fin = fopen(filename, "rb");
  if (NULL == fin) {
    fprintf(stderr, "error opening file: %s\n", filename);
    exit(1);
  }

  fseek(fin, 0x8000, SEEK_SET);

  uint64_t colors[4] = {0xFFFFFFFF, 0xCCCCCC, 0x7F7F7F, 0x00000000};
  /* 6. Event loop */
  while (1) {
    XNextEvent(display, &event);

    if (event.type == Expose) {
      // Set the color for the filled rectangle (e.g., a light gray pixel value)
      XSetForeground(display, gc, colors[1]);
      // Draw a filled rectangle: XFillRectangle(display, drawable, gc, x, y, width, height)


      char chr[16];
      size_t count = fread(&chr, 1, 16, fin);
      if (count != 16) {
	fprintf(stderr, "error reading full 16 bytes CHR\n");
	exit(1);
      }

      for (int c = 0; c < 16; c+=2) {
	for (int b = 7; b >= 0; b--) {
	  int upper = (chr[c] & (1 << b)) != 0;
	  int lower = (chr[c+1] & (1 << b)) != 0;
	  int color = (upper << 1) + lower;
	  printf("DEBUG: color -> %d\n", color);
	  XSetForeground(display, gc, colors[color]);
	  int row = c >> 1;
	  int column = 7-b;
	  XFillRectangle(display, window, gc, column * 20, row*20, 20, 20);
	}
      }


      //XFillRectangle(display, window, gc, 10, 10, 20, 20);


      // Change color for the outline rectangle (e.g., a darker gray)
      //      XSetForeground(display, gc, colors[2]);
      // Draw an outline rectangle: XDrawRectangle(display, drawable, gc, x, y, width, height)
      //      XDrawRectangle(display, window, gc, 10, 10, 10, 10);

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
