#include "cpu.h"
#include "inst.h"


char upper(uint16_t value) { return (value >> 8) & 0xFF; }
char lower(uint16_t value) { return value & 0xFF; }


// cpu_exec takes a cpu and executs num_cycles number of cycles.
// It returns the number of cycles run.
int cpu_exec(struct cpu *cpu, int num_cycles)
{
  if (num_cycles < 0)
    return 0;
  return 0;
}
