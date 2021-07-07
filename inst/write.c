#include <stdio.h>
#include "../inst.h"

char *reg_str(enum reg r, int inc, int dec)
{
    switch (r)
    {
    case A:
        return "A";
    case B:
        return "B";
    case C:
        return "C";
    case D:
        return "D";
    case E:
        return "E";
    case H:
        return "H";
    case L:
        return "L";
    case AF:
        return "AF";
    case BC:
        return "BC";
    case DE:
        return "DE";
    case HL:
        if (inc)
        {
            return "HLI";
        }
        if (dec)
        {
            return "HLD";
        }
        return "HL";
    case PC:
        return "PC";
    case SP:
        return "SP";
    default:
        return "_err_";
    }
}

char *cond_str(enum cond c)
{
    switch (c)
    {
    case NZ:
        return "NZ";
    case Z:
        return "Z";
    case NC:
        return "NC";
    case YC:
        return "C";
    default:
        return "_err_";
    }
}

// TODO: combine logic of dest_str with src_str
void dest_str(struct inst *ins, char *out)
{
    if (ins->r_d)
    {
        sprintf(out, "%s", reg_str(ins->r_d, ins->incr, ins->decr));
    }
    else if (ins->raddr_d)
    {
        sprintf(out, "(%s)", reg_str(ins->raddr_d, ins->incr, ins->decr));
    }
    else if (ins->has_lit_16bit)
    {
        sprintf(out, "0x%04X", ins->lit_16bit & 0xFFFF);
    }
    else if (ins->has_addr_16bit)
    {
        sprintf(out, "(0x%04X)", ins->addr_16bit & 0xFFFF);
    }
    else if (ins->has_lit_8bit)
    {
        sprintf(out, "0x%02X", ins->lit_8bit & 0xFF);
    }
    else
    {
        sprintf(out, "_err_");
    }
}

void src_str(struct inst *ins, char *out)
{
    if (ins->r_s)
    {
        sprintf(out, "%s", reg_str(ins->r_s, ins->incr, ins->decr));
    }
    else if (ins->raddr_s)
    {
        sprintf(out, "(%s)", reg_str(ins->raddr_s, ins->incr, ins->decr));
    }
    else if (ins->has_lit_16bit)
    {
        sprintf(out, "0x%04X", ins->lit_16bit & 0xFFFF);
    }
    else if (ins->has_addr_16bit)
    {
        sprintf(out, "(0x%04X)", ins->addr_16bit & 0xFFFF);
    }
    else if (ins->has_lit_8bit)
    {
        sprintf(out, "0x%02X", ins->lit_8bit & 0xFF);
    }
    else
    {
        sprintf(out, "_err_");
    }
}

void accum_arg_str(struct inst *ins, char *out)
{
    if (ins->r_s)
    {
        sprintf(out, "%s", reg_str(ins->r_s, ins->incr, ins->decr));
    }
    else if (ins->raddr_s)
    {
        sprintf(out, "(%s)", reg_str(ins->raddr_s, ins->incr, ins->decr));
    }
    else if (ins->has_lit_8bit)
    {
        sprintf(out, "0x%02X", ins->lit_8bit & 0xFF);
    }
    else
    {
        sprintf(out, "_err_");
    }
}

int inst_write(struct inst *ins, char *out)
{
    char buf1[16];
    char buf2[16];
    int offset;
    switch (ins->type)
    {
    case ADC:
        accum_arg_str(ins, buf1);
        sprintf(out, "ADC %s", buf1);
        break;
    case ADD:
        accum_arg_str(ins, buf1);
        sprintf(out, "ADD %s,%s", reg_str(ins->r_d, 0, 0), buf1);
        break;
    case AND:
        accum_arg_str(ins, buf1);
        sprintf(out, "AND %s", buf1);
        break;
    case BIT:
        dest_str(ins, buf1);
        sprintf(out, "BIT %d,%s", ins->lit_8bit, buf1);
        break;
    case CALL:
        if (ins->cond)
        {
            sprintf(out, "CALL %s,0x%04X", cond_str(ins->cond), ins->lit_16bit & 0xFFFF);
        }
        else
        {
            sprintf(out, "CALL 0x%04X", ins->lit_16bit & 0xFFFF);
        }
        break;
    case CCF:
        sprintf(out, "CCF");
        break;
    case CP:
        accum_arg_str(ins, buf1);
        sprintf(out, "CP %s", buf1);
        break;
    case CPL:
        sprintf(out, "CPL");
        break;
    case DAA:
        sprintf(out, "DAA");
        break;
    case DB:
        sprintf(out, "DB 0x%02X", ins->lit_8bit & 0xFF);
        break;
    case DEC:
        dest_str(ins, buf1);
        sprintf(out, "DEC %s", buf1);
        break;
    case DI:
        sprintf(out, "DI");
        break;
    case EI:
        sprintf(out, "EI");
        break;
    case HALT:
        sprintf(out, "HALT");
        break;
    case INC:
        dest_str(ins, buf1);
        sprintf(out, "INC %s", buf1);
        break;
    case JP:
        if (ins->cond)
        {
            sprintf(out, "JP %s,0x%04X", cond_str(ins->cond), ins->lit_16bit & 0xFFFF);
        }
        else if (ins->r_s == HL)
        {
            sprintf(out, "JP HL");
        }
        else
        {
            sprintf(out, "JP 0x%04X", ins->lit_16bit & 0xFFFF);
        }
        break;
    case JR:
        offset = ins->lit_8bit;
        offset += 2;
        if (ins->cond)
        {
            sprintf(out, "JR %s,%d", cond_str(ins->cond), offset);
        }
        else
        {
            sprintf(out, "JR %d", offset);
        }
        break;
    case LD:
        dest_str(ins, buf1);
        src_str(ins, buf2);
        sprintf(out, "LD %s,%s", buf1, buf2);
        break;
    case LDHL:
        sprintf(out, "LDHL SP,%d", ins->lit_8bit);
        break;
    case NOP:
        sprintf(out, "NOP");
        break;
    case OR:
        accum_arg_str(ins, buf1);
        sprintf(out, "OR %s", buf1);
        break;
    case POP:
        sprintf(out, "POP %s", reg_str(ins->r_d, ins->incr, ins->decr));
        break;
    case PUSH:
        sprintf(out, "PUSH %s", reg_str(ins->r_s, ins->incr, ins->decr));
        break;
    case RES:
        dest_str(ins, buf1);
        sprintf(out, "RES %d,%s", ins->lit_8bit, buf1);
        break;
    case RET:
        if (ins->cond)
        {
            sprintf(out, "RET %s", cond_str(ins->cond));
        }
        else
        {
            sprintf(out, "RET");
        }
        break;
    case RETI:
        sprintf(out, "RETI");
        break;
    case RL:
        dest_str(ins, buf1);
        sprintf(out, "RL %s", buf1);
        break;
    case RLA:
        sprintf(out, "RLA");
        break;
    case RLC:
        dest_str(ins, buf1);
        sprintf(out, "RLC %s", buf1);
        break;
    case RLCA:
        sprintf(out, "RLCA");
        break;
    case RR:
        dest_str(ins, buf1);
        sprintf(out, "RR %s", buf1);
        break;
    case RRA:
        sprintf(out, "RRA");
        break;
    case RRC:
        dest_str(ins, buf1);
        sprintf(out, "RRC %s", buf1);
        break;
    case RRCA:
        sprintf(out, "RRCA");
        break;
    case RST:
        sprintf(out, "RST %d", ins->lit_8bit);
        break;
    case SBC:
        accum_arg_str(ins, buf1);
        sprintf(out, "SBC %s", buf1);
        break;
    case SCF:
        sprintf(out, "SCF");
        break;
    case SET:
        if (ins->raddr_d == HL)
        {
            sprintf(out, "SET %d,(HL)", ins->lit_8bit);
        }
        else
        {
            sprintf(out, "SET %d,%s", ins->lit_8bit, reg_str(ins->r_d, 0, 0));
        }
        break;
    case SLA:
        dest_str(ins, buf1);
        sprintf(out, "SLA %s", buf1);
        break;
    case SRA:
        dest_str(ins, buf1);
        sprintf(out, "SRA %s", buf1);
        break;
    case SRL:
        dest_str(ins, buf1);
        sprintf(out, "SRL %s", buf1);
        break;
    case STOP:
        sprintf(out, "STOP");
        break;
    case SUB:
        accum_arg_str(ins, buf1);
        sprintf(out, "SUB %s", buf1);
        break;
    case SWAP:
        dest_str(ins, buf1);
        sprintf(out, "SWAP %s", buf1);
        break;
    case XOR:
        accum_arg_str(ins, buf1);
        sprintf(out, "XOR %s", buf1);
        break;
    default:
        sprintf(out, "DEBUG -> %d", ins->type);
        return 0;
    }

    return 1;
}