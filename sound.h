#ifndef SOUND_H
#define SOUND_H

#include "memory.h"
#include "types.h"

void global_init_sound();

enum sound_reg {
  rNR10 = 0,
  rNR11,
  rNR12,
  rNR13,
  rNR14,
  rNR21,
  rNR22,
  rNR23,
  rNR24,
  rNR30,
  rNR31,
  rNR32,
  rNR33,
  rNR34,
  rNR41,
  rNR42,
  rNR43,
  rNR44,
  rNR50,
  rNR51,
  rNR52
};

struct sound {
  u8 type;  // [1,2,3,4]

  // common fields
  int current_sample;
  int duration_samples;
  int samples_per_wave;
  u8 is_continuous;

  // square wave, optional sweep and envelope
  s8 waveform_duty_cycle;

  u8 sweep_time;
  u16 sweep_time_samples;
  int sweep_freq;
  u8 sweep_shift;
  u8 is_sweep_increasing;

  u8 env_value;
  int samples_per_env_step;
  u8 is_env_decreasing;

  // defined waveform
  u8* waveform;
  u8 output_level;  // [0,1,2,3]

  // white noise
  int freq_hz;
  int is_long_mode;  // long: 15 bit shift register, short: 7 bit
  int lfsr_shift_register;
};

struct sound_controller {
  // registers
  u8 regs[21];

  // waveform ram for use in sound type 3
  u8 wram[16];

  // at most four types of sounds playing concurrently
  struct sound sounds[4];
  uint32_t t_cycles;
};

void init_sound(struct sound_controller*);
void sound_tick(struct sound_controller*);
u8 sound_read_wram(struct sound_controller*, u16);
u8 sound_read_reg(struct sound_controller*, enum sound_reg);
void sound_write_wram(struct sound_controller*, u16, u8);
void sound_write_reg(struct sound_controller*, enum sound_reg, u8);

#endif
