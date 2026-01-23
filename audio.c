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
  ss.channels = 2;
  ss.rate = 44100;

  s = pa_simple_new(NULL,               // Use the default server.
		    "gameboy",          // Our application's name.
		    PA_STREAM_PLAYBACK,
		    NULL,               // Use the default device.
		    "test",             // Description of our stream.
		    &ss,                // Our sample format.
		    NULL,               // Use default channel map
		    NULL,               // Use default buffering attributes.
		    NULL
		    );
}

// TODO: specify for each sound type just the bits that define the wave
static int64_t sound_id(uint8_t r1,
			uint8_t r2,
			uint8_t r3,
			uint8_t r4,
			uint8_t r5) {
  int64_t id = r1;
  id <<= 32;
  return (id | (r2 << 24) | (r3 << 16) | (r4 << 8) | r5);
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

static int generate_square_wave(int16_t **samples,
				int frequency,
				int8_t waveform_duty_cycle,
				int8_t sweep_time,
				int8_t sweep_shift,
				int8_t is_sweep_decreasing,
				int duration_ms) {

  printf("debug: NEW waveform duty cycle: %d\n", waveform_duty_cycle);
  int sweep_time_samples = sweep_time ? (ss.rate * sweep_time / frequency) : 0;
  int samples_per_wave = ss.rate / frequency;

  int samples_length = ss.rate*duration_ms/1000;
  *samples = (int16_t*)malloc(samples_length*sizeof(int16_t));
  int sweep_count = 0;
  int s = 0;
  while (s < samples_length) {
    // each square wave can be split into 8 time slices of high/low
    uint8_t is_high = 0;
    int samples_per_wave_slice = samples_per_wave / 8;
    int current_wave_slice = (s * 8 / samples_per_wave) % 8;
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
    for (int ts = 0; ts < samples_per_wave_slice && s < samples_length; ts++) {
      (*samples)[s++] = is_high ? 0x9C : 0;
    }

    // handle sweep
    if (sweep_time && (s / sweep_time_samples > sweep_count) && sweep_shift)  {
      sweep_count++;
      int diff = frequency >> (1 << sweep_shift);
      if (is_sweep_decreasing) {
	if (diff > 0) {
	  frequency -= diff;
	}
      } else {
	frequency += diff;
      }
      // TODO: check if the result frequency is greater than 11 bits and stop
      //      printf("info: new freq: %d\n", freq);
      if (frequency > (1 << 10)) {
	printf("warn: should stop here @ freq: %d\n", frequency);
      }
      samples_per_wave = ss.rate / frequency; // redefine samples per wave
    }
  }

  return samples_length;
}

static void wrap_envelope(int16_t *samples,
			  int length,
			  uint8_t value,
			  uint8_t steps,
			  uint8_t is_decreasing) {

  int samples_per_step = ss.rate / 64 * steps;

  for (int s = 0; s < length; s++) {
    if (s && samples_per_step && s % samples_per_step == 0) {
      if (value > 0 && is_decreasing)
	value--;
      if (value < 15 && !is_decreasing)
	value++;
    }
    samples[s] *= value;
  }
}


static struct sound * create_sound_1(struct sound_controller *sc) {
  int64_t id = sound_id(sc->NR10,sc->NR11,sc->NR12,sc->NR13,sc->NR14);
  struct sound * cached;
  if ((cached = get_cached_sound(id))) {
    return cached;
  }

  int freq_X = ((sc->NR14 & 7) << 8) + sc->NR13;
  int freq = (4194304 >> 5) / (2048 - freq_X);

  int16_t *samples = NULL;
  int samples_length = generate_square_wave(&samples,
					    freq,
					    sc->NR11 >> 6,
					    (sc->NR10 >> 4) & 7,
					    sc->NR10 & 3,
					    (sc->NR10 >> 3) & 1,
					    (64-(sc->NR11 & 0x4F)) * 1000 / 256);

  // do a pass over the samples for the envelope
  wrap_envelope(samples,
		samples_length,
		sc->NR12 >> 4,
		sc->NR12 & 7,
		!(sc->NR12 & 8));


  struct sound sound = {
    .id = id,
    .is_continuous = ((sc->NR14 & 0x40) == 0),
    .samples = samples,
    .length = samples_length,
  };

  cache[cache_length++] = sound;
  return &cache[cache_length-1];
}

// TODO: perhaps consolidate with sound1 given the similarities
static struct sound * create_sound_2(struct sound_controller *sc) {
  int64_t id = sound_id(sc->NR21,sc->NR22,sc->NR23,sc->NR24, 0);
  struct sound * cached;
  if ((cached = get_cached_sound(id))) {
    return cached;
  }

  int freq_X = ((sc->NR24 & 7) << 8) + sc->NR23;
  int freq = (4194304 >> 5) / (2048 - freq_X);

  int16_t *samples = NULL;
  int samples_length = generate_square_wave(&samples,
					    freq,
					    sc->NR21 >> 6,
					    0, // sweep off
					    0,
					    0,
					    (64-(sc->NR21 & 0x4F)) * 1000 / 256);

  // do a pass over the samples for the envelope
  wrap_envelope(samples,
		samples_length,
		sc->NR22 >> 4,
		sc->NR22 & 7,
		!(sc->NR22 & 8));


  struct sound sound = {
    .id = id,
    .is_continuous = ((sc->NR24 & 0x40) == 0),
    .samples = samples,
    .length = samples_length,
  };

  cache[cache_length++] = sound;
  return &cache[cache_length-1];
}

void audio_tick(struct sound_controller *sc) {
  if (!(sc->NR52 & 0x80))
    return;

  sc->t_cycles++;
  // TODO: consider bundling more than one sample at a time
  while (sc->t_cycles >= 1500) { // TODO: compute this on init
    // iterate through all the playbacks and write a sample
    int16_t samples[32];
    for (int s = 0; s < 32; s++) {
      samples[s] = 0;
    }

    // TODO: set to proper value
    int8_t volume_stereo_1 = sc->NR50 & 7;
    int8_t volume_stereo_2 = (sc->NR50 >> 4) & 7;

    // TODO gate on audio on
    for (int s = 0; s < 4; s++) {
      struct playback *pb = &sc->playing[s];
      if (!pb->sound || (!pb->sound->is_continuous && !(sc->NR52 & (1<<s))))
	continue;
      for (int i = 0; i < 16; i++) {
	int sample = pb->sound->samples[pb->current_sample++];
	int is_on_stereo_1 = sc->NR51 & (1 << s);
	int is_on_stereo_2 = sc->NR51 & (1 << 4 << s);
	if (is_on_stereo_1) {
	  samples[i*2] += sample*volume_stereo_1;
	}
	if (is_on_stereo_2) {
	  samples[i*2+1] += sample*volume_stereo_2;
	}
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
    int error = 0;
    pa_simple_write(s, samples, 32*sizeof(int16_t), &error);
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
void sc_write_NR14(struct sound_controller* sc, uint8_t byte) {
  sc->NR14 = byte;
  if (sc->NR14 & 0x80) { // initialize bit on
    struct sound * sound = create_sound_1(sc);
    printf("debug: creating sound 1\n");
    sc->playing[0] = (struct playback) {
      .current_sample = 0,
      .sound = sound,
    };
  }
}

void sc_write_NR21(struct sound_controller* sc, uint8_t byte) { sc->NR21 = byte; }
void sc_write_NR22(struct sound_controller* sc, uint8_t byte) { sc->NR22 = byte; }
void sc_write_NR23(struct sound_controller* sc, uint8_t byte) { sc->NR23 = byte; }
void sc_write_NR24(struct sound_controller* sc, uint8_t byte) {
  sc->NR24 = byte;
  if (sc->NR24 & 0x80) { // initialize bit on
    printf("debug: creating sound 2\n");
    struct sound * sound = create_sound_2(sc);
    sc->playing[1] = (struct playback) {
      .current_sample = 0,
      .sound = sound,
    };
  }
}

void sc_write_NR50(struct sound_controller* sc, uint8_t byte) { sc->NR50 = byte; }
void sc_write_NR51(struct sound_controller* sc, uint8_t byte) { sc->NR51 = byte; }
void sc_write_NR52(struct sound_controller* sc, uint8_t byte) { sc->NR52 = byte; }
