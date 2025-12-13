
#define rLCDC 0xFF40
#define rSTAT 0xFF41
#define rSCY  0xFF42
#define rSCX  0xFF43
#define rLY   0xFF44
#define rLYC  0xFF45
#define rDMA  0xFF46
#define rBGP  0xFF47
#define rOBP0 0xFF48
#define rOBP1 0xFF49
#define rWY   0xFF4A
#define rWX   0xFF4B

struct lcd_controller {
  uint32 t_cycle;

  // TODO: put global in its own memory controller, shared by call components?
  uint8 *global_ram; // for lcd registers,
  uint8 local_ram[0x2000];
};

void lcd_init(struct *lcd_controller *lcd) {
  lcd->global_ram[rLCDC] = 0x83;
  lcd->global_ram[rSCY] = 0;
  lcd->global_ram[rSCX] = 0;
  lcd->global_ram[rLYC] = 0;
  lcd->global_ram[rWY] = 0;
  lcd->global_ram[rWX] = 0;
}

uint32 lcd_tick(struct lcd_controller *lcd) {
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
  return 0;
}
