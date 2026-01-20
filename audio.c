#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "audio.h"

pa_simple *s;
pa_sample_spec ss;

static struct sound cache[256];
static int cache_length = 0;

void init_audio() {
  ss.format = PA_SAMPLE_S16NE;
  ss.channels = 1;
  ss.rate = 44100;
  int error = 0;

  s = pa_simple_new(NULL,               // Use the default server.
		    "gsameboy",       // Our application's name.
		    PA_STREAM_PLAYBACK,
		    NULL,               // Use the default device.
		    "test",          // Description of our stream.
		    &ss,                // Our sample format.
		    NULL,               // Use default channel map
		    NULL,               // Use default buffering attributes.
		    &error
		    );
}

// TODO: specify for each sound type just the bits that define the wave
static uint64_t sound_id(
				  uint8_t r1,
				  uint8_t r2,
				  uint8_t r3,
				  uint8_t r4,
				  uint8_t r5) {
  uint64_t uuid = r1;
  uuid <<= 32;
  uuid |= (r2 << 24) | (r3 << 16) | (r4 << 8) | r5;
  return uuid;
}

struct sound * get_cached_sound(uint64_t id) {
  for (int i = 0; i < cache_length; i++) {
    if (cache[i].id == id) {
      return &cache[i];
    }
  }
  printf("debug: missed cached sound with id: %ldd\n", id);
  return NULL;
}

static struct sound * create_sound_1(struct sound_controller *sc) {
  struct sound * cached;
  if ((cached = get_cached_sound(sound_id(sc->NR10,sc->NR11,sc->NR12,sc->NR13,sc->NR14)))) {
    return cached;
  }

  uint8_t sweep_shift_num = sc->NR10 & 3;
  uint8_t is_sweep_decrease = (sc->NR10 >> 3) & 1;
  uint8_t sweep_time_ts = (sc->NR10 >> 4) & 7;

  printf("info: sweep_time_ts: %d, envelope reg: 0x%02X\n", sweep_time_ts, sc->NR12);

  int freq_X = ((sc->NR14 & 7) << 8) + sc->NR13;
  int freq = (4194304 >> 5) / (2048 - freq_X);

  int sound_length_samples = (ss.rate * (64-(sc->NR11 & 0x4F))) >> 8;
  int sweep_time_samples = sweep_time_ts ? (ss.rate / freq) * sweep_time_ts : 0;


  printf("info: ss.rate: %d, freq_X: %d, freq: %d, sweep_time_samples: %d, sound_length_samples: %d\n",
	 ss.rate, freq_X, freq, sweep_time_samples, sound_length_samples);

  int samples_per_wave = ss.rate / freq;
  int waveform_duty_cycle = sc->NR11 >> 6;
  int sweep_count = 0;

  uint16_t *data = malloc(sound_length_samples*sizeof(uint16_t));
  int d = 0;
  printf("info: samples_per_wave: %d, waveform_duty_cycle: %d\n", samples_per_wave, waveform_duty_cycle);
  while (d < sound_length_samples) {
    // each square wave can be split into 8 time slices of high/low
    uint8_t is_high = 0;
    int samples_per_wave_slice = samples_per_wave / 8;
    int current_wave_slice = (d * 8 / samples_per_wave) % 8;
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
      data[d++] = is_high ? 0x444 : 0;
    }

    // handle sweep
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
    }
  }


  // possibly pass through the samples to implement the envelope
  uint8_t env_steps = sc->NR12 & 7;
  uint8_t env_value = sc->NR12 >> 4;
  printf("debug: env_value factor: %d\n", env_value);
  uint8_t is_env_decrease = !((sc->NR12 >> 3) & 1);
  int env_steps_samples = ss.rate / 64 * env_steps;

  d = 0;
  while (d < sound_length_samples) {
    if (d && env_steps_samples && d % env_steps_samples == 0) {
      if (is_env_decrease && env_value > 0)
	env_value--;
      if (!is_env_decrease && env_value < 15)
	env_value++;
    }
    //printf("debug: multiplying env_value factor: %d\n", env_value);
    data[d++] *= env_value;
  }

  struct sound r = {
    .id = sound_id(sc->NR10,
		   sc->NR11,
		   sc->NR12,
		   sc->NR13,
		   sc->NR14),
    .is_continuous = ((sc->NR14 & 0x40) == 0),
    .samples = data,
    .length = sound_length_samples,
  };
  cache[cache_length++] = r;
  return &cache[cache_length-1];
}

void audio_tick(struct sound_controller *sc) {
  if (!(sc->NR52 & 0x80))
    return;

  sc->t_cycles++;
  // TODO: consider bundling more than one sample at a time
  while (sc->t_cycles >= 1500) { // TODO: compute this on init
    /*printf("debug: NR50: 0x%02X\n", sc->NR50);
    printf("debug: NR51: 0x%02X\n", sc->NR51);
    printf("debug: NR52: 0x%02X\n", sc->NR52);

    printf("debug: NR10: 0x%02X\n", sc->NR10);
    printf("debug: NR11: 0x%02X\n", sc->NR11);
    printf("debug: NR12: 0x%02X\n", sc->NR12);
    printf("debug: NR13: 0x%02X\n", sc->NR13);
    printf("debug: NR14: 0x%02X\n", sc->NR14);*/

    // iterate through all the playbacks and write a sample
    uint16_t samples[16];
    for (int s = 0; s < 16; s++) {
      samples[s] = 0;
    }
    // TODO gate on audio on
    for (int s = 0; s < 1; s++) {
      struct playback *pb = &sc->playing[s];
      if (!pb->sound || (!pb->sound->is_continuous && !(sc->NR52 & (1<<s))))
	continue;
      for (int i = 0; i < 16; i++) {
	samples[i] += pb->sound->samples[pb->current_sample++];
	if (pb->current_sample == pb->sound->length) {
	  if (pb->sound->is_continuous) {
	    pb->current_sample %= pb->sound->length;
	  } else {
	    sc->NR52 &= ~(1<<s); // done, turn the sound off
	    break;
	  }
	}
      }
    }
    int error;
    //printf("debug: sample: %d\n", samples[0]);
    pa_simple_write(s, samples, 16*sizeof(uint16_t), &error);
    if (error) {
      printf("debug: error code: %d\n", error);
      printf("debug: error str: %s\n", pa_strerror(error));
    }
    sc->t_cycles -= 1500;
  }
}

void sc_write_NR10(struct sound_controller* sc, uint8_t byte) { sc->NR10 = byte; }
void sc_write_NR11(struct sound_controller* sc, uint8_t byte) { sc->NR11 = byte; }
void sc_write_NR12(struct sound_controller* sc, uint8_t byte) { sc->NR12 = byte; }
void sc_write_NR13(struct sound_controller* sc, uint8_t byte) { sc->NR13 = byte; }

static int DEBUG_no_new_sounds = 0;

void sc_write_NR14(struct sound_controller* sc, uint8_t byte) {
  sc->NR14 = byte;
  if (sc->NR14 & 0x80 && !DEBUG_no_new_sounds) { // initialize bit on
    struct sound * sound = create_sound_1(sc);
    /*printf("debug: NR10: 0x%02X\n", sc->NR10);
    printf("debug: NR11: 0x%02X\n", sc->NR11);
    printf("debug: NR12: 0x%02X\n", sc->NR12);
    printf("debug: NR13: 0x%02X\n", sc->NR13);
    printf("debug: NR14: 0x%02X\n", sc->NR14);
    printf("debug: NR14 assign\n");*/

    //DEBUG_no_new_sounds = 1;

    sc->playing[0] = (struct playback) {
      .current_sample = 0,
      .sound = sound,
    };
  }
}

void sc_write_NR50(struct sound_controller* sc, uint8_t byte) { sc->NR50 = byte; }
void sc_write_NR51(struct sound_controller* sc, uint8_t byte) { sc->NR51 = byte; }
void sc_write_NR52(struct sound_controller* sc, uint8_t byte) { sc->NR52 = byte; }
