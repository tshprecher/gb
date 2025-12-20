#include <stdio.h>
#include "lcd.h"


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
