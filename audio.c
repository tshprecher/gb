#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "audio.h"

pa_simple *s;
pa_sample_spec ss;

int write_squarewave(pa_simple *s, int freq, int seconds) {
  uint8_t data[ss.rate*seconds];
  int error = 0;
  int period_length = ss.rate / freq;

  for (int d = 0; d < sizeof(data); d+=period_length) {
    for (int low = 0; low < period_length>>1; low++) {
      data[d+low] = 0;
    }
    for (int high = period_length>>1; high < period_length; high++) {
      data[d+high] = 0x80;
    }
  }

  pa_simple_write(s, data, sizeof(data), &error);
  return error;
}


void init_audio() {
    ss.format = PA_SAMPLE_U8;
  ss.channels = 1;
  ss.rate = 44100;
  //ss.rate = 440*10;
  int error = 0;

  s = pa_simple_new(NULL,               // Use the default server.
		    "pulse_hack",       // Our application's name.
		    PA_STREAM_PLAYBACK,
		    NULL,               // Use the default device.
		    "GB_test",          // Description of our stream.
		    &ss,                // Our sample format.
		    NULL,               // Use default channel map
		    NULL,               // Use default buffering attributes.
		    &error
		    );

}



void sc_write_N10(struct sound_controller* sc, uint8_t byte) {sc->NR
void sc_write_N11(struct sound_controller* sc, uint8_t byte);
void sc_write_N12(struct sound_controller* sc, uint8_t byte);
void sc_write_N13(struct sound_controller* sc, uint8_t byte);
void sc_write_N14(struct sound_controller* sc, uint8_t byte);
