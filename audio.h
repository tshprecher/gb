#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

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

  uint32_t t_cycles;
};

void init_audio();
void audio_tick(struct sound_controller*);

void sc_write_NR10(struct sound_controller*, uint8_t);
void sc_write_NR11(struct sound_controller*, uint8_t);
void sc_write_NR12(struct sound_controller*, uint8_t);
void sc_write_NR13(struct sound_controller*, uint8_t);
void sc_write_NR14(struct sound_controller*, uint8_t);

void sc_write_NR50(struct sound_controller*, uint8_t);
void sc_write_NR51(struct sound_controller*, uint8_t);
void sc_write_NR52(struct sound_controller*, uint8_t);




#endif
