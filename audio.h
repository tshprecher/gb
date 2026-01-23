#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

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

  // at most four types of sounds playing concurrently
  struct playback playing[4];
  uint32_t t_cycles;
};

void init_audio();
void audio_tick(struct sound_controller*);

void sc_write_NR10(struct sound_controller*, uint8_t);
void sc_write_NR11(struct sound_controller*, uint8_t);
void sc_write_NR12(struct sound_controller*, uint8_t);
void sc_write_NR13(struct sound_controller*, uint8_t);
void sc_write_NR14(struct sound_controller*, uint8_t);

void sc_write_NR21(struct sound_controller*, uint8_t);
void sc_write_NR22(struct sound_controller*, uint8_t);
void sc_write_NR23(struct sound_controller*, uint8_t);
void sc_write_NR24(struct sound_controller*, uint8_t);

void sc_write_NR50(struct sound_controller*, uint8_t);
void sc_write_NR51(struct sound_controller*, uint8_t);
void sc_write_NR52(struct sound_controller*, uint8_t);




#endif
