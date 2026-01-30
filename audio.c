#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "audio.h"

#define SAMPLING_RATE 44100

pa_simple *s;
pa_sample_spec ss;

void init_audio() {
  ss.format = PA_SAMPLE_S16NE;
  ss.channels = 2;
  ss.rate = SAMPLING_RATE;

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

static int initialize_on(uint8_t r) {
  return (r & 0x80) > 0;
}

static inline int stereo_vol_left(struct sound_controller *sc) {
  return sc->regs[rNR50] & 7;
}

static inline int stereo_vol_right(struct sound_controller *sc) {
  return (sc->regs[rNR50] >> 4) & 7;
}

static inline int is_on_stereo_left(struct sound_controller *sc,
				    uint8_t sound_type) {
  return sc->regs[rNR51] & (1 << (sound_type-1));
}

static inline int is_on_stereo_right(struct sound_controller *sc,
				     uint8_t sound_type) {
  return (sc->regs[rNR51] & (1 << (sound_type+3)));
}

static inline int is_all_enabled(struct sound_controller *sc) {
  return (sc->regs[rNR52] & 0x80) > 0;
}

static inline int is_all_disabled(struct sound_controller *sc) {
  return !is_all_enabled(sc);
}

static inline int is_sound_type_enabled(struct sound_controller *sc,
					uint8_t sound_type) {
  return (sc->regs[rNR52] & (1 << (sound_type-1))) > 0;
}

static inline int is_completed(struct sound *sound) {
  printf("debug: SOUND is_completed: is_continuous: %d, current sample: %d, duration_samples: %d\n",
	 sound->is_continuous,
	 sound->current_sample,
	 sound->duration_samples);
  return (!sound->is_continuous &&
	  sound->current_sample == sound->duration_samples);
}

static inline int calculate_frequency(uint8_t freq_high, uint8_t freq_low) {
  int freq_X = ((freq_high & 7) << 8) + freq_low;
  return (4194304 >> 5) / (2048 - freq_X);
}

/*
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
      (*samples)[s++] = is_high ? 0xA5 : 0;
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
*/

static int sound1_generate_samples(struct sound *sound, int16_t *buf, int len) {
  printf("debug: SOUND inside sound1_generate_samples()\n");

  // each square wave can be split into 8 time slices of high/low
  float samples_per_wave_slice = (float)sound->samples_per_wave / 8;
  int s = 0;
  while (s < len && !is_completed(sound)) {
    uint8_t is_high = 0;

    // where in the waveslice are we?
    int current_wave_position = sound->current_sample % sound->samples_per_wave;
    switch (sound->waveform_duty_cycle) {
    case 0: // 12.5%
      is_high = current_wave_position >= samples_per_wave_slice &&
	current_wave_position < 2*samples_per_wave_slice ? 1 : 0;
      break;
    case 1: // 25%
      is_high = current_wave_position < 2 * samples_per_wave_slice ? 1 : 0;
      break;
    case 2: // 50%
      is_high = current_wave_position < 4 * samples_per_wave_slice ? 1 : 0;
      break;
    case 3: // 75 %
      is_high = current_wave_position >= 2 * samples_per_wave_slice ? 1 : 0;
      break;
    }
    buf[s++] = is_high ? 0xA5 : 0;
    sound->current_sample++;
    if (sound->is_continuous && sound->current_sample > sound->duration_samples) {
      sound->current_sample = 0;
    }
  }
  return s;
}

int sound_generate_samples(struct sound *sound, int16_t *buf, int len) {
  switch (sound->type) {
  case 1:
    return sound1_generate_samples(sound, buf, len);
    break;
  default:
    return 0;
  }
}

void audio_tick(struct sound_controller *sc) {
  printf("debug: inside audio tick\n");
  if (is_all_disabled(sc))
    return;

  sc->t_cycles++;
  // TODO: consider bundling more than one sample at a time
  while (sc->t_cycles >= 1500) { // TODO: compute this on init
    int16_t accumulated_samples[32];
    for (int s = 0; s < 32; s++) {
      accumulated_samples[s] = 0;
    }

    int8_t volume_left = stereo_vol_left(sc);
    int8_t volume_right = stereo_vol_right(sc);

    int16_t sbuf[16];
    for (int s = 0; s < 1; s++) {
      struct sound *sound = &sc->sounds[s];
      /*if (!is_sound_type_enabled(sc, sound->type)) {
	continue;
	}*/
      if (is_completed(sound)) {
	printf("debug: SOUND completed\n");
	sc->regs[rNR52] &= ~(1<<s); // done, turn sound off
	continue;
      }
      int generated = sound_generate_samples(sound, sbuf, 16);
      printf("debug: SOUND samples generated: %d\n", generated);
      for (int s = 0; s < generated; s++) {
	printf("debug: SOUND samples: %d\n", sbuf[s]);
	if (is_on_stereo_left(sc, sound->type)) {
	  accumulated_samples[s*2] += sbuf[s]*volume_left;
	}
	if (is_on_stereo_right(sc, sound->type)) {
	  accumulated_samples[s*2+1] += sbuf[s]*volume_right;
	}
      }
    }
    int error = 0;
    pa_simple_write(s, accumulated_samples, 32*sizeof(int16_t), &error);
    if (error) {
      printf("debug: error code: %d\n", error);
      printf("debug: error str: %s\n", pa_strerror(error));
    }
    sc->t_cycles -= 1500;
  }
}


void audio_write_reg(struct sound_controller *sc, enum sound_reg reg, uint8_t byte) {
  sc->regs[reg] = byte;
  switch (reg) {
  case rNR14:
    if (initialize_on(sc->regs[rNR14])) {
      int frequency = calculate_frequency(sc->regs[rNR14], sc->regs[rNR13]);
      int sweep_time = ((sc->regs[rNR10] >> 4) & 7);
      int duration_ms = (64-(sc->regs[rNR11] & 0x4F)) * 1000 / 256;

      printf("debug: initializing sound 1: freq -> %d, sweep_time -> %d, duration_ms -> %d\n", frequency, sweep_time, duration_ms);

      // define sound 1 wave
      sc->sounds[0] = (struct sound) {
	.type = 1,
	.current_sample = 0,
	.duration_samples = ss.rate*duration_ms/1000, // TODO: properly set
	.is_continuous = (sc->regs[rNR14] & 0x40) == 0,

	// initial wave parameters
	.waveform_duty_cycle = (sc->regs[rNR11] >> 6),
	.frequency = frequency,
	.samples_per_wave = ss.rate / frequency,

	// sweep parameters
	.sweep_time_samples = (sweep_time ? (ss.rate * sweep_time / frequency) : 0),
	.sweep_shift = (sc->regs[rNR10] & 3),
	.is_sweep_decreasing = ((sc->regs[rNR10] >> 3) & 1),

	// TODO: envelope parameters
      };
    }
    break;
  default:
    break;
  };
}

uint8_t audio_read_reg(struct sound_controller* sc, enum sound_reg reg) {
  return sc->regs[reg];
}
