#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

struct sound_controller {
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
};

void init_audio();

void sc_write_N10(struct sound_controller*, uint8_t);
void sc_write_N11(struct sound_controller*, uint8_t);
void sc_write_N12(struct sound_controller*, uint8_t);
void sc_write_N13(struct sound_controller*, uint8_t);
void sc_write_N14(struct sound_controller*, uint8_t);




#endif
