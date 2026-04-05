#ifndef SOUND_H
#define SOUND_H

#include "types.h"
#include "memory.h"

enum sound_reg {
  rNR10 = 0,  rNR11,  rNR12,  rNR13,  rNR14,
  rNR21, rNR22, rNR23, rNR24,
  rNR30, rNR31, rNR32, rNR33, rNR34,
  rNR41, rNR42, rNR43, rNR44,
  rNR50, rNR51, rNR52
};

struct sound {
  u8 type; // [1,2,3,4]

  // common fields
  int frequency;
  int current_sample;
  int duration_samples;
  int samples_per_wave;
  u8 is_continuous; // TODO: combine is_continuous with duration_samples?

  // for sounds 1, 2
  s8 waveform_duty_cycle;

  // for sound 1
  s8 sweep_time_samples;
  s8 sweep_shift;
  s8 is_sweep_decreasing;

  // for sounds 3, 4
  u8 env_value;
  int samples_per_env_step;
  s8 is_env_decreasing;

  // for sound 3
  u8 waveform[32];
  u8 output_level; // [0,1,2,3]

  // for sound 4
  int is_long_mode; // long: 15 bit shift register, short: 7 bit
  int lfsr_shift_register;
};

struct sound_controller {
  // registers
  u8 regs[21];

  struct mem_controller *memory_c;

  // at most four types of sounds playing concurrently
  struct sound sounds[4];
  uint32_t t_cycles;
};

void init_sound();
void sound_tick(struct sound_controller*);

u8 sound_reg_read(struct sound_controller*, enum sound_reg);
void sound_reg_write(struct sound_controller*, enum sound_reg, u8);

#endif
