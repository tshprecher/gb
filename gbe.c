#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "inst.h"
#include "cpu.h"

#define TERM_COLOR_GREEN "\x1B[32m"
#define TERM_COLOR_CYAN "\x1B[36m"
#define TERM_COLOR_RESET "\033[0m"

struct gb
{
    struct cpu *cpu;
    char ram[0x10000];

    // for debugging
    // TODO: split into its own debugger struct?
    struct cpu last_cpu;
    struct inst debug_last_inst[5];
    int16_t debug_last_inst_addr[5];
    int debug_last_pointer;
    int debug_mode : 1;
};

void gb_debug_print_bin(char byte)
{
    printf("%d%d%d%d%d%d%d%d",
           (byte & (1 << 7)) != 0,
           (byte & (1 << 6)) != 0,
           (byte & (1 << 5)) != 0,
           (byte & (1 << 4)) != 0,
           (byte & (1 << 3)) != 0,
           (byte & (1 << 2)) != 0,
           (byte & (1 << 1)) != 0,
           (byte & 1) != 0);
}

void gb_debug_print_diff_int(int is_diff, int num)
{
    if (is_diff)
    {
        printf(TERM_COLOR_CYAN);
        printf("%d", num);
        printf(TERM_COLOR_RESET);
    }
    else
    {
        printf("%d", num);
    }
}

void gb_debug_print_diff_16_bit(int is_diff, int value)
{
    if (is_diff)
    {
        printf(TERM_COLOR_CYAN);
        printf("0x%04X", value & 0xFFFF);
        printf(TERM_COLOR_RESET);
    }
    else
    {
        printf("0x%04X", value & 0xFFFF);
    }
}

void gb_debug_print_diff_8_bit(int is_diff, int value)
{
    if (is_diff)
    {
        printf(TERM_COLOR_CYAN);
        printf("0x%02X", value & 0xFF);
        printf(TERM_COLOR_RESET);
    }
    else
    {
        printf("0x%02X", value & 0xFF);
    }
}

void gb_debug_print_last_inst(struct gb *gb, int item)
{
    int total = gb->debug_last_pointer > 4 ? 5 : gb->debug_last_pointer;
    if (item < total)
    {
        char buf[32];
        inst_write(&gb->debug_last_inst[item], buf);
        printf("    %04X : %s", gb->debug_last_inst_addr[item], buf);
    }
}

void gb_debug_print_next_inst(struct gb *gb, int item)
{
    if (item < 1)
        return;

    struct inst inst;
    uint16_t pc = gb->cpu->PC;
    int pc_offset = 0;

    inst_decode((unsigned char *)&gb->cpu->ram[pc], &inst);
    for (int i = 0; i < item; i++)
    {
        pc_offset += inst_decode((unsigned char *)&gb->cpu->ram[pc + pc_offset], &inst);
    }

    char buf[32];
    inst_write(&inst, buf);
    if (item == 1)
    {
        printf(" %s->%s %04X : %s", TERM_COLOR_GREEN, TERM_COLOR_RESET, pc + pc_offset, buf);
    }
    else
    {
        printf("    %04X : %s", pc + pc_offset, buf);
    }
}

void gb_debug_print(struct gb *gb)
{
    char buf[32];
    struct inst ins;
    unsigned char *pc = ((unsigned char *)gb->last_cpu.ram) + gb->last_cpu.PC;

    int ilen = inst_decode(pc, &ins);
    inst_write(&ins, buf);

    printf("inst:\t\t%s\n", buf);

    printf("inst (hex):\t");
    for (int i = 0; i < ilen; i++)
    {
        printf("0x%2X ", pc[i]);
    }
    printf("\n");
    printf("inst (bin):\t");
    for (int i = 0; i < ilen; i++)
    {
        gb_debug_print_bin(pc[i]);
        printf(" ");
    }

    printf("\n\n");

    printf("\t -----------------------------------------      Z:   ");
    gb_debug_print_diff_int(gb->last_cpu.Z != gb->cpu->Z, gb->cpu->Z);
    printf("\n");
    printf("\t|                    PC                   |     N:   ");
    gb_debug_print_diff_int(gb->last_cpu.N != gb->cpu->N, gb->cpu->N);
    printf("\n");
    printf("\t|-----------------------------------------|     H:   ");
    gb_debug_print_diff_int(gb->last_cpu.H != gb->cpu->H, gb->cpu->H);
    printf("\n");
    printf("\t|                  ");
    gb_debug_print_diff_16_bit(gb->last_cpu.PC != gb->cpu->PC, gb->cpu->PC);
    printf("                 |     CY:  ");
    gb_debug_print_diff_int(gb->last_cpu.CY != gb->cpu->CY, gb->cpu->CY);
    printf("\n");

    printf("\t|-----------------------------------------|\n");
    printf("\t|                    SP                   |\n");
    printf("\t|-----------------------------------------|   Code:\n");
    printf("\t|                  ");
    gb_debug_print_diff_16_bit(gb->last_cpu.SP != gb->cpu->SP, gb->cpu->SP);
    printf("                 |");
    gb_debug_print_last_inst(gb, 4);
    printf("\n");

    printf("\t|-----------------------------------------|");
    gb_debug_print_last_inst(gb, 3);
    printf("\n");
    printf("\t|         A          |          F         |");
    gb_debug_print_last_inst(gb, 2);
    printf("\n");
    printf("\t|--------------------|--------------------|");
    printf("\n");
    gb_debug_print_last_inst(gb, 1);
    printf("\t|       ");

    gb_debug_print_diff_8_bit(gb->last_cpu.A != gb->cpu->A, gb->cpu->A);
    printf("         |        ");
    gb_debug_print_diff_8_bit(gb->last_cpu.F != gb->cpu->F, gb->cpu->F);
    printf("        |");
    gb_debug_print_last_inst(gb, 0);
    printf("\n");

    printf("\t|-----------------------------------------|\n");
    printf("\t|         B          |          C         |");
    gb_debug_print_next_inst(gb, 1);
    printf("\n");
    printf("\t|--------------------|--------------------|");
    gb_debug_print_next_inst(gb, 2);
    printf("\n");
    printf("\t|       ");
    gb_debug_print_diff_8_bit(gb->last_cpu.B != gb->cpu->B, gb->cpu->B);
    printf("         |        ");
    gb_debug_print_diff_8_bit(gb->last_cpu.C != gb->cpu->C, gb->cpu->C);
    printf("        |");
    gb_debug_print_next_inst(gb, 3);
    printf("\n");
    printf("\t|-----------------------------------------|");
    gb_debug_print_next_inst(gb, 4);
    printf("\n");
    printf("\t|         D          |          E         |");
    gb_debug_print_next_inst(gb, 5);
    printf("\n");
    printf("\t|--------------------|--------------------|\n");
    printf("\t|       ");
    gb_debug_print_diff_8_bit(gb->last_cpu.D != gb->cpu->D, gb->cpu->D);
    printf("         |        ");
    gb_debug_print_diff_8_bit(gb->last_cpu.E != gb->cpu->E, gb->cpu->E);
    printf("        |\n");

    printf("\t|-----------------------------------------|\n");
    printf("\t|         H          |          L         |\n");
    printf("\t|--------------------|--------------------|\n");
    printf("\t|       ");
    gb_debug_print_diff_8_bit(gb->last_cpu.H != gb->cpu->H, gb->cpu->H);
    printf("         |        ");
    gb_debug_print_diff_8_bit(gb->last_cpu.L != gb->cpu->L, gb->cpu->L);
    printf("        |\n");

    printf("\t ----------------------------------------- \n");
}

int gb_debug_exec_inst(struct gb *gb)
{
    struct cpu snap = cpu_copy(gb->cpu);
    gb->last_cpu = snap;

    // populate the queue of last instructions
    inst_decode((unsigned char *)&gb->cpu->ram[gb->cpu->PC], &gb->debug_last_inst[gb->debug_last_pointer % 5]);
    gb->debug_last_inst_addr[gb->debug_last_pointer % 5] = gb->cpu->PC;
    gb->debug_last_pointer++;

    return cpu_exec(gb->cpu, 1);
}

void gb_debug_break(struct gb *gb)
{
    char *line = NULL;
    size_t lencap = 0;
    ssize_t linelen;

    gb_debug_print(gb);
    printf("\n");

    while (1)
    {
        printf("\ndebug> ");
        linelen = getline(&line, &lencap, stdin);
        line[linelen - 1] = '\0';

        char *tokens[5]; // only handle 5 tokens for now
        int t = 0;
        while ((tokens[t] = strsep(&line, " ")) != NULL)
        {
            t++;
        }
        if (strcmp(tokens[0], "n") == 0)
        {
            if (!gb_debug_exec_inst(gb))
            {
                printf("error executing instruction\n");
            }
        }
        else
        {
            printf("unknown command: ");
            for (int i = 0; i < t; i++)
            {
                printf("%s ", tokens[i]);
            }
            printf("\n");
        }
        free(line);
    }
}

void gb_run(struct gb *gb)
{
    if (gb->debug_mode)
    {
        // always run the first instruction
        if (!gb_debug_exec_inst(gb))
        {
            printf("error executing instruction\n");
            exit(1);
        }
        while (1)
        {
            gb_debug_break(gb);
            exit(0);
        }
    }
    else
    {
        fprintf(stderr, "Error: can only run in debug mode for now\n");
        exit(1);
    }
}

// load 32K ROM into starting address 0x150
void gb_load_rom(struct gb *gb, char *filename)
{
    FILE *fin = fopen(filename, "rb");

    // TODO: this error handling looks wrong
    // TODO: perhaps make reading roms common in some library?
    int err;
    if (NULL == fin)
    {
        fprintf(stderr, "Error opening file: %s\n", filename);
        exit(1);
    }

    char b;
    int addr;
    while (addr < 0x8150)
    {
        size_t c = fread(&b, 1, 1, fin);
        if (c == 0)
        {
            break;
        }
        if (addr < 0x150)
        {
            addr++;
            continue;
        }
        gb->ram[addr] = b;
        addr++;
    }

    if (addr < 0x150)
    {
        fprintf(stderr, "Error reading ROM: cannot find code starting at addresss 0x150: %x\n", addr);
        exit(1);
    }
    if (addr != 0x8150)
    {
        fprintf(stderr, "Error reading ROM: cannot read full 32K of ROM: %x\n", addr);
        exit(1);
    }
    gb->cpu->PC = 0x150;
    gb->cpu->SP = 0xFFFE;

    fclose(fin);
}

// TODO: use getopt
int main(int argc, char *argv[])
{
    if (argc == 3)
    {
        struct cpu cpu = {0};
        struct gb gb = {0};

        cpu.ram = gb.ram;
        gb.debug_mode = 1;
        gb.cpu = &cpu;
        gb_load_rom(&gb, argv[2]);
        gb_run(&gb);
    }
    else
    {
        fprintf(stderr, "Error: can only run in debug mode now\n");
        return 1;
    }
    return 0;
}