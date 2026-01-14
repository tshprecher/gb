#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "audio.h"

pa_simple *s;
pa_sample_spec ss;


static int sc_play_sound_1(struct sound_controller *sc, pa_simple *s) {
  printf("debug: sc registers: %d, %d, %d, %d, %d\n",
	 sc->NR10, sc->NR11, sc->NR12, sc->NR13, sc->NR14);

  uint8_t sweep_shift_num = sc->NR10 & 3;
  uint8_t is_sweep_decrease = (sc->NR10 >> 3) & 1;
  uint8_t sweep_time_ts = (sc->NR10 >> 4) & 7;

  printf("info: sweep_time_ts: %d\n", sweep_time_ts);

  int freq_X = ((sc->NR14 & 7) << 8) + sc->NR13;
  int freq = (4194304 >> 5) / (2048 - freq_X);

  // TODO: handle continuous sound
  int sound_length_samples = (ss.rate * (64-(sc->NR11 & 0x4F))) >> 8;
  //  int sound_length_samples = 44100*2; // two seconds
  int sweep_time_samples = sweep_time_ts ? (ss.rate / freq) * sweep_time_ts : 0;


  printf("info: ss.rate: %d, freq_X: %d, freq: %d, sweep_time_samples: %d, sound_length_samples: %d\n",
	 ss.rate, freq_X, freq, sweep_time_samples, sound_length_samples);

  int16_t data[sound_length_samples];
  int error = 0;

  int d = 0;
  int samples_per_wave = ss.rate / freq;
  int waveform_duty_cycle = sc->NR11 >> 6;
  int sweep_count = 0;

  printf("info: samples_per_wave: %d, waveform_duty_cycle: %d\n", samples_per_wave, waveform_duty_cycle);
  while (d < sound_length_samples) {
    // each square wave can be split into 8 time slices of high/low
    uint8_t is_high = 0;
    int samples_per_wave_slice = samples_per_wave / 8;
    //    printf("debug: samples_per_wave before divide: %d\n", samples_per_wave);
    int current_wave_slice = (d * 8 / samples_per_wave) % 8; // TODO: simplify with above
    switch (waveform_duty_cycle) {
    case 0: // 12.5%
      is_high = current_wave_slice == 1 ? 1 : 0;
      break;
    case 1: // 25%
      is_high = current_wave_slice < 2 ? 1 : 0;
      break;
    case 2: // 50%
      is_high = current_wave_slice < 4 ? 1 : 0;
      break;
    case 3: // 75 %
      is_high = current_wave_slice >= 2 ? 1 : 0;
      break;
    }
    for (int ts = 0; ts < samples_per_wave_slice && d < sound_length_samples; ts++) {
      data[d++] = is_high ? 0x4000 : 0;
    }
    if (sweep_time_ts && (d / sweep_time_samples > sweep_count) && sweep_shift_num)  {
      sweep_count++;
      int diff = freq >> (1 << sweep_shift_num);
      if (is_sweep_decrease) {
	if (diff > 0) {
	  freq = freq - diff;
	}
      } else {
	freq = freq + diff;
      }
      // TODO: check if the result frequency is greater than 11 bits and stop
      printf("info: new freq: %d\n", freq);
      if (freq > (1 << 10)) {
	printf("warn: should stop here @ freq: %d\n", freq);
      }
      samples_per_wave = ss.rate / freq; // redefine samples per wave
      printf("debug: samples_per_wave set to %d\n", samples_per_wave);
    }
  }

  /*  for (int d = 0; d < sound_length_samples; d++) {
   printf("debug: gnu plot %d\t%d\n", d, data[d]);
   }*/

  pa_simple_write(s, data, sizeof(data), NULL);
  return error;
}

void init_audio() {
  ss.format = PA_SAMPLE_S16NE;
  ss.channels = 1;
  ss.rate = 44100;
  int error = 0;

  s = pa_simple_new(NULL,               // Use the default server.
		    "gameboy",       // Our application's name.
		    PA_STREAM_PLAYBACK,
		    NULL,               // Use the default device.
		    "test",          // Description of our stream.
		    &ss,                // Our sample format.
		    NULL,               // Use default channel map
		    NULL,               // Use default buffering attributes.
		    &error
		    );

}



void sc_write_NR10(struct sound_controller* sc, uint8_t byte) { sc->NR10 = byte; }
void sc_write_NR11(struct sound_controller* sc, uint8_t byte) { sc->NR11 = byte; }
void sc_write_NR12(struct sound_controller* sc, uint8_t byte) { sc->NR12 = byte; }
void sc_write_NR13(struct sound_controller* sc, uint8_t byte) { sc->NR13 = byte; }
void sc_write_NR14(struct sound_controller* sc, uint8_t byte) {
  sc->NR14 = byte;
  if (sc->NR14 & 0x80) {
    sc_play_sound_1(sc, s);
  }
}

void sc_write_NR50(struct sound_controller* sc, uint8_t byte) { sc->NR50 = byte; }
void sc_write_NR51(struct sound_controller* sc, uint8_t byte) { sc->NR51 = byte; }
void sc_write_NR52(struct sound_controller* sc, uint8_t byte) { sc->NR52 = byte; }
