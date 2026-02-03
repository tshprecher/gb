#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>
#include "memory.h"

enum sound_reg {
  rNR10 = 0,  rNR11,  rNR12,  rNR13,  rNR14,
  rNR21, rNR22, rNR23, rNR24,
  rNR30, rNR31, rNR32, rNR33, rNR34,
  rNR41, rNR42, rNR43, rNR44,
  rNR50, rNR51, rNR52
};

struct sound {
  uint8_t type; // [1,2,3,4]

  // common fields
  int current_sample;
  int duration_samples;
  uint8_t is_continuous;
  int frequency;
  int samples_per_wave;

  // for sound 1

  int8_t sweep_time_samples;
  int8_t sweep_shift;
  int8_t is_sweep_decreasing;

  // for sounds 1, 2
  int8_t waveform_duty_cycle;
  int samples_per_env_step;
  uint8_t env_value;
  int8_t is_env_decreasing;

  // for sound 3
  uint8_t waveform[32];
  uint8_t output_level; //[0, 1, 2, 4]

};

struct sound_controller {
  // registers
  uint8_t regs[21];

  struct mem_controller *mc;

  // at most four types of sounds playing concurrently
  struct sound sounds[4];
  uint32_t t_cycles;
};

void init_sound();
void sound_tick(struct sound_controller*);

uint8_t sound_reg_read(struct sound_controller*, enum sound_reg);
void sound_reg_write(struct sound_controller*, enum sound_reg, uint8_t);

#endif
