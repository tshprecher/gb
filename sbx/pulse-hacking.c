#include <stdio.h>
#include <pulse/simple.h>
#include <pulse/error.h>

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

int main() {
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

  if (error)
    printf("pa_simple_new error code: %d\n", error);

  printf("pa_frame_size: %ld\n", pa_frame_size(&ss));
  printf("pa_bytes_per_second: %ld\n", pa_bytes_per_second(&ss));

  error = write_squarewave(s, 440, 10);
  if (error) {
    printf("error code: %d\n", error);
    //printf("error str: %s\n", pa_strerror(error));
  }

  return 0;
}
