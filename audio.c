#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "audio.h"

pa_simple *s;
pa_sample_spec ss;


struct sound {
  uint64_t uuid; // TODO: technically a serialization and not a standard uuid
  uint8_t type;
  uint8_t is_continuous;
  uint16_t *samples;
  int samples_len;
};

struct sound sound_cache[256];
int sound_cache_len = 0;

struct playback {
  struct sound *sound;
  int current_sample;
};

struct playback playbacks[2][4]; // 4 sounds per channel

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

  for (int ch = 0; ch < 2; ch++) {
    for (int s = 0; s < 4; s++) {
      playbacks[ch][s] = (struct playback){0};
    }
  }

}

// TODO: specify for each sound type just the bits that define the wave
static uint64_t create_sound_uuid(
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

struct sound * get_cached_sound(uint64_t uuid) {
  for (int i = 0; i < sound_cache_len; i++) {
    if (sound_cache[i].uuid == uuid) {
      return &sound_cache[i];
    }
  }
  printf("debug: missed cached sound with id: %ldd\n", uuid);
  return NULL;
}


struct sound * create_sound_1(struct sound_controller *sc) {
  struct sound * cached_sound = get_cached_sound(create_sound_uuid(sc->NR10,sc->NR11,sc->NR12,sc->NR13,sc->NR14));
  if (cached_sound)
    return cached_sound;

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

  uint16_t *data = malloc(sound_length_samples*sizeof(uint16_t));
  int d = 0;
  int samples_per_wave = ss.rate / freq;
  int waveform_duty_cycle = sc->NR11 >> 6;
  int sweep_count = 0;

  printf("info: samples_per_wave: %d, waveform_duty_cycle: %d\n", samples_per_wave, waveform_duty_cycle);
  while (d < sound_length_samples) {
    // each square wave can be split into 8 time slices of high/low
    uint8_t is_high = 0;
    int samples_per_wave_slice = samples_per_wave / 8;
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
    }
  }

  struct sound r = {
    .uuid = create_sound_uuid(sc->NR10,
			      sc->NR11,
			      sc->NR12,
			      sc->NR13,
			      sc->NR14),
    .type = 1,
    .is_continuous = ((sc->NR14 & 0x40) == 0),
    .samples = data,
    .samples_len = sound_length_samples,
  };
  sound_cache[sound_cache_len++] = r;

  return &sound_cache[sound_cache_len-1];
}


static int sound_enabled(struct sound_controller* sc, uint8_t sound_type) {
  sound_type &= 3;
  uint8_t nr51_mask = 0x11 << sound_type;
  uint8_t nr52_mask = 0x80 + (1 << sound_type);
  return (sc->NR51 & nr51_mask) && (sc->NR52 & nr52_mask);
}


void audio_tick(struct sound_controller *sc) {
  sc->t_cycles++;
  // TODO: consider bundling more than one sample at a time
  /*  for (int snd = 0; snd < 4; snd++) {
    struct playback pb = playbacks[0][snd];
    if (!pb.sound)
      continue;
    pa_simple_write(s, pb.sound->samples, pb.sound->samples_len*sizeof(uint16_t), NULL);
    }*/
  while (sc->t_cycles >= 95) { // TODO: compute this on init
    //printf("debug: computing samples\n");
    // iterate through all the playbacks and write a sample
    uint16_t samples[1];
    for (int s = 0; s < 1; s++) {
      samples[s] = 0;
    }
    // TODO gate on audio on
    for (int s = 0; s < 4; s++) {
      struct playback *pb = &playbacks[0][s];
      printf("debug: pb: %d, current sample: %d\n", pb->current_sample);
      if (!pb->sound)
	continue;
      for (int i = 0; i < 1; i++) {
	if (pb->sound->is_continuous) {
	  samples[i] += pb->sound->samples[pb->current_sample++];
	  pb->current_sample %= pb->sound->samples_len;
	} else if (pb->current_sample < pb->sound->samples_len) {
	  samples[i] += pb->sound->samples[pb->current_sample++];
	}
      }
    }
    /*    for (int s = 0; s < 1; s++) {
      printf("debug: adding sample: %d\n", samples[s]);
      }*/
    int error;
    pa_simple_write(s, samples, 1*sizeof(uint16_t), &error);
    if (error) {
      printf("debug: error code: %d\n", error);
      //printf("error str: %s\n", pa_strerror(error));
    }
    sc->t_cycles -= 95;
  }

}

void sc_write_NR10(struct sound_controller* sc, uint8_t byte) { sc->NR10 = byte; }
void sc_write_NR11(struct sound_controller* sc, uint8_t byte) { sc->NR11 = byte; }
void sc_write_NR12(struct sound_controller* sc, uint8_t byte) { sc->NR12 = byte; }
void sc_write_NR13(struct sound_controller* sc, uint8_t byte) { sc->NR13 = byte; }
void sc_write_NR14(struct sound_controller* sc, uint8_t byte) {
  sc->NR14 = byte;
  if (sc->NR14 & 0x80) {
    struct sound * sound = create_sound_1(sc);
    playbacks[0][0] = (struct playback) {.sound = sound, .current_sample = 0};
  }
}

void sc_write_NR50(struct sound_controller* sc, uint8_t byte) { sc->NR50 = byte; }
void sc_write_NR51(struct sound_controller* sc, uint8_t byte) { sc->NR51 = byte; }
void sc_write_NR52(struct sound_controller* sc, uint8_t byte) { sc->NR52 = byte; }
