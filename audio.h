#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include "mem_controller.h"

struct sound {
  // unique id based on creation parameters
  uint64_t id;

  uint8_t is_continuous;

  // points to the series of samples to output sound card
  int16_t *samples;

  // number of samples in the series
  int length;
};

struct playback {
  struct sound *sound;
  int current_sample;
};


struct sound_controller {
  // registers
  uint8_t NR10,
    NR11,
    NR12,
    NR13,
    NR14,
    NR21,
    NR22,
    NR23,
    NR24,
    NR30,
    NR31,
    NR32,
    NR33,
    NR34,
    NR41,
    NR42,
    NR43,
    NR44,
    NR50,
    NR51,
    NR52;

  struct mem_controller *mc;

  // at most four types of sounds playing concurrently
  struct playback playing[4];
  uint32_t t_cycles;
};

void init_audio();
void audio_tick(struct sound_controller*);

// TODO: I hate these write functions, maybe one of the worse
// things i've ever done in software, even for an alpha version
void sc_write_NR10(struct sound_controller*, uint8_t);
void sc_write_NR11(struct sound_controller*, uint8_t);
void sc_write_NR12(struct sound_controller*, uint8_t);
void sc_write_NR13(struct sound_controller*, uint8_t);
void sc_write_NR14(struct sound_controller*, uint8_t);

void sc_write_NR21(struct sound_controller*, uint8_t);
void sc_write_NR22(struct sound_controller*, uint8_t);
void sc_write_NR23(struct sound_controller*, uint8_t);
void sc_write_NR24(struct sound_controller*, uint8_t);

void sc_write_NR30(struct sound_controller*, uint8_t);
void sc_write_NR31(struct sound_controller*, uint8_t);
void sc_write_NR32(struct sound_controller*, uint8_t);
void sc_write_NR33(struct sound_controller*, uint8_t);
void sc_write_NR34(struct sound_controller*, uint8_t);

void sc_write_NR50(struct sound_controller*, uint8_t);
void sc_write_NR51(struct sound_controller*, uint8_t);
void sc_write_NR52(struct sound_controller*, uint8_t);




#endif
