#include "sound.h"

#include <pulse/pulseaudio.h>
#include <stdio.h>
#include <stdlib.h>

#include "macros.h"
#include "types.h"

#define SAMPLING_RATE 44100

// TODO: for all sounds make sure they operate properly as parameters
//       change between initializations.

pa_mainloop* mainloop = NULL;
pa_stream* stream = NULL;
pa_sample_spec ss;

// triggered when the connection context state updates
void context_state_callback(pa_context* context, void* userdata) {
  switch (pa_context_get_state(context)) {
    case PA_CONTEXT_READY: {
      // data format
      ss.format = PA_SAMPLE_S16NE;
      ss.channels = 2;
      ss.rate = SAMPLING_RATE;

      // init stream
      stream = pa_stream_new(context, "gbe sound", &ss, NULL);

      // connect the stream onto the default output
      pa_stream_connect_playback(stream, NULL, NULL, PA_STREAM_NOFLAGS, NULL,
                                 NULL);
      break;
    }
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
      printf("error: pulseaudio context connection disconnected or failed.\n");
      exit(1);
    default:
      break;
  }
}

void global_init_sound() {
  mainloop = pa_mainloop_new();
  pa_mainloop_api* mainloop_api = pa_mainloop_get_api(mainloop);

  // create the base client communications context
  pa_context* context = pa_context_new(mainloop_api, "Async API Demo Client");

  // register the event handler to track the lifecycle of our context connection
  pa_context_set_state_callback(context, context_state_callback, mainloop_api);

  // connect the context
  if (pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
    fprintf(stderr, "sound: failed to execute pulseaudio context connect.\n");
    exit(1);
  }

  while (!stream || pa_stream_get_state(stream) != PA_STREAM_READY)
    pa_mainloop_iterate(mainloop, 0, NULL);
}

void init_sound(struct sound_controller* sc) { sc->regs[rNR52] = 0x80; }

static inline int initialize_on(u8 r) { return b_is_on(r, 7); }

static inline int stereo_vol_left(struct sound_controller* sc) {
  // TODO(nit perf): compute this on write with a trigger mechanism?
  return sc->regs[rNR50] & 7;
}

static inline int stereo_vol_right(struct sound_controller* sc) {
  return (sc->regs[rNR50] >> 4) & 7;
}

static inline int is_on_stereo_left(struct sound_controller* sc,
                                    u8 sound_type) {
  return b_is_on(sc->regs[rNR51], (sound_type - 1));
}

static inline int is_on_stereo_right(struct sound_controller* sc,
                                     u8 sound_type) {
  return b_is_on(sc->regs[rNR51], (sound_type + 3));
}

static inline int is_all_disabled(struct sound_controller* sc) {
  return b_is_off(sc->regs[rNR52], 7);
}

static inline void disable_sound(struct sound_controller* sc, u8 sound_type) {
  b_set_off(sc->regs[rNR52], (sound_type - 1));
}

static inline int is_sound_enabled(struct sound_controller* sc,
                                   struct sound* sound) {
  return b_is_on(sc->regs[rNR52], (sound->type - 1));
}

static inline int is_completed(struct sound* sound) {
  return !sound->is_continuous &&
         sound->current_sample == sound->duration_samples;
}

static inline int freq_to_hz(u8 freq_high, u8 freq_low) {
  int freq = ((freq_high & 7) << 8) + freq_low;
  return (CLOCK_FREQ >> 5) / (2048 - freq);
}

static int generate_square_wave_samples(struct sound_controller* sc,
                                        struct sound* sound, s16* buf,
                                        int len) {
  // each square wave can be split into 8 time slices of high/low
  float samples_per_wave_slice = (float)sound->samples_per_wave / 8;
  int s = 0;
  while (s < len && !is_completed(sound)) {
    u8 is_high = 0;

    // where in the waveslice are we?
    int current_wave_position = sound->current_sample % sound->samples_per_wave;
    switch (sound->waveform_duty_cycle) {
      case 0:  // 12.5%
        is_high = current_wave_position >= samples_per_wave_slice &&
                          current_wave_position < 2 * samples_per_wave_slice
                      ? 1
                      : 0;
        break;
      case 1:  // 25%
        is_high = current_wave_position < 2 * samples_per_wave_slice ? 1 : 0;
        break;
      case 2:  // 50%
        is_high = current_wave_position < 4 * samples_per_wave_slice ? 1 : 0;
        break;
      case 3:  // 75 %
        is_high = current_wave_position >= 2 * samples_per_wave_slice ? 1 : 0;
        break;
    }

    // handle sweep
    if (sound->sweep_time_samples && sound->current_sample &&
        sound->current_sample % sound->sweep_time_samples == 0) {
      int sweep_time = ((sc->regs[rNR10] >> 4) & 7);
      sound->sweep_time_samples =
          (sweep_time ? (ss.rate * sweep_time / 128) : 0);

      int sweep_freq = ((sc->regs[rNR14] & 7) << 8) + sc->regs[rNR13];
      int freq_step = sweep_freq >> (sc->regs[rNR10] & 7);

      if (sound->is_sweep_increasing) {
        sweep_freq += freq_step;
      } else {
        sweep_freq -= freq_step;
      }
      if (sweep_freq < 0 || sweep_freq >= 2048) {
        disable_sound(sc, sound->type);
        return 0;
      }

      sc->regs[rNR14] &= ~7;
      sc->regs[rNR14] |= (sweep_freq >> 8) & 7;
      sc->regs[rNR13] = sweep_freq & 0xFF;
      sound->samples_per_wave =
          ss.rate / freq_to_hz(sc->regs[rNR14], sc->regs[rNR13]);

      sweep_freq = ((sc->regs[rNR14] & 7) << 8) + sc->regs[rNR13];
    }

    // handle envelope
    if (sound->current_sample && sound->samples_per_env_step &&
        sound->current_sample % sound->samples_per_env_step == 0) {
      if (sound->env_value > 0 && sound->is_env_decreasing) sound->env_value--;
      if (sound->env_value < 15 && !sound->is_env_decreasing)
        sound->env_value++;
    }

    buf[s] = is_high ? (0xF << 2) : 0;
    buf[s] *= sound->env_value;
    s++;
    sound->current_sample++;
    if (sound->is_continuous &&
        sound->current_sample >= sound->duration_samples) {
      sound->current_sample = 0;
    }
  }
  return s;
}

static int generate_defined_wave_samples(struct sound* sound, s16* buf,
                                         int len) {
  float samples_per_step = (float)sound->samples_per_wave / 32;
  int s = 0;
  while (s < len && !is_completed(sound)) {
    int current_step = (int)(sound->current_sample / samples_per_step) % 32;
    if (sound->output_level) {
      u8 val = sound->waveform[current_step / 2];
      // TODO(perf): write in nibbles instead of this shifting?
      if (current_step % 2 == 0) val >>= 4;
      val &= 0xF;
      val >>= (sound->output_level - 1);
      buf[s] = val;
    } else {
      buf[s] = 0;
    }

    buf[s] <<= 6;
    s++;
    sound->current_sample++;
    if (sound->is_continuous &&
        sound->current_sample >= sound->duration_samples) {
      sound->current_sample = 0;
    }
  }
  return s;
}

static int generate_white_noise_wave_samples(struct sound* sound, s16* buf,
                                             int len) {
  int s = 0;
  while (s < len && !is_completed(sound)) {
    int is_high = b_is_on(sound->lfsr_shift_register, 0);

    // handle envelope
    if (sound->current_sample && sound->samples_per_env_step &&
        sound->current_sample % sound->samples_per_env_step == 0) {
      if (sound->env_value > 0 && sound->is_env_decreasing) sound->env_value--;
      if (sound->env_value < 15 && !sound->is_env_decreasing)
        sound->env_value++;
    }

    buf[s] = is_high ? (0xF << 2) : 0;
    buf[s] *= sound->env_value;

    // TODO: handle case where sound->frequency < ss.rate?
    int noise_cycle_before = sound->current_sample * sound->freq_hz / ss.rate;
    sound->current_sample++;
    int noise_cycle_after = sound->current_sample * sound->freq_hz / ss.rate;

    while (noise_cycle_before < noise_cycle_after) {
      int bit_0 = b_is_on(sound->lfsr_shift_register, 0);
      int bit_1 = b_is_on(sound->lfsr_shift_register, 1);
      sound->lfsr_shift_register >>= 1;

      int xor_result = bit_0 ^ bit_1;
      if (sound->is_long_mode) {
        sound->lfsr_shift_register |= (xor_result << 14);
      } else {
        sound->lfsr_shift_register |= (xor_result << 6);
      }
      noise_cycle_before++;
    }

    s++;
  }
  return s;
}

static int sound_generate_samples(struct sound_controller* sc,
                                  struct sound* sound, s16* buf, int len) {
  switch (sound->type) {
    case 1:
    case 2:
      return generate_square_wave_samples(sc, sound, buf, len);
    case 3:
      return generate_defined_wave_samples(sound, buf, len);
    case 4:
      return generate_white_noise_wave_samples(sound, buf, len);
    default:
      return 0;
  }
}

void sound_tick(struct sound_controller* sc) {
  if (is_all_disabled(sc)) return;

  sc->t_cycles++;
  while (sc->t_cycles >= 380) {        // TODO: compute this on init
    s16 accumulated_samples[8] = {0};  // TODO: rename stereo samples?
    s16 sbuf[4];
    for (int s = 0; s < 4; s++) {
      struct sound* sound = &sc->sounds[s];
      if (sound->type < 1 || sound->type > 4) {
        continue;
      }
      if (!is_sound_enabled(sc, sound)) {
        continue;
      }
      if (is_completed(sound)) {
        disable_sound(sc, sound->type);
        continue;
      }
      // TODO: do we need to check for sound completion in functions called from
      // here?
      int generated = sound_generate_samples(sc, sound, sbuf, 4);

      for (int s = 0; s < generated; s++) {
        if (is_on_stereo_left(sc, sound->type)) {
          accumulated_samples[s * 2] += sbuf[s] * stereo_vol_left(sc);
        }
        if (is_on_stereo_right(sc, sound->type)) {
          accumulated_samples[s * 2 + 1] += sbuf[s] * stereo_vol_right(sc);
        }
      }
    }

    int error = pa_stream_write(stream, accumulated_samples, 8 * sizeof(s16),
                                NULL, 0, PA_SEEK_RELATIVE);
    if (error) {
      printf("error: writing samples, error code: %d\n", error);
      printf("error: writing samples, error str: %s\n", pa_strerror(error));
    }
    pa_mainloop_iterate(mainloop, 0, NULL);
    sc->t_cycles -= 380;
  }
}

void sound_write_reg(struct sound_controller* sc, enum sound_reg reg,
                     u8 value) {
  sc->regs[reg] = value;

  switch (reg) {
    case rNR10: {
      int sweep_time = ((sc->regs[rNR10] >> 4) & 7);
      sc->sounds[0].sweep_time = sweep_time;
      sc->sounds[0].sweep_time_samples =
          (sweep_time ? (ss.rate * sweep_time) >> 7 : 0);
      sc->sounds[0].sweep_shift = (sc->regs[rNR10] & 7);
      sc->sounds[0].is_sweep_increasing = b_is_on(sc->regs[rNR10], 3);
    } break;
    case rNR11:
      sc->sounds[0].waveform_duty_cycle = (sc->regs[rNR11] >> 6);
      break;
    case rNR12:
      if (!(sc->regs[rNR12] & 0xF8)) disable_sound(sc, 1);

      sc->sounds[0].samples_per_env_step = ss.rate * (sc->regs[rNR12] & 7) / 64;
      sc->sounds[0].env_value = sc->regs[rNR12] >> 4;
      sc->sounds[0].is_env_decreasing = b_is_off(sc->regs[rNR12], 3);
      break;
    case rNR13:
      sc->sounds[0].sweep_freq = ((sc->regs[rNR14] & 7) << 8) + sc->regs[rNR13];
      sc->sounds[0].samples_per_wave =
          ss.rate / freq_to_hz(sc->regs[rNR14], sc->regs[rNR13]);
      break;
    case rNR14:
      sc->sounds[0].sweep_freq = ((sc->regs[rNR14] & 7) << 8) + sc->regs[rNR13];
      sc->sounds[0].samples_per_wave =
          ss.rate / freq_to_hz(sc->regs[rNR14], sc->regs[rNR13]);
      if (initialize_on(sc->regs[rNR14])) {
        int duration_ms = (64 - (sc->regs[rNR11] & 0x3F)) * 1000 / 256;
        /* printf("debug (sound): initializing sound 1: duration_ms -> %d\n", */
        /*        duration_ms); */

        sc->sounds[0].type = 1;
        sc->sounds[0].current_sample = 0;  // restart sound on initialize
        sc->sounds[0].duration_samples = ss.rate * duration_ms / 1000;
        sc->sounds[0].is_continuous = b_is_off(sc->regs[rNR14], 6);

        b_set_on(sc->regs[rNR52], 0);
      }
      break;
    case rNR21:
      sc->sounds[1].waveform_duty_cycle = (sc->regs[rNR21] >> 6);
      break;
    case rNR22:
      sc->sounds[1].samples_per_env_step = ss.rate * (sc->regs[rNR22] & 7) / 64;
      sc->sounds[1].env_value = sc->regs[rNR22] >> 4;
      sc->sounds[1].is_env_decreasing = b_is_off(sc->regs[rNR22], 3);

      if (!(sc->regs[rNR22] & 0xF8)) disable_sound(sc, 2);
      break;
    case rNR23:
      sc->sounds[1].samples_per_wave =
          ss.rate / freq_to_hz(sc->regs[rNR24], sc->regs[rNR23]);
      break;
    case rNR24:
      sc->sounds[1].samples_per_wave =
          ss.rate / freq_to_hz(sc->regs[rNR24], sc->regs[rNR23]);
      if (initialize_on(sc->regs[rNR24])) {
        int duration_ms = (64 - (sc->regs[rNR21] & 0x3F)) * 1000 / 256;
        sc->sounds[1].type = 2;
        sc->sounds[1].current_sample = 0;  // restart sound on initialize
        sc->sounds[1].duration_samples = ss.rate * duration_ms / 1000;
        sc->sounds[1].is_continuous = b_is_off(sc->regs[rNR24], 6);

        // sweep parameters off
        // TODO: can we remove and rely on the struct never setting to nonzero?
        sc->sounds[1].sweep_time_samples = 0;
        sc->sounds[1].sweep_shift = 0;
        sc->sounds[1].is_sweep_increasing = 0;

        b_set_on(sc->regs[rNR52], 1);
      }
      break;
    case rNR30:
      if (b_is_on(sc->regs[rNR30], 7)) {
        b_set_on(sc->regs[rNR52], 2);
        sc->sounds[2].current_sample = 0;
      } else {
        b_set_off(sc->regs[rNR52], 2);
      }
      break;
    case rNR32:
      sc->sounds[2].output_level = (sc->regs[rNR32] >> 5) & 0x3;
      break;
    case rNR33:
      sc->sounds[2].samples_per_wave =
          ss.rate / freq_to_hz(sc->regs[rNR34], sc->regs[rNR33]);
      break;
    case rNR34:
      sc->sounds[2].samples_per_wave =
          ss.rate / freq_to_hz(sc->regs[rNR34], sc->regs[rNR33]);
      if (initialize_on(sc->regs[rNR34])) {
        int duration_ms = (256 - (sc->regs[rNR31])) * 1000 / 256;
        /* printf("debug (sound): initializing sound 3: duration_ms ->%d\n", */
        /*        duration_ms); */

        sc->sounds[2].type = 3;
        sc->sounds[2].current_sample = 0;  // restart sound on initialize
        sc->sounds[2].duration_samples = ss.rate * duration_ms / 1000;
        sc->sounds[2].samples_per_wave =
            ss.rate / freq_to_hz(sc->regs[rNR34], sc->regs[rNR33]);
        sc->sounds[2].is_continuous = b_is_off(sc->regs[rNR34], 6);
        sc->sounds[2].output_level = (sc->regs[rNR32] >> 5) & 0x3;

        sc->sounds[2].waveform = sc->wram;

        b_set_on(sc->regs[rNR52], 2);
      }
      break;
    case rNR43: {
      int freq_hz = (CLOCK_FREQ >> 3);
      int div_factor = sc->regs[rNR43] & 7;
      if (div_factor) {
        freq_hz /= div_factor;
      } else {
        freq_hz *= 2;
      }

      int shift_factor = (sc->regs[rNR43] >> 4) & 15;
      if (shift_factor > 14) {
        printf("warn: invalid sound 4 shift factor: %d\n", shift_factor);
      } else {
        freq_hz >>= (shift_factor + 1);
      }

      int is_long_mode = b_is_off(sc->regs[rNR43], 3);

      sc->sounds[3].freq_hz = freq_hz;
      sc->sounds[3].lfsr_shift_register = is_long_mode ? 0x7FFF : 0x7F;
      sc->sounds[3].is_long_mode = is_long_mode;
    } break;
    case rNR44:
      if (initialize_on(sc->regs[rNR44])) {
        int duration_ms = (64 - (sc->regs[rNR41] & 0x3F)) * 1000 / 256;
        /* printf("debug (sound): initializing sound 4: duration_ms -> %d\n", */
        /*  duration_ms); */

        sc->sounds[3].type = 4;
        sc->sounds[3].current_sample = 0;
        sc->sounds[3].duration_samples = ss.rate * duration_ms / 1000;
        sc->sounds[3].is_continuous = b_is_off(sc->regs[rNR44], 6);

        sc->sounds[3].samples_per_env_step =
            ss.rate * (sc->regs[rNR42] & 7) / 64;
        sc->sounds[3].env_value = sc->regs[rNR42] >> 4;
        sc->sounds[3].is_env_decreasing = b_is_off(sc->regs[rNR42], 3);

        b_set_on(sc->regs[rNR52], 3);
      }
      break;
    default:
      break;
  };
}

inline u8 sound_read_wram(struct sound_controller* sc, u16 addr) {
  return sc->wram[addr - 0xFF30];
}

inline void sound_write_wram(struct sound_controller* sc, u16 addr, u8 value) {
  sc->wram[addr - 0xFF30] = value;
}

inline u8 sound_read_reg(struct sound_controller* sc, enum sound_reg reg) {
  return sc->regs[reg];
}
