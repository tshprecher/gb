#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "../inst.h"

// TODO: remove this once decode.o library is complete
int match_n(char *, unsigned char *);

char *get_register_old(unsigned char b, int start_bit)
{
    b >>= (start_bit - 2);
    b = b & 0b111;
    switch (b)
    {
    case 0b111:
        return "A";
    case 0b000:
        return "B";
    case 0b001:
        return "C";
    case 0b010:
        return "D";
    case 0b011:
        return "E";
    case 0b100:
        return "H";
    case 0b101:
        return "L";
    }
    // should never hit
    return NULL;
}

char *get_register_pair_dd_old(unsigned char b, int start_bit)
{
    b >>= (start_bit - 1);
    int i = b & 0b11;
    char *dd[] = {"BC", "DE", "HL", "SP"};
    return dd[i];
}

char *get_register_pair_ss_old(unsigned char b, int start_bit)
{
    return get_register_pair_dd_old(b, start_bit);
}

char *get_register_pair_qq_old(unsigned char b, int start_bit)
{
    b >>= (start_bit - 1);
    int i = b & 0b11;
    char *qq[] = {"BC", "DE", "HL", "AF"};
    return qq[i];
}

char *get_condition(unsigned char b, int start_bit)
{
    b >>= (start_bit - 1);
    int i = b & 0b11;
    char *cc[] = {"NZ", "Z", "NC", "C"};
    return cc[i];
}

int get_bit(unsigned char b, int start_bit)
{
    b >>= (start_bit - 2);
    return b & 0b111;
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        fprintf(stderr, "Error: missing ROM file\n");
        return 1;
    }

    int err;

    FILE *fin = fopen(argv[1], "rb");
    if (NULL == fin)
    {
        fprintf(stderr, "Error opening file: %s\n", strerror(err));
        return err;
    }

    FILE *fout = fdopen(STDOUT_FILENO, "w");
    if (NULL == fout)
    {
        fprintf(stderr, "Error writing output: %s\n", strerror(err));
        return err;
    }

    // code starts at 0x150
    char b;
    int addr = 0;
    while (addr < 0x150)
    {
        size_t c = fread(&b, 1, 1, fin);
        if (c == 0)
        {
            break;
        }
        addr++;
    }

    if (addr != 0x150)
    {
        fprintf(stderr, "Error reading ROM: cannot find code starting at addresss 0x150: %x\n", addr);
        return 1;
    }

    int res;

    struct inst decoded;
    unsigned char ibuf[3];
    char obuf[32];
    int count_read;
    while ((count_read = fread(ibuf, 1, 3, fin)) != EOF)
    {
        int len = inst_decode(ibuf, &decoded);
        if (len == -1)
        {
            fprintf(stderr, "Error decoding instruction with bytes: 0x%02X 0x%02X 0x%02X\n", ibuf[0], ibuf[1], ibuf[2]);
        }
        fprintf(fout, "0x%04X\t", addr);
        addr += len;
        inst_write(&decoded, obuf);
        fprintf(fout, "%s\n", obuf);

        if (count_read < 3)
        {
            // we read fewer chars than asked, so assume end is reached
            // TODO: doesn't always work because we may read fewer without reaching end
            break;
        }
        else
        {
            for (int l = 2; l >= len; l--)
            {
                ungetc(ibuf[l], fin);
            }
        }
    }

    return 0;
}
