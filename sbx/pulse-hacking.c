#include <stdio.h>
#include <pulse/simple.h>
#include <pulse/error.h>

pa_simple *s;
pa_sample_spec ss;


int write_squarewave(pa_simple *s, int freq, int seconds) {
  uint8_t data[ss.rate*seconds];
  int error = 0;

  // define the period of
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

struct sound1_generator {
  int freq,
    samples_sound_length,
    samples_per_wave,
    samples_per_sweep_time;

  uint8_t wave_duty_cycle,
    sweep_time_ts,
    sweep_counter,
    sweep_shift_num,
    is_sweep_decrease;
};

static struct sound1_generator create_sound1_generator(uint8_t reg_NR10,
						       uint8_t reg_NR11,
						       uint8_t reg_NR12,
						       uint8_t reg_NR13,
						       uint8_t reg_NR14) {
  struct sound1_generator gen = {0};

  gen.sweep_shift_num =  reg_NR10 & 3;
  gen.is_sweep_decrease = (reg_NR10 >> 3) & 1;
  gen.sweep_time_ts = (reg_NR10 >> 4) & 7;
  // TODO: handle continuous sound
  gen.samples_sound_length = (ss.rate * (64-(reg_NR11 & 0x4F))) >> 8;

  int freq_X = ((reg_NR14 & 7) << 8) + reg_NR13;
  gen.freq = (4194304 >> 5) / (2048 - freq_X);
  gen.samples_per_sweep_time = gen.sweep_time_ts ? (ss.rate / gen.freq) * gen.sweep_time_ts : 0;
  gen.samples_per_wave = ss.rate / gen.freq;
  gen.wave_duty_cycle = reg_NR11 >> 6;

  return gen;
}


void generate_samples(struct sound1_generator* gen, int16_t *samples_buf, int buflen) {
  int s = 0;
  while (s < buflen) {
    // each square wave can be split into 8 time slices of high/low
    uint8_t is_high = 0;
    int samples_per_wave_slice = gen->samples_per_wave / 8;
    int current_wave_slice = (s * 8 / gen->samples_per_wave) % 8;
    switch (gen->wave_duty_cycle) {
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
    for (int ts = 0; ts < samples_per_wave_slice && s < gen->samples_sound_length; ts++) {
      samples_buf[s++] = is_high ? 0x4000 : 0;
    }
    if (gen->sweep_time_ts && (s / gen->samples_per_sweep_time > gen->sweep_counter) && gen->sweep_shift_num)  {
      gen->sweep_counter++;
      int diff = gen->freq >> (1 << gen->sweep_shift_num);
      if (gen->is_sweep_decrease) {
	if (diff > 0) {
	  gen->freq -= diff;
	}
      } else {
	gen->freq += diff;
      }
      // TODO: check if the result frequency is greater than 11 bits and stop
      printf("info: new freq: %d\n", gen->freq);
      if (gen->freq > (1 << 10)) {
	printf("warn: should stop here @ freq: %d\n", gen->freq);
      }
      gen->samples_per_wave = ss.rate / gen->freq; // redefine samples per wave
    }
  }
}


int test_sound_1(pa_simple *s,
		  uint8_t reg_NR10,
		  uint8_t reg_NR11,
		  uint8_t reg_NR12, // TODO: implement
		  uint8_t reg_NR13,
		  uint8_t reg_NR14) {

  struct sound1_generator gen = create_sound1_generator(reg_NR10, reg_NR11, reg_NR12, reg_NR13, reg_NR14);

  printf("info: sweep_time_ts: %d\n", gen.sweep_time_ts);
  printf("info: ss.rate: %d, freq: %d, sweep_time_samples: %d, sound_length_samples: %d\n",
	 ss.rate, gen.freq, gen.samples_per_sweep_time, gen.samples_sound_length);

  int16_t data[gen.samples_sound_length];

  generate_samples(&gen, data, gen.samples_sound_length);

  for (int d = 0; d < gen.samples_sound_length; d++) {
    printf("debug: gnu plot %d\t%d\n", d, data[d]);
  }

  int error = 0;
  pa_simple_write(s, data, sizeof(data), &error);
  return error;
}


int main() {
  ss.format = PA_SAMPLE_S16NE;
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

  if (error)
    printf("pa_simple_new error code: %d\n", error);

  printf("pa_frame_size: %ld\n", pa_frame_size(&ss));
  printf("pa_bytes_per_second: %ld\n", pa_bytes_per_second(&ss));

  error = test_sound_1(s, 0 , 0, 8, 0, 128);
  //error = write_sound_1(s, 0x79 , 0x00, 0x08, 0x00, 0x04);

  if (error) {
    printf("error code: %d\n", error);
    //printf("error str: %s\n", pa_strerror(error));
  }

  return 0;
}
