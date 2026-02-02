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

static inline int initialize_on(uint8_t r) {
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
  return (sc->regs[rNR52] & 0x80) == 0;
}

static inline int is_sound_type_enabled(struct sound_controller *sc,
					uint8_t sound_type) {
  //  printf("debug: SOUND is sound_type_enabled NR52 0x%02X\n", sc->regs[rNR52]);
  return (sc->regs[rNR52] & (1 << (sound_type-1))) > 0;
}

static inline int is_sound_enabled(struct sound_controller *sc, struct sound *sound) {
  return sound->is_continuous || is_sound_type_enabled(sc, sound->type);
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

static int generate_square_wave_samples(struct sound *sound, int16_t *buf, int len) {
  printf("debug: SOUND inside generate_square_wave_samples()\n");

  // each square wave can be split into 8 time slices of high/low
  float samples_per_wave_slice = (float)sound->samples_per_wave / 8;
  int s = 0;
  int current_sweep = sound->sweep_time_samples ?
    (sound->current_sample / sound->sweep_time_samples) : 0;
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

    // handle sweep
    if (sound->sweep_shift && sound->sweep_time_samples &&
	((sound->current_sample / sound->sweep_time_samples) > current_sweep))  {
      current_sweep++;
      int freq_step = sound->frequency >> (1 << sound->sweep_shift);
      if (sound->is_sweep_decreasing) {
	sound->frequency -= freq_step;
      } else {
	sound->frequency += freq_step;
      }
      printf("debug: new freq -> %d\n", sound->frequency);
      // TODO: check if the result frequency is greater than 11 bits and stop
      //      printf("info: new freq: %d\n", freq);
      if (sound->frequency > (1 << 10)) {
	printf("warn: should stop here @ freq: %d\n", sound->frequency);
      }
      // redefine samples per wave from new frequency
      sound->samples_per_wave = ss.rate / sound->frequency;
    }

    // handle envelope
    if (sound->current_sample && sound->samples_per_env_step &&
	sound->current_sample % sound->samples_per_env_step == 0) {
      if (sound->env_value > 0 && sound->is_env_decreasing)
	sound->env_value--;
      if (sound->env_value < 15 && !sound->is_env_decreasing)
	sound->env_value++;
    }

    buf[s] = is_high ? 0xA5 : 0;
    buf[s] *= sound->env_value;
    s++;
    sound->current_sample++;
    if (sound->is_continuous && sound->current_sample >= sound->duration_samples) {
      sound->current_sample = 0;
    }
  }
  return s;
}

static int generate_defined_wave_samples(struct sound *sound, int16_t *buf, int len) {
  printf("debug: SOUND inside generate_defined_wave_samples()\n");
  float samples_per_step = (float)sound->samples_per_wave / 32;
  printf("debug: samples_per_step -> %f\n", samples_per_step);
  int s = 0;
  while (s < len && !is_completed(sound)) {
    int current_step = (int)(sound->current_sample / samples_per_step) % 32;
    if (sound->output_level) {
      buf[s] = sound->waveform[current_step] * (0x2C >> (sound->output_level-1));
    } else {
      buf[s] = 0;
    }

    s++;
    sound->current_sample++;
    if (sound->is_continuous && sound->current_sample >= sound->duration_samples) {
      sound->current_sample = 0;
    }
  }
  return s;
}

int sound_generate_samples(struct sound *sound, int16_t *buf, int len) {
  switch (sound->type) {
  case 1:
  case 2:
    return generate_square_wave_samples(sound, buf, len);
  case 3:
    return generate_defined_wave_samples(sound, buf, len);
  default:
    return 0;
  }
}

void audio_tick(struct sound_controller *sc) {
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
    for (int s = 0; s < 3; s++) {
      //if (s != 2)
      //      	continue;
      struct sound *sound = &sc->sounds[s];
      if (sound->type < 1 || sound->type > 4) { // type not valid: sound not yet set
	continue;
      }
      if (!is_sound_enabled(sc, sound)) {
	continue;
      }
      if (is_completed(sound)) {
	printf("debug: SOUND completed\n");
	sc->regs[rNR52] &= ~(1<<s); // done, turn sound off
	continue;
      }
      printf("debug: generating SOUND %d samples\n", sound->type);
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
	.current_sample = 0, // restart sound on initialize
	.duration_samples = ss.rate*duration_ms/1000,
	.is_continuous = (sc->regs[rNR14] & 0x40) == 0,

	// initial wave parameters
	.waveform_duty_cycle = (sc->regs[rNR11] >> 6),
	.frequency = frequency,
	.samples_per_wave = ss.rate / frequency,

	// sweep parameters
	.sweep_time_samples = (sweep_time ? (ss.rate * sweep_time / frequency) : 0),
	.sweep_shift = (sc->regs[rNR10] & 3),
	.is_sweep_decreasing = ((sc->regs[rNR10] >> 3) & 1),

	// envelope parameters
	.samples_per_env_step = ss.rate * (sc->regs[rNR12] & 7) / 64,
	.env_value = sc->regs[rNR12] >> 4,
	.is_env_decreasing = !(sc->regs[rNR12] & 8),

      };
      printf("debug: SOUND 1 initialized NR14 -> 0x%02X, NR52 -> 0x%02X, is_continuous -> %d\n", sc->regs[rNR14], sc->regs[rNR52], sc->sounds[0].is_continuous);
      if (!sc->sounds[0].is_continuous) {
	sc->regs[rNR52] |= 1;
      }

    }
    break;
  case rNR24:
    if (initialize_on(sc->regs[rNR24])) {
      int frequency = calculate_frequency(sc->regs[rNR24], sc->regs[rNR23]);
      int duration_ms = (64-(sc->regs[rNR21] & 0x4F)) * 1000 / 256;

      printf("debug: initializing sound 2: freq -> %d, duration_ms -> %d\n", frequency, duration_ms);

      // define sound 2 wave (sound 1 but sweep off)
      sc->sounds[1] = (struct sound) {
	.type = 2,
	.current_sample = 0, // restart sound on initialize
	.duration_samples = ss.rate*duration_ms/1000,
	.is_continuous = (sc->regs[rNR24] & 0x40) == 0,

	// initial wave parameters
	.waveform_duty_cycle = (sc->regs[rNR21] >> 6),
	.frequency = frequency,
	.samples_per_wave = ss.rate / frequency,

	// sweep parameters off
	.sweep_time_samples = 0,
	.sweep_shift = 0,
	.is_sweep_decreasing = 0,

	// envelope parameters
	.samples_per_env_step = ss.rate * (sc->regs[rNR22] & 7) / 64,
	.env_value = sc->regs[rNR22] >> 4,
	.is_env_decreasing = !(sc->regs[rNR22] & 8),
      };

      printf("debug: SOUND 2 initialized NR24 -> 0x%02X, NR52 -> 0x%02X, is_continuous -> %d\n", sc->regs[rNR24], sc->regs[rNR52], sc->sounds[1].is_continuous);
      if (!sc->sounds[1].is_continuous) {
	sc->regs[rNR52] |= 2;
      }
    }
    break;
  case rNR30:
    printf("debug: writing SOUND 3 rNR30 -> 0x%02X \n", byte);
    if (sc->regs[rNR30] & 0x80) {
      // TODO: restart sound
    } else {
      // TODO: stop sound
    }
    break;
  case rNR31:
    printf("debug: writing SOUND 3 rNR31 -> 0x%02X\n", byte);
    break;
  case rNR32:
    printf("debug: writing SOUND 3 rNR32 -> 0x%02X\n", byte);
    break;
  case rNR33:
    printf("debug: writing SOUND 3 rNR33 -> 0x%02X\n", byte);
    break;
  case rNR34:
    if (initialize_on(sc->regs[rNR34])) {
      int frequency = calculate_frequency(sc->regs[rNR34], sc->regs[rNR33]);
      int duration_ms = (256-(sc->regs[rNR31])) * 1000 / 256;

      printf("debug: initializing SOUND 3: freq -> %d, duration_ms -> %d\n", frequency, duration_ms);

      // define sound 3 wave
      // TODO: output level
      struct sound sound3 = {
	.type = 3,
	.current_sample = 0, // restart sound on initialize
	.frequency = frequency,
	.duration_samples = ss.rate*duration_ms/1000,
	.samples_per_wave = ss.rate / frequency,
	.is_continuous = (sc->regs[rNR34] & 0x40) == 0,
	.output_level = (sc->regs[rNR32] >> 5) & 0x3,
      };

      uint8_t *waveform = &sc->mc->ram[0xFF30];
      for (int step = 0; step < 32; step+=2) {
	uint8_t byte = waveform[step/2];
	sound3.waveform[step] = (byte >> 4) & 0x0F;
	sound3.waveform[step+1] = byte & 0x0F;
      }

      sc->sounds[2] = sound3;

      printf("debug: SOUND 3 initialized NR34 -> 0x%02X, NR52 -> 0x%02X, is_continuous -> %d\n", sc->regs[rNR34], sc->regs[rNR52], sc->sounds[1].is_continuous);

      if (!sc->sounds[2].is_continuous) {
	sc->regs[rNR52] |= 4;
      }
    }
    break;
  case rNR44:
    printf("debug: TODO implement SOUND 4: NR44 -> 0x%02X\n", sc->regs[rNR44]);
    break;
  default:
    break;
  };
}

uint8_t audio_read_reg(struct sound_controller* sc, enum sound_reg reg) {
  return sc->regs[reg];
}
