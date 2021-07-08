#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "../inst.h"

int disasm(char *filename, int raw_addresses)
{
    FILE *fin = fopen(filename, "rb");
    if (NULL == fin)
    {
        perror("error");
        return 1;
    }

    FILE *fout = fdopen(STDOUT_FILENO, "w");
    if (NULL == fout)
    {
        perror("error");
        return 1;
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
        fprintf(stderr, "error: cannot find code starting at addresss 0x150\n");
        return 1;
    }

    int res;

    struct inst decoded;
    unsigned char ibuf[3];
    char obuf[16];
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
        inst_write(&decoded, obuf, raw_addresses);
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

int main(int argc, char *argv[])
{
    int raw_addresses = 0;
    char *filename = NULL;
    int c;

    while ((c = getopt(argc, argv, "rf:")) != -1)
        switch (c)
        {
        case 'r':
            raw_addresses = 1;
            break;
        default:
            fprintf(stderr, "error: unknown option\n");
            return 1;
        }

    int index = optind;
    if (index < argc)
    {
        filename = argv[index++];
    }

    if (filename == NULL)
    {
        fprintf(stderr, "error: missing input file\n");
        return 1;
    }

    return disasm(filename, raw_addresses);
}
