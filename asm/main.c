#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "../inst.h"


// TODO: use getopt
int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        fprintf(stderr, "Error: missing ROM file\n");
        return 1;
    }

    // TODO: this error handling looks wrong
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
