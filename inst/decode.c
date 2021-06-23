#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../inst.h"

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

enum reg get_register(unsigned char b, int start_bit)
{
    b >>= (start_bit - 2);
    b = b & 0b111;

    switch (b)
    {
    case 0b111:
        return A;
    case 0b000:
        return B;
    case 0b001:
        return C;
    case 0b010:
        return D;
    case 0b011:
        return E;
    case 0b100:
        return H;
    case 0b101:
        return L;
    }
    // should never hit
    return -1;
}

enum reg get_register_pair_dd(unsigned char b, int start_bit)
{
    b >>= (start_bit - 1);
    int i = b & 0b11;
    enum reg dd[] = {BC, DE, HL, SP};
    return dd[i];
}

enum reg get_register_pair_qq(unsigned char b, int start_bit)
{
    b >>= (start_bit - 1);
    int i = b & 0b11;
    enum reg qq[] = {BC, DE, HL, AF};
    return qq[i];
}

enum cond get_cond(unsigned char b, int start_bit)
{
    b >>= (start_bit - 1);
    int i = b & 0b11;
    enum cond cc[] = {NZ, Z, NC, YC};
    return cc[i];
}

char get_register_pair_ss(unsigned char b, int start_bit)
{
    return get_register_pair_dd(b, start_bit);
}

int8_t get_8bit(unsigned char b, int start_bit)
{
    b >>= (start_bit - 2);
    return b & 0b111;
}

int16_t get_16bit(unsigned char upper, unsigned char lower)
{
    int16_t res = upper;
    res <<= 8;
    return res | lower;
}

// TODO: make this signed, no reason why it's unsigned.
int inst_decode(unsigned char *ib, struct inst *decoded)
{
    memset(decoded, 0, sizeof(struct inst));

    // instruction length of matched instruction
    int ilen = 1;

    // single byte instructions

    if (match_n("00111111", ib))
    { // invert carry flag
        decoded->type = CCF;
    }
    else if (match_n("01rr", ib))
    { // load register to register
        decoded->type = LD;
        decoded->r_d = get_register(ib[0], 5);
        decoded->r_s = get_register(ib[0], 2);
    }
    else if (match_n("01r110", ib))
    { // LD r, (HL)
        decoded->type = LD;
        decoded->r_d = get_register(ib[0], 5);
        decoded->raddr_s = HL;
    }
    else if (match_n("01110r", ib))
    { // LD (HL), r
        decoded->type = LD;
        decoded->r_s = get_register(ib[0], 2);
        decoded->raddr_d = HL;
    }
    else if (match_n("00001010", ib))
    {
        decoded->type = LD;
        decoded->r_d = A;
        decoded->raddr_s = BC;
    }
    else if (match_n("00011010", ib))
    {
        decoded->type = LD;
        decoded->r_d = A;
        decoded->raddr_s = DE;
    }
    else if (match_n("11110010", ib))
    { // reads internal RAM to A
        decoded->type = LD;
        decoded->r_d = A;
        decoded->raddr_s = C;
    }
    else if (match_n("11100010", ib))
    {
        // write internal RAM from A
        decoded->type = LD;
        decoded->r_s = A;
        decoded->raddr_d = C;
    }
    else if (match_n("00101010", ib))
    { // load (HL) into A, increment HL
        decoded->type = LD;
        decoded->r_d = A;
        decoded->raddr_s = HL;
        decoded->incr = 1;
    }
    else if (match_n("00111010", ib))
    { // load (HL) into A, decrement HL
        decoded->type = LD;
        decoded->r_d = A;
        decoded->raddr_s = HL;
        decoded->decr = 1;
    }
    else if (match_n("00000010", ib))
    { // load A into (BC)
        decoded->type = LD;
        decoded->r_s = A;
        decoded->raddr_d = BC;
    }
    else if (match_n("00010010", ib))
    { // load A into (DE)
        decoded->type = LD;
        decoded->r_s = A;
        decoded->raddr_d = DE;
    }
    else if (match_n("00100010", ib))
    { // load A into (HL), increment HL
        decoded->type = LD;
        decoded->r_s = A;
        decoded->raddr_d = HL;
        decoded->incr = 1;
    }
    else if (match_n("00110010", ib))
    { // load A into (HL), decrement HL
        decoded->type = LD;
        decoded->r_s = A;
        decoded->raddr_d = HL;
        decoded->decr = 1;
    }
    else if (match_n("11111001", ib))
    { // load HL into SP
        decoded->type = LD;
        decoded->r_s = HL;
        decoded->r_d = SP;
    }
    else if (match_n("11q0101", ib))
    { // pushes the register pair to the stack, decrement SP by 2
        decoded->type = PUSH;
        decoded->r_s = get_register_pair_qq(ib[0], 5);
    }
    else if (match_n("11q0001", ib))
    { // pop the register pair from the stack, increment SP by 2
        decoded->type = POP;
        decoded->r_d = get_register_pair_qq(ib[0], 5);
    }
    else if (match_n("00110111", ib))
    { // set the carry flag
        decoded->type = SCF;
    }
    else if (match_n("10000r", ib))
    { // add value of register r to A
        decoded->type = ADD;
        decoded->r_d = A;
        decoded->r_s = get_register(ib[0], 2);
    }
    else if (match_n("10000110", ib))
    { // add value at address HL to A
        decoded->type = ADD;
        decoded->r_d = A;
        decoded->raddr_s = HL;
    }
    else if (match_n("10001r", ib))
    {
        decoded->type = ADC;
        decoded->r_d = A;
        decoded->r_s = get_register(ib[0], 2);
    }
    else if (match_n("10001110", ib))
    {
        decoded->type = ADC;
        decoded->r_d = A;
        decoded->raddr_s = HL;
    }
    else if (match_n("10010r", ib))
    { // subtract r from A
        decoded->type = SUB;
        decoded->r_d = A;
        decoded->r_s = get_register(ib[0], 2);
    }
    else if (match_n("10010110", ib))
    { // subtract value at (HL) from A
        decoded->type = SUB;
        decoded->r_d = A;
        decoded->raddr_s = HL;
    }
    else if (match_n("10011r", ib))
    { // subtract (r+CY) from A
        decoded->type = SBC;
        decoded->r_d = A;
        decoded->r_s = get_register(ib[0], 2);
    }
    else if (match_n("10011110", ib))
    { // subtract ((HL) + CY) from A
        decoded->type = SBC;
        decoded->r_d = A;
        decoded->raddr_s = HL;
    }
    else if (match_n("10100r", ib))
    { // do bitwise AND with register, store in A
        decoded->type = AND;
        decoded->r_d = A;
        decoded->r_s = get_register(ib[0], 2);
    }
    else if (match_n("10100110", ib))
    { // do bitwise AND with value at (HL), store in A
        decoded->type = AND;
        decoded->r_d = A;
        decoded->raddr_s = HL;
    }
    else if (match_n("10110r", ib))
    { // do bitwise OR with register, store in A
        decoded->type = OR;
        decoded->r_d = A;
        decoded->r_s = get_register(ib[0], 2);
    }
    else if (match_n("10110110", ib))
    { // do bitwise OR with value at (HL), store in A
        decoded->type = OR;
        decoded->r_d = A;
        decoded->raddr_s = HL;
    }
    else if (match_n("10101r", ib))
    { // do bitwise XOR with register, store in A
        decoded->type = XOR;
        decoded->r_d = A;
        decoded->r_s = get_register(ib[0], 2);
    }
    else if (match_n("10101110", ib))
    { // do bitwise XOR with value at (HL), store in A
        decoded->type = XOR;
        decoded->r_d = A;
        decoded->raddr_s = HL;
    }
    else if (match_n("10111r", ib))
    { // compare A with register, set flag if they're equal
        decoded->type = CP;
        decoded->r_d = A;
        decoded->r_s = get_register(ib[0], 2);
    }
    else if (match_n("10111110", ib))
    { // compare with value at (HL), set flag if they're equal
        decoded->type = CP;
        decoded->r_d = A;
        decoded->raddr_s = HL;
    }
    else if (match_n("00r100", ib))
    { // increment register
        decoded->type = INC;
        decoded->r_d = get_register(ib[0], 5);
    }
    else if (match_n("00110100", ib))
    { // increment (HL)
        decoded->type = INC;
        decoded->raddr_d = HL;
    }
    else if (match_n("00r101", ib))
    { // decrement register
        decoded->type = DEC;
        decoded->r_d = get_register(ib[0], 5);
    }
    else if (match_n("00110101", ib))
    { // decrement (HL)
        decoded->type = DEC;
        decoded->raddr_d = HL;
    }
    else if (match_n("00s1001", ib))
    { // add the contents of register ss to HL, store in HL
        decoded->type = ADD;
        decoded->r_d = HL;
        decoded->r_s = get_register_pair_ss(ib[0], 5);
    }
    else if (match_n("00s0011", ib))
    { // increment register ss
        decoded->type = INC;
        decoded->r_d = get_register_pair_ss(ib[0], 5);
    }
    else if (match_n("00s1011", ib))
    { // decrement register ss
        decoded->type = DEC;
        decoded->r_d = get_register_pair_ss(ib[0], 5);
    }
    else if (match_n("00000111", ib))
    { // rotate A to the left
        decoded->type = RLCA;
    }
    else if (match_n("00010111", ib))
    { // rotate A to the left
        decoded->type = RLA;
    }
    else if (match_n("00001111", ib))
    { // rotate A to the right
        decoded->type = RRCA;
    }
    else if (match_n("00011111", ib))
    { // rotate A to the right
        decoded->type = RRA;
    }
    else if (match_n("11101001", ib))
    { // load the contents in register HL to PC
        decoded->type = JP;
        decoded->r_d = PC;
        decoded->r_s = HL;
    }
    else if (match_n("11001001", ib))
    { // return
        decoded->type = RET;
    }
    else if (match_n("110c000", ib))
    { // conditional return
        decoded->type = RET;
        decoded->cond = get_cond(ib[0], 4);
    }
    else if (match_n("11011001", ib))
    {
        decoded->type = RETI;
    }
    else if (match_n("11t111", ib))
    {
        decoded->type = RST;
        decoded->lit_8bit = get_8bit(ib[0], 5);
        decoded->has_lit_8bit = 1;
    }
    else if (match_n("00100111", ib))
    {
        decoded->type = DAA;
    }
    else if (match_n("00101111", ib))
    {
        decoded->type = CPL;
    }
    else if (match_n("00000000", ib))
    {
        decoded->type = NOP;
    }
    else if (match_n("11110011", ib))
    {
        decoded->type = DI;
    }
    else if (match_n("11111011", ib))
    {
        decoded->type = EI;
    }
    else if (match_n("01110110", ib))
    {
        decoded->type = HALT;
    }
    else
    {
        // two byte instructions
        ilen++;

        if (match_n("00r110 n", ib))
        { // load 8-bit immediate
            decoded->type = LD;
            decoded->r_d = get_register(ib[0], 5);
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("00110110 n", ib))
        { // load 8-bit immediate to memory
            decoded->type = LD;
            decoded->raddr_d = HL;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11110000 n", ib))
        { // read from internal RAM address to A
            decoded->type = LD;
            decoded->r_d = A;
            decoded->addr_16bit = 0xFF00 | ib[1];
            decoded->has_addr_16bit = 1;
        }
        else if (match_n("11100000 n", ib))
        { // write to internal RAM address from A
            decoded->type = LD;
            decoded->r_s = A;
            decoded->addr_16bit = 0xFF00 | ib[1];
            decoded->has_addr_16bit = 1;
        }
        else if (match_n("11111000 n", ib))
        {
            decoded->type = LDHL;
            decoded->r_d = HL;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11000110 n", ib))
        { // add 8-bit immediate to A
            decoded->type = ADD;
            decoded->r_d = A;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11001110 n", ib))
        { // add 8-bit immediate + CY to A
            decoded->type = ADC;
            decoded->r_d = A;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11010110 n", ib))
        { // subtract 8-bit immediate from A
            decoded->type = SUB;
            decoded->r_d = A;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11011110 n", ib))
        { // subtract (8-bit immediate + CY) from A
            decoded->type = SBC;
            decoded->r_d = A;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11100110 n", ib))
        { // do bitwise AND with 8-bit immediate, store in A
            decoded->type = AND;
            decoded->r_d = A;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11110110 n", ib))
        { // do bitwise OR with 8-bit immediate, store in A
            decoded->type = OR;
            decoded->r_d = A;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11101110 n", ib))
        { // do bitwise XOR with 8-bit immediate, store in A
            decoded->type = XOR;
            decoded->r_d = A;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11111110 n", ib))
        { // compare A with 8-bit immediate, set flag if they're equal
            decoded->type = CP;
            decoded->r_d = A;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11101000 n", ib))
        { // add 8-bit signed to SP and store in A
            // TODO: is the offset supposed to be signed?
            decoded->type = ADD;
            decoded->r_d = SP;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11001011 00000r", ib))
        { // rotate register to the left
            decoded->type = RLC;
            decoded->r_d = get_register(ib[1], 2);
        }
        else if (match_n("11001011 00000110", ib))
        { // rotate (HL) to the left
            decoded->type = RLC;
            decoded->raddr_d = HL;
        }
        else if (match_n("11001011 00010r", ib))
        { // rotate register to the left
            decoded->type = RL;
            decoded->r_d = get_register(ib[1], 2);
        }
        else if (match_n("11001011 00010110", ib))
        { // rotate (HL) to the left
            decoded->type = RL;
            decoded->raddr_d = HL;
        }
        else if (match_n("11001011 00001r", ib))
        { // rotate register to the right
            decoded->type = RRC;
            decoded->r_d = get_register(ib[1], 2);
        }
        else if (match_n("11001011 00001110", ib))
        { // rotate (HL) to the right
            decoded->type = RRC;
            decoded->raddr_d = HL;
        }
        else if (match_n("11001011 00011r", ib))
        { // rotate register to the right
            decoded->type = RR;
            decoded->r_d = get_register(ib[1], 2);
        }
        else if (match_n("11011011 00011110", ib))
        { // rotate (HL) to the right
            decoded->type = RR;
            decoded->raddr_d = HL;
        }
        else if (match_n("11001011 00100r", ib))
        { // shift the contents of the register to the left
            decoded->type = SLA;
            decoded->r_d = get_register(ib[1], 2);
        }
        else if (match_n("11001011 00100110", ib))
        { // shift (HL) to the left
            decoded->type = SLA;
            decoded->raddr_d = HL;
        }
        else if (match_n("11001011 00101r", ib))
        { // shift the contents of the register to the right
            decoded->type = SRA;
            decoded->r_d = get_register(ib[1], 2);
        }
        else if (match_n("11001011 00101110", ib))
        { // shift (HL) to the right
            decoded->type = SRA;
            decoded->raddr_d = HL;
        }
        else if (match_n("11001011 00111r", ib))
        { // shift register to the right, reset bit 7
            decoded->type = SRL;
            decoded->r_d = get_register(ib[1], 2);
        }
        else if (match_n("11001011 00111110", ib))
        { // shift (HL) to the right, reset bit 7
            decoded->type = SRL;
            decoded->raddr_d = HL;
        }
        else if (match_n("11001011 00110r", ib))
        { // swap the high order 4 bits with the low order 4 bits of a register
            decoded->type = SWAP;
            decoded->r_d = get_register(ib[1], 2);
        }
        else if (match_n("11001011 00110110", ib))
        { // swap the high order 4 bits with the low order 4 bits of (HL)
            decoded->type = SWAP;
            decoded->raddr_d = HL;
        }
        else if (match_n("11001011 01br", ib))
        { // copy the complement of the bit in r to the Z flag of the program status word
            decoded->type = BIT;
            decoded->r_d = get_register(ib[1], 2);
            decoded->lit_8bit = get_8bit(ib[1], 5);
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11001011 01b110", ib))
        { // copy the complement of the bit in (HL) to the Z flag of the program status word
            decoded->type = BIT;
            decoded->raddr_d = HL;
            decoded->lit_8bit = get_8bit(ib[1], 5);
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11001011 11br", ib))
        { // set to 1 the bit in specified register
            decoded->type = SET;
            decoded->r_d = get_register(ib[1], 2);
            decoded->lit_8bit = get_8bit(ib[1], 5);
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11001011 11b110", ib))
        { // set to 1 the bit in the specified memory
            decoded->type = SET;
            decoded->raddr_d = HL;
            decoded->lit_8bit = get_8bit(ib[1], 5);
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11001011 10br", ib))
        { // reset to 0 the specified bit in the register
            decoded->type = RES;
            decoded->r_d = get_register(ib[1], 2);
            decoded->lit_8bit = get_8bit(ib[1], 5);
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("11001011 10b110", ib))
        { // reset to 0 the bit in the specified memory
            decoded->type = RES;
            decoded->raddr_d = HL;
            decoded->lit_8bit = get_8bit(ib[1], 5);
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("00011000 n", ib))
        { // jump -126 to +129 steps from current PC
            decoded->type = JR;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
        }
        else if (match_n("001c000 n", ib))
        { // conditionally jump -127 to +129 steps from current PC
            decoded->type = JR;
            decoded->lit_8bit = ib[1];
            decoded->has_lit_8bit = 1;
            decoded->cond = get_cond(ib[0], 4);
        }
        else if (match_n("00010000 00000000", ib))
        {
            decoded->type = STOP;
        }
        else
        {

            // three byte instructions
            ilen++;

            if (match_n("11111010 n n", ib))
            { // read from 16-bit RAM address into A
                decoded->type = LD;
                decoded->r_d = A;
                decoded->addr_16bit = get_16bit(ib[2], ib[1]);
                decoded->has_addr_16bit = 1;
            }
            else if (match_n("11101010 n n", ib))
            { // write to 16-bit RAM address from A
                decoded->type = LD;
                decoded->r_s = A;
                decoded->addr_16bit = get_16bit(ib[2], ib[1]);
                decoded->has_addr_16bit = 1;
            }
            else if (match_n("00d0001 n n", ib))
            { // load immediate into register pair
                decoded->type = LD;
                decoded->r_d = get_register_pair_dd(ib[0], 5);
                decoded->lit_16bit = get_16bit(ib[2], ib[1]);
                decoded->has_lit_16bit = 1;
            }
            else if (match_n("00001000 n n", ib))
            { // write SP to address in RAM
                decoded->type = LD;
                decoded->r_s = SP;
                decoded->addr_16bit = get_16bit(ib[2], ib[1]);
                decoded->has_addr_16bit = 1;
            }
            else if (match_n("11000011 n n", ib))
            { // set the PC to the literal
                decoded->type = JP;
                decoded->lit_16bit = get_16bit(ib[2], ib[1]);
                decoded->has_lit_16bit = 1;
            }
            else if (match_n("110c010 n n", ib))
            { // set the PC based on the condition
                decoded->type = JP;
                decoded->cond = get_cond(ib[0], 4);
                decoded->lit_16bit = get_16bit(ib[2], ib[1]);
                decoded->has_lit_16bit = 1;
            }
            else if (match_n("11001101 n n", ib))
            {
                decoded->type = CALL;
                decoded->lit_16bit = get_16bit(ib[2], ib[1]);
                decoded->has_lit_16bit = 1;
            }
            else if (match_n("110c100 n n", ib))
            {
                decoded->type = CALL;
                decoded->cond = get_cond(ib[0], 4);
                decoded->lit_16bit = get_16bit(ib[2], ib[1]);
                decoded->has_lit_16bit = 1;
            }
            else
            {
                decoded->type = DB;
                decoded->lit_8bit = ib[0];
                decoded->has_lit_8bit = 1;
                return 1;
            }
        }
    }
    return ilen;
}