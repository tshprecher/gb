#include "cpu.h"
#include "inst.h"

struct cpu cpu_copy(struct cpu *c)
{
    struct cpu ret = *c;
    return ret;
}

char upper(uint16_t value)
{
    return (value >> 8) & 0xFF;
}

char lower(uint16_t value)
{
    return value & 0xFF;
}

// cpu_exec takes a cpu and executs inst_count number of instructions.
// If inst_count == -1, it runs forever. It returns 1 on success, 0 on failure.
int cpu_exec(struct cpu *cpu, int inst_count)
{
    int c = 0;
    while (c < inst_count || inst_count == -1)
    {
        struct inst inst = {0};
        inst_decode((unsigned char *)(&cpu->ram[cpu->PC]), &inst);
        switch (inst.type)
        {
        case CALL:
            if (inst.cond)
            {
                return 0;
            }
            else
            {
                cpu->ram[cpu->SP - 1] = upper(cpu->PC);
                cpu->ram[cpu->SP - 2] = lower(cpu->PC);
                cpu->PC = inst.lit_16bit;
                cpu->SP -= 2;
            }
            break;
        default:
            return 0;
        }
        c++;
    }
    return 1;
}
