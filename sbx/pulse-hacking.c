#include <stdio.h>
#include <pulse/simple.h>
#include <pulse/error.h>

pa_simple *s;
pa_sample_spec ss;


int main() {
  ss.format = PA_SAMPLE_U8;
  ss.channels = 1;
  //  ss.rate = 44100;
  ss.rate = 440*10;
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

  uint8_t data[440*10];
  for (int d = 0; d < sizeof(data); d+=10) {
    data[d] = 0;
    data[d+1] = 0;
    data[d+2] = 0;
    data[d+3] = 0;
    data[d+4] = 0;

    data[d+5] = 0x10;
    data[d+6] = 0x10;
    data[d+7] = 0x10;
    data[d+8] = 0x10;
    data[d+9] = 0x10;
  }



  /*  for (int d = 0; d < sizeof(data)/sizeof(int16_t)/2; d++) {
    data[d] = 0;
    data[10000-d] = 1 << 10;
    }*/

  pa_simple_write(s, data, sizeof(data), &error);
  if (error) {
    printf("error code: %d\n", error);
    //printf("error str: %s\n", pa_strerror(error));
  }




  return 0;
}
