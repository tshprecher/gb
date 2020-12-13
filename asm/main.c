#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

// Returns true iff the instruction byte matches the pattern.
// Spaces separate bytes. Special characters:
//   - 'r' matches any 3 bit pattern except 110 indicating a register id
//   - 'd', 'q', and 's' match any 2 bit pattern indicating a register pair
//   - 'n' matches any byte
int match_n(char *pattern, unsigned char *buf)
{
    int shift = 0;
    int c = 0;
    char ch = buf[c++];
    while (1)
    {
        // handle boundary conditions
        if (*pattern == ' ' || *pattern == '\0')
        {
            while (*pattern == ' ')
            {
                pattern++;
            }
            if (*pattern == '\0')
            {
                return shift == 8;
            }
            if (shift != 8)
            {
                return 0;
            }
            shift = 0;
            ch = buf[c++];
            continue;
        }

        // match
        char si = ch << shift;
        switch (*pattern)
        {
        case '0':
            if (128 & si)
                return 0;
            shift++;
            break;
        case '1':
            if (!(128 & si))
                return 0;
            shift++;
            break;
        case 'c':
            shift += 2;
            break;
        case 'r':
            if ((unsigned char)((0xE0 & si) >> 5) == 6)
            {
                return 0;
            }
            shift += 3;
            break;
        case 'b':
        case 't':
            shift += 3;
            break;
        case 'd':
        case 'q':
        case 's':
            shift += 2;
            break;
        case 'n':
            shift += 8;
            break;
        default:
            return 0;
        }
        pattern++;
    }

    // should never occur
    return 0;
}

void test_match_n()
{
    unsigned char buf[3] = {0b01000000, 1, 1};
    printf("should be true -> %d\n", match_n("01rr", buf));
    printf("should be false -> %d\n", match_n("01r", buf));
    printf("should be false -> %d\n", match_n("01r 00000000", buf));
    printf("should be false -> %d\n", match_n("01rrr", buf));
    printf("should be true -> %d\n", match_n("01rr 00000001", buf));
    printf("should be true -> %d\n", match_n("01rr 00000001    ", buf));
    printf("should be false -> %d\n", match_n("01rr 00000000", buf));
    printf("should be false -> %d\n", match_n("01rr 00000000 ", buf));

    buf[0] = 0b11001101;
    printf("should be true -> %d\n", match_n("11001101 n n", buf));
}

char *get_register(unsigned char b, int start_bit)
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

char *get_register_pair_dd(unsigned char b, int start_bit)
{
    b >>= (start_bit - 1);
    int i = b & 0b11;
    char *dd[] = {"BC", "DE", "HL", "SP"};
    return dd[i];
}

char *get_register_pair_ss(unsigned char b, int start_bit)
{
    return get_register_pair_dd(b, start_bit);
}

char *get_register_pair_qq(unsigned char b, int start_bit)
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

// returns the number of bytes of the instruction if properly decoded, 
// EOF on end of input read.
int decode_inst(FILE *in, FILE *out)
{
    unsigned char ib[3]; // instruction buffer
    int ilen = 0;        // instruction length of matched instruction
    if (fread(ib + ilen, 1, 1, in) == 0)
    {
        return EOF;
    }
    ilen++;

    // single byte instructions

    if (match_n("01rr", ib))
    { // load register to register
        fprintf(out, "LD %s,%s\n", get_register(ib[0], 5), get_register(ib[0], 2));
    }
    else if (match_n("01r110", ib))
    { // LD r, (HL)
        fprintf(out, "LD %s,(HL)\n", get_register(ib[0], 5));
    }
    else if (match_n("01110r", ib))
    { // LD (HL), r
        fprintf(out, "LD (HL),%s\n", get_register(ib[0], 2));
    }
    else if (match_n("00001010", ib))
    {
        fprintf(out, "LD A,(BC)\n");
    }
    else if (match_n("00011010", ib))
    {
        fprintf(out, "LD A,(DE)\n");
    }
    else if (match_n("11110010", ib))
    { // reads internal RAM to A
        fprintf(out, "LD A,(C)\n");
    }
    else if (match_n("11100010", ib))
    {
        fprintf(out, "LD (C),A\n"); // write internal RAM from A
    }
    else if (match_n("00101010", ib))
    { // load (HL) into A, increment HL
        fprintf(out, "LD A,(HLI)\n");
    }
    else if (match_n("00111010", ib))
    { // load (HL) into A, decrement HL
        fprintf(out, "LD A,(HLD)\n");
    }
    else if (match_n("00000010", ib))
    { // load A into (BL)
        fprintf(out, "LD (BC),A\n");
    }
    else if (match_n("00010010", ib))
    { // load A into (DE)
        fprintf(out, "LD (DE),A\n");
    }
    else if (match_n("00100010", ib))
    { // load A into (HL), increment HL
        fprintf(out, "LD (HLI), A\n");
    }
    else if (match_n("00110010", ib))
    { // load A into (HL), decrement HL
        fprintf(out, "LD (HLD), A\n");
    }
    else if (match_n("11111001", ib))
    { // load HL into SP
        fprintf(out, "LD HL,SP\n");
    }
    else if (match_n("11q0101", ib))
    { // pushes the register pair to the stack, decrement SP by 2
        fprintf(out, "PUSH %s\n", get_register_pair_qq(ib[0], 5));
    }
    else if (match_n("11q0001", ib))
    { // pop the register pair from the stack, increment SP by 2
        fprintf(out, "POP %s\n", get_register_pair_qq(ib[0], 5));
    }
    else if (match_n("10000r", ib))
    { // add value of register r to A
        fprintf(out, "ADD A,%s\n", get_register(ib[0], 2));
    }
    else if (match_n("10000110", ib))
    { // add value at address HL to A
        fprintf(out, "ADD A,(HL)\n");
    }
    else if (match_n("10001r", ib))
    {
        fprintf(out, "ADC A,%s\n", get_register(ib[0], 2));
    }
    else if (match_n("10001110", ib))
    {
        fprintf(out, "ADC A,(HL)\n");
    }
    else if (match_n("10010r", ib))
    { // subtract r from A
        fprintf(out, "SUB %s\n", get_register(ib[0], 2));
    }
    else if (match_n("10010110", ib))
    { // subtract value at (HL) from A
        fprintf(out, "SUB (HL)\n");
    }
    else if (match_n("10011r", ib))
    { // subtract (r+CY) from A
        fprintf(out, "SBC %s\n", get_register(ib[0], 2));
    }
    else if (match_n("10011110", ib))
    { // subtract ((HL) + CY) from A
        fprintf(out, "SBC A,(HL)\n");
    }
    else if (match_n("10100r", ib))
    { // do bitwise AND with register, store in A
        fprintf(out, "AND %s\n", get_register(ib[0], 2));
    }
    else if (match_n("10100110", ib))
    { // do bitwise AND with value at (HL), store in A
        fprintf(out, "AND (HL)\n");
    }
    else if (match_n("10110r", ib))
    { // do bitwise OR with register, store in A
        fprintf(out, "OR %s\n", get_register(ib[0], 2));
    }
    else if (match_n("10110110", ib))
    { // do bitwise OR with value at (HL), store in A
        fprintf(out, "OR (HL)\n");
    }
    else if (match_n("10101r", ib))
    { // do bitwise XOR with register, store in A
        fprintf(out, "XOR %s\n", get_register(ib[0], 2));
    }
    else if (match_n("10101110", ib))
    { // do bitwise XOR with value at (HL), store in A
        fprintf(out, "XOR (HL)\n");
    }
    else if (match_n("10111r", ib))
    { // compare A with register, set flag if they're equal
        fprintf(out, "CP %s\n", get_register(ib[0], 2));
    }
    else if (match_n("10111110", ib))
    { // compare with value at (HL), set flag if they're equal
        fprintf(out, "CP (HL)\n");
    }
    else if (match_n("00r100", ib))
    { // increment register
        fprintf(out, "INC %s\n", get_register(ib[0], 5));
    }
    else if (match_n("00110100", ib))
    { // increment (HL)
        fprintf(out, "INC (HL)\n");
    }
    else if (match_n("00r101", ib))
    { // decrement register
        fprintf(out, "DEC %s\n", get_register(ib[0], 5));
    }
    else if (match_n("00110101", ib))
    { // decrement (HL)
        fprintf(out, "DEC (HL)\n");
    }
    else if (match_n("00s1001", ib))
    { // add the contents of register ss to HL, store in HL
        fprintf(out, "ADD HL,%s\n", get_register_pair_ss(ib[0], 5));
    }
    else if (match_n("00s0011", ib))
    { // increment register ss
        fprintf(out, "INC %s\n", get_register_pair_ss(ib[0], 5));
    }
    else if (match_n("00s1011", ib))
    { // decrement register ss
        fprintf(out, "DEC %s\n", get_register_pair_ss(ib[0], 5));
    }
    else if (match_n("00000111", ib))
    { // rotate A to the left
        fprintf(out, "RLCA\n");
    }
    else if (match_n("00010111", ib))
    { // rotate A to the left
        fprintf(out, "RLA\n");
    }
    else if (match_n("00001111", ib))
    { // rotate A to the right
        fprintf(out, "RRCA\n");
    }
    else if (match_n("00011111", ib))
    { // rotate A to the right
        fprintf(out, "RRA\n");
    }
    else if (match_n("11101001", ib))
    { // load the contents in register HL to PC
        fprintf(out, "JP HL\n");
    }
    else if (match_n("11001001", ib))
    {
        fprintf(out, "RET\n");
    }
    else if (match_n("110c000", ib))
    { // conditional return
        fprintf(out, "RET %s\n", get_condition(ib[0], 5));
    }
    else if (match_n("11011001", ib))
    {
        fprintf(out, "RETI\n");
    }
    else if (match_n("11t111", ib))
    {
        fprintf(out, "RST %d\n", get_bit(ib[0], 5));
    }
    else if (match_n("00100111", ib))
    {
        fprintf(out, "DAA\n");
    }
    else if (match_n("00101111", ib))
    {
        fprintf(out, "CPL\n");
    }
    else if (match_n("00000000", ib))
    {
        fprintf(out, "NOP\n");
    }
    else if (match_n("11110011", ib))
    {
        fprintf(out, "DI\n");
    }
    else if (match_n("11111011", ib))
    {
        fprintf(out, "EI\n");
    }
    else if (match_n("01110110", ib))
    {
        fprintf(out, "HALT\n");
    }
    else
    {
        // two byte instructions

        if (fread(ib + ilen, 1, 1, in) == 0)
        {            
            return 0;
        }
        ilen++;

        if (match_n("00r110 n", ib))
        { // load 8-bit immediate
            fprintf(out, "LD %s,0x%02hhX\n", get_register(ib[0], 5), ib[1]);
        }
        else if (match_n("00110110 n", ib))
        { // load 8-bit immediate to memory
            fprintf(out, "LD (HL),0x%02hhX\n", ib[1]);
        }
        else if (match_n("11110000 n", ib))
        { // read from internal RAM address to A
            fprintf(out, "LDH A,(0x%02hhX)\n", ib[1]);
        }
        else if (match_n("11100000 n", ib))
        { // write to internal RAM address from A
            fprintf(out, "LDH (0x%02hhX),A\n", ib[1]);
        }
        else if (match_n("11111000 n", ib))
        { // add 8-bit signed to SP and store in HL
            char sb = ib[1];
            fprintf(out, "LDHL SP,%d\n", sb);
        }
        else if (match_n("11000110 n", ib))
        { // add 8-bit immediate to A
            fprintf(out, "ADD A,0x%02hhX\n", ib[1]);
        }
        else if (match_n("11001110 n", ib))
        { // add 8-bit immediate + CY to A
            fprintf(out, "ADC A,0x%02hhX\n", ib[1]);
        }
        else if (match_n("11010110 n", ib))
        { // subtract 8-bit immediate from A
            fprintf(out, "SUB 0x%02hhX\n", ib[1]);
        }
        else if (match_n("11011110 n", ib))
        { // subtract (8-bit immediate + CY) from A
            fprintf(out, "SBC A,0x%02hhX\n", ib[1]);
        }
        else if (match_n("11100110 n", ib))
        { // do bitwise AND with 8-bit immediate, store in A
            fprintf(out, "AND 0x%02hhX\n", ib[1]);
        }
        else if (match_n("11110110 n", ib))
        { // do bitwise OR with 8-bit immediate, store in A
            fprintf(out, "OR 0x%02hhX\n", ib[1]);
        }
        else if (match_n("11101110 n", ib))
        { // do bitwise XOR with 8-bit immediate, store in A
            fprintf(out, "XOR 0x%02hhX\n", ib[1]);
        }
        else if (match_n("11111110 n", ib))
        { // compare A with 8-bit immediate, set flag if they're equal
            fprintf(out, "CP 0x%02hhX\n", ib[1]);
        }
        else if (match_n("11101000 n", ib))
        { // add 8-bit signed to SP and store in A
            char sb = ib[1];
            fprintf(out, "ADD SP,%d\n", sb);
        }
        else if (match_n("11001011 000000r", ib))
        { // rotate register to the left
            fprintf(out, "RLC %s\n", get_register(ib[1], 2));
        }
        else if (match_n("11001011 000000110", ib))
        { // rotate (HL) to the left
            fprintf(out, "RLC (HL)\n");
        }
        else if (match_n("11001011 000010r", ib))
        { // rotate register to the left
            fprintf(out, "RL %s\n", get_register(ib[1], 2));
        }
        else if (match_n("11001011 000010110", ib))
        { // rotate (HL) to the left
            fprintf(out, "RL (HL)\n");
        }
        else if (match_n("11001011 000001r", ib))
        { // rotate register to the right
            fprintf(out, "RRC %s\n", get_register(ib[1], 2));
        }
        else if (match_n("11001011 000001110", ib))
        { // rotate (HL) to the right
            fprintf(out, "RRC (HL)\n");
        }
        else if (match_n("11001011 000011r", ib))
        { // rotate register to the right
            fprintf(out, "RR %s\n", get_register(ib[1], 2));
        }
        else if (match_n("11011011 00011110", ib))
        { // rotate (HL) to the right
            fprintf(out, "RR (HL)\n");
        }
        else if (match_n("11001011 00100r", ib))
        { // shift the contents of the register to the left
            fprintf(out, "SLA %s\n", get_register(ib[1], 2));
        }
        else if (match_n("11011011 00100110", ib))
        { // shift (HL) to the left
            fprintf(out, "SLA (HL)\n");
        }
        else if (match_n("11001011 00101r", ib))
        { // shift the contents of the register to the right
            fprintf(out, "SRA %s\n", get_register(ib[1], 2));
        }
        else if (match_n("11001011 00101110", ib))
        { // shift (HL) to the right
            fprintf(out, "SRA (HL)\n");
        }
        else if (match_n("11001011 00111r", ib))
        {
            // shift register to the right, reset bit 7
            fprintf(out, "SRL %s\n", get_register(ib[1], 2));
        }
        else if (match_n("11001011 00111110", ib))
        { // shift (HL) to the right, reset bit 7
            fprintf(out, "SRL (HL)\n");
        }
        else if (match_n("11001011 00110r", ib))
        { // swap the high order 4 bits with the low order 4 bits of a register
            fprintf(out, "SWAP %s\n", get_register(ib[1], 2));
        }
        else if (match_n("11001011 00110110", ib))
        { // swap the high order 4 bits with the low order 4 bits of (HL)
            fprintf(out, "SWAP (HL)\n");
        }
        else if (match_n("11001011 01br", ib))
        { // copy the complement of the bit in r to the Z flag of the program status word
            fprintf(out, "BIT %d,%s\n", get_bit(ib[1], 5), get_register(ib[1], 2));
        }
        else if (match_n("11001011 01b110", ib))
        { // copy the complement of the bit in (HL) to the Z flag of the program status word
            fprintf(out, "BIT %d,(HL)\n", get_bit(ib[1], 5));
        }
        else if (match_n("11001011 11br", ib))
        { // set to 1 the bit in specified register
            fprintf(out, "SET %d,%s\n", get_bit(ib[1], 5), get_register(ib[1], 2));
        }
        else if (match_n("11001011 11b110", ib))
        { // set to 1 the bit in the specified memory
            fprintf(out, "SET %d,(HL)\n", get_bit(ib[1], 5));
        }
        else if (match_n("11001011 10br", ib))
        { // reset to 0 the specified bit in the register
            fprintf(out, "RES %d,%s\n", get_bit(ib[1], 5), get_register(ib[1], 2));
        }
        else if (match_n("11001011 10b110", ib))
        { // reset to 0 the bit in the specified memory
            fprintf(out, "RES %d,(HL)\n", get_bit(ib[1], 5));
        }
        else if (match_n("00011000 n", ib))
        { // jump -126 to +129 steps from current PC
            char sb = ib[1];
            int si = sb;
            fprintf(out, "JR %d\n", si + 2);
        }
        else if (match_n("001c000 n", ib))
        { // conditionally jump -126 to +129 steps from current PC
            char sb = ib[1];
            int si = sb;
            fprintf(out, "JR %s,%d\n", get_condition(ib[0], 4), si + 2);
        }
        else if (match_n("00010000 00000000", ib))
        {
            fprintf(out, "STOP\n");
        }
        else
        {

            // three byte instructions

            if (fread(ib + ilen, 1, 1, in) == 0)
            {
                return 0;
            }
            ilen++;

            if (match_n("11111010 n n", ib))
            { // read from 16-bit RAM address into A
                fprintf(out, "LD A,(0x%02hhX%02hhX)\n", ib[2], ib[1]);
            }
            else if (match_n("11101010 n n", ib))
            { // write to 16-bit RAM address from A
                fprintf(out, "LD (0x%02hhX%02hhX),A\n", ib[2], ib[1]);
            }
            else if (match_n("00d0001 n n", ib))
            { // load immediate into register pair
                fprintf(out, "LD %s,0x%02hhX%02hhX\n", get_register_pair_dd(ib[0], 5), ib[2], ib[1]);
            }
            else if (match_n("00001000 n n", ib))
            { // write SP to address in RAM
                fprintf(out, "LD (0x%02hhX%02hhX),SP\n", ib[2], ib[1]);
            }
            else if (match_n("11000011 n n", ib))
            { // set the PC to the literal
                fprintf(out, "JP 0x%02hhX%02hhX\n", ib[2], ib[1]);
            }
            else if (match_n("110c010 n n", ib))
            { // set the PC based on the condition
                fprintf(out, "JP %s,0x%02hhX%02hhX\n", get_condition(ib[0], 4), ib[2], ib[1]);
            }
            else if (match_n("11001101 n n", ib))
            {
                fprintf(out, "CALL 0x%02hhX%02hhX\n", ib[2], ib[1]);
            }
            else if (match_n("110c100 n n", ib))
            {
                fprintf(out, "CALL %s,0x%02hhX%02hhX\n", get_condition(ib[0], 4), ib[2], ib[1]);
            }
            else
            {
                fprintf(out, "db 0x%02hhX\n", ib[0]);
                ungetc(ib[2], in);
                ungetc(ib[1], in);
                return 1;
            }
        }
    }

    return ilen;
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

    //test_match_n();
    int res;
    fprintf(fout, "0x%04x\t", addr);

    while ((res = decode_inst(fin, fout)) != EOF)
    {
        addr += res;
        fprintf(fout, "0x%04x\t", addr);
    }

    return 0;
}
