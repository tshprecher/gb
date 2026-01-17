#include <stdio.h>
#include <stdlib.h>
#include <pulse/simple.h>
#include <pulse/error.h>

pa_simple *s;
pa_sample_spec ss;

struct sound {
  uint64_t uuid;
  uint8_t type;
  uint8_t is_continuous;
  uint16_t *samples;
  int samples_len;
};

struct sound sound_cache[256];
int sound_cache_len = 0;


static uint64_t create_sound_uuid(
				  uint8_t r1,
				  uint8_t r2,
				  uint8_t r3,
				  uint8_t r4,
				  uint8_t r5) {
  uint64_t uuid = r1;
  uuid <<= 32;
  uuid += (r2 << 24) + (r3 << 16) + (r4 << 8) + r5;
  return uuid;
}


struct sound create_sound_1(pa_simple *s,
		  uint8_t reg_NR10,
		  uint8_t reg_NR11,
		  uint8_t reg_NR12, // TODO: implement
		  uint8_t reg_NR13,
		  uint8_t reg_NR14) {

  uint8_t sweep_shift_num = reg_NR10 & 3;
  uint8_t is_sweep_decrease = (reg_NR10 >> 3) & 1;
  uint8_t sweep_time_ts = (reg_NR10 >> 4) & 7;

  printf("info: sweep_time_ts: %d\n", sweep_time_ts);

  int freq_X = ((reg_NR14 & 7) << 8) + reg_NR13;
  int freq = (4194304 >> 5) / (2048 - freq_X);

  // TODO: handle continuous sound
  int sound_length_samples = (ss.rate * (64-(reg_NR11 & 0x4F))) >> 8;
  //  int sound_length_samples = 44100*2; // two seconds
  int sweep_time_samples = sweep_time_ts ? (ss.rate / freq) * sweep_time_ts : 0;


  printf("info: ss.rate: %d, freq_X: %d, freq: %d, sweep_time_samples: %d, sound_length_samples: %d\n",
	 ss.rate, freq_X, freq, sweep_time_samples, sound_length_samples);

  uint16_t *data = malloc(sound_length_samples*sizeof(uint16_t));
  int d = 0;
  int samples_per_wave = ss.rate / freq;
  int waveform_duty_cycle = reg_NR11 >> 6;
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
    .uuid = create_sound_uuid(reg_NR10,
			      reg_NR11,
			      reg_NR12,
			      reg_NR13,
			      reg_NR14),
    .type = 1,
    .is_continuous = ((reg_NR14 & 0x40) == 0),
    .samples = data,
    .samples_len = sound_length_samples,
  };

  return r;
}


int main() {
  ss.format = PA_SAMPLE_S16NE;
  ss.channels = 1;
  ss.rate = 44100;
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

  if (error)
    printf("pa_simple_new error code: %d\n", error);

  printf("pa_frame_size: %ld\n", pa_frame_size(&ss));
  printf("pa_bytes_per_second: %ld\n", pa_bytes_per_second(&ss));

  //  struct sound sound = create_sound_1(s, 0 , 0, 8, 0, 128);
  struct sound sound = create_sound_1(s, 0x79 , 0x00, 0x08, 0x00, 0x04);

  if (error) {
    printf("error code: %d\n", error);
    //printf("error str: %s\n", pa_strerror(error));
  }

  for (int s = s; s < sound.samples_len; s++) {
    printf("%d\t%d\n", s, sound.samples[s]);
  }

  pa_simple_write(s, sound.samples, sound.samples_len*sizeof(uint16_t), NULL);

  return 0;
}
