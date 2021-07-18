#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../inst.h"

char *map_cond_to_str[4] = {"NZ", "Z", "NC", "C"};
char *map_reg_to_str[8] = {"B", "C", "D", "E", "H", "L", "_err_", "A"};
char *map_dd_to_str[4] = {"BC", "DD", "HL", "SP"}; // TODO: should 'DD' be 'DE'?
char *map_qq_to_str[4] = {"BC", "DE", "HL", "AF"};
char *map_ss_to_str[4] = {"BC", "DE", "HL", "SP"};


// TODO: indicate it's private
struct inst instructions[] = {
  {LD, 1, 1, "01{r}{r}", "LD {r}, {r}"},
  {LD, 2, 2, "00{r}110 {n}", "LD {r}, {n}"},
  {LD, 1, 2, "01{r}110", "LD {r}, (HL)"},
  {LD, 1, 2, "01110{r}", "LD (HL), {r}"},
  {LD, 2, 3, "00110110 {n}", "LD (HL), {n}"},
  {LD, 1, 2, "00001010", "LD A, (BC)"},
  {LD, 1, 2, "00011010", "LD A, (DE)"},
  {LD, 1, 2, "11110010", "LD A, (C)"},
  {LD, 1, 2, "11100010", "LD (C), A"},
  {LD, 2, 3, "11110000 {n}", "LD A, ({n})"},
  {LD, 2, 3, "11100000 {n}", "LD ({n}), A"},
  {LD, 3, 4, "11111010 {nn}", "LD A, ({nn})"},
  {LD, 3, 4, "11101010 {nn}", "LD ({nn}), A"},
  {LD, 1, 2, "00101010", "LD A, (HLI)"},
  {LD, 1, 2, "00111010", "LD A, (HLD)"},
  {LD, 1, 2, "00000010", "LD (BC), A"},
  {LD, 1, 2, "00010010", "LD (DE), A"},
  {LD, 1, 2, "00100010", "LD (HLI), A"},
  {LD, 1, 2, "00110010", "LD (HLD), A"},
  {LD, 3, 3, "00{dd}0001 {nn}", "LD {dd}, {nn}"},
  {LD, 1, 2, "11111001", "LD SP, HL"},
  {LD, 3, 5, "00001000 {nn}", "LD ({nn}), SP"},
  {PUSH, 1, 4, "11{qq}0101", "PUSH {qq}"},
  {POP, 1, 3, "11{qq}0001", "POP {qq}"},
  {LDHL, 2, 3, "11111000 {e}", "LDHL SP, {e}"},

  {ADD, 1, 1, "10000{r}", "ADD A, {r}"},
  {ADD, 2, 2, "11000110 {n}", "ADD A, {n}"},
  {ADD, 1, 2, "10000110", "ADD A, (HL)"},

  {ADC, 1, 1, "10001{r}", "ADC A, {r}"},
  {ADC, 2, 2, "11001110 {n}", "ADC A, {n}"},
  {ADC, 1, 2, "10001110", "ADC A, (HL)"},
  {SUB, 1, 1, "10010{r}", "SUB {r}"},
  {SUB, 2, 2, "11010110 {n}", "SUB {n}"},
  {SUB, 1, 2, "10010110", "SUB (HL)"},
  {SBC, 1, 1, "10011{r}", "SBC A, {r}"},
  {SBC, 2, 2, "11011110 {n}", "SBC A, {n}"},
  {SBC, 1, 2, "10011110", "SBC A, (HL)"},
  {AND, 1, 1, "10100{r}", "AND {r}"},
  {AND, 2, 2, "11100110 {n}", "AND {n}"},
  {AND, 1, 2, "10100110", "AND (HL)"},
  {OR, 1, 1, "10110{r}", "OR {r}"},
  {OR, 2, 2, "11110110 {n}", "OR {n}"},
  {OR, 1, 2, "10110110", "OR (HL)"},
  {CP, 1, 1, "10111{r}", "CP {r}"},
  {CP, 2, 2, "11111110 {n}", "CP {n}"},
  {CP, 1, 2, "10111110", "CP (HL)"},
  {INC, 1, 1, "00{r}100", "INC {r}"},
  {INC, 1, 3, "00110100", "INC (HL)"},
  {DEC, 1, 1, "00{r}101", "DEC {r}"},
  {DEC, 1, 3, "00110101", "DEC (HL)"},
  {AND, 1, 2, "00{ss}1001", "ADD HL, {ss}"},
  {ADD, 2, 4, "11101000 {e}", "ADD SP, {e}"},
  {INC, 1, 2, "00{ss}0011", "INC {ss}"},
  {DEC, 1, 2, "00{ss}1011", "DEC {ss}"},
  {RLCA, 1, 1, "00000111", "RLCA"},
  {RLA, 1, 1, "00010111", "RLA"},
  {RRCA, 1, 1, "00001111", "RRCA"},
  {RRA, 1, 1, "00011111", "RRA"},
  {RLC, 2, 2, "11001011 00000{r}", "RLC {r}"},
  {RLC, 2, 4, "11001011 00000110", "RLC (HL)"},
  {RL, 2, 2, "11001011 00010{r}", "RL {r}"},
  {RL, 2, 4, "11011011 00010110", "RL (HL)"},
  {RRC, 2, 2, "11001011 00001{r}", "RRC {r}"},
  {RRC, 2, 4, "11001011 00001110", "RRC (HL)"},
  {RR, 2, 2, "11001011 00011{r}", "RR {r}"},
  {RR, 2, 4, "11011011 00011110", "RR (HL)"},
  {SLA, 2, 2, "11001011 00100{r}", "SLA {r}"},
  {SLA, 2, 4, "11011011 00100110", "SLA (HL)"},
  {SRA, 2, 2, "11001011 00101{r}", "SRA {r}"},
  {SRA, 2, 4, "11001011 00101110", "SRA (HL)"},
  {SRL, 2, 2, "11001011 00111{r}", "SRL {r}"},
  {SRL, 2, 4, "11001011 00111110", "SRL (HL)"},
  {SWAP, 2, 2, "11001011 00110{r}", "SWAP {r}"},
  {SWAP, 2, 4, "11001011 00110110", "SWAP (HL)"},
  {BIT, 2, 2, "11001011 01{b}{r}", "BIT {b}, {r}"},
  {BIT, 2, 3, "11001011 01{b}110", "BIT {b}, (HL)"},
  {SET, 2, 2, "11001011 11{b}{r}", "SET {b}, {r}"},
  {SET, 2, 4, "11001011 11{b}110", "SET {b}, (HL)"},
  {RES, 2, 2, "11001011 10{b}{r}", "RES {b}, {r}"},
  {RES, 2, 4, "11001011 10{b}110", "RES {b}, (HL)"},
  {JP, 3, 4, "11000011 {nn}", "JP {nn}"},
  {JP, 3, 3, "110{cc}010 {nn}", "JP {cc}, {nn}"}, // TODO: handle the 4-cycle variant
  {JR, 2, 3, "00011000 {e}", "JR {e}"},
  {JR, 2, 2, "001{cc}000 {e}", "JR {cc}, {e}"},  // TODO: handle the 3-cycle variant
  {JP, 1, 1, "11101001", "JP (HL)"},
  {CALL, 3, 6, "11001101 {nn}", "CALL {nn}"},
  {CALL, 3, 3, "110{cc}100 {nn}", "CALL {cc}, {nn}"}, // TODO: handle the 6-cycle variat
  {RET, 1, 4, "11001001", "RET"},
  {RETI, 1, 4, "11011001", "RETI"},
  {RET, 1, 2, "110{cc}000", "RET {cc}"}, // TODO: handle the 5-cycle variant
  {RST, 1, 4, "11{t}111", "RST {t}"},
  {DAA, 1, 1, "00100111", "DAA"},
  {CPL, 1, 1, "00101111", "CPL"},
  {NOP, 1, 1, "00000000", "NOP"},
  {XOR, 1, 1, "10101{r}", "XOR {r}"},
  {XOR, 2, 2, "11101110 {n}", "XOR {n}" },
  {XOR, 1, 2, "10101110", "XOR (HL)"},
  {HALT, 1, 1, "01110110", "HALT"},
  {STOP, 2, 1, "00010000 00000000", "STOP"},
  {DI, 1, 1, "11110011", "DI"},
  {EI, 1, 1, "11111011", "EI"},
  {SCF, 1, 1, "00110111", "SCF"},
};

void inst_add_arg(struct inst * inst, struct inst_arg arg) {
  inst->args[inst->args_count++] = arg;
}

void inst_clear_args(struct inst *inst) {
  inst->args_count=0;
}

// Returns true iff the instruction byte matches the bit pattern.
// If true, the instruction is filled with parsed args.
int _match_bit_pattern(struct inst* inst, char *bytes)
{
    int shift = 0;
    int c = 0;
    char * pattern = inst->bit_pattern;
    char ch = bytes[c++];
    while (1)
    {
        // handle boundary conditions
        if (*pattern == ' ' || *pattern == '\0')
        {
            while (*pattern == ' ')
                pattern++;

            if (*pattern == '\0')
	      return (shift % 8) == 0;
            if (shift != 8)
	      return 0;
            shift = 0;
            ch = bytes[c++];
            continue;
        }

        // attempt match
        char si = ch << shift;
	char arg[3];
	int arglen = 0;
        switch (pattern[0])
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
	case '{':
	  while (pattern[arglen+1] != '}') {
	    if (pattern[arglen] == ' ' || pattern[arglen] == '\n') {
	      return 0;
	    }
	    arglen++;
	  }
	  strncpy(arg, pattern+1, arglen);
	  arg[arglen] = '\0';
	  pattern += arglen+1;

	  if (strcmp(arg, "r") == 0) {
 	    uint8_t byte = (0b11100000 & si) >> 5;
	    if (byte == 6) // used in another instr to indicate (HL)
	      return 0;
	    struct inst_arg arg = { .type = R, .value.byte = byte };
	    inst_add_arg(inst, arg);
	    shift += 3;
	  } else  if (strcmp(arg, "b") == 0) {
	    uint8_t byte = (0b11100000 & si) >> 5;
	    struct inst_arg arg = { .type = B, .value.byte = byte };
	    inst_add_arg(inst, arg);
	    shift += 3;
	  } else  if (strcmp(arg, "t") == 0) {
	    uint8_t byte = (0b11100000 & si) >> 5;
	    struct inst_arg arg = { .type = T, .value.byte = byte };
	    inst_add_arg(inst, arg);
	    shift += 3;
	  } else if (strcmp(arg, "cc") == 0) {
	    uint8_t byte = (0b11000000 & si) >> 6;
	    struct inst_arg arg = { .type = CC, .value.byte = byte };
	    inst_add_arg(inst, arg);
	    shift += 2;
	  } else if (strcmp(arg, "dd") == 0) {
	    uint8_t byte = (0b11000000 & si) >> 6;
	    struct inst_arg arg = { .type = DD, .value.byte = byte };
	    inst_add_arg(inst, arg);
	    shift += 2;
	  } else if (strcmp(arg, "qq") == 0) {
	    uint8_t byte = (0b11000000 & si) >> 6;
	    struct inst_arg arg = { .type = QQ, .value.byte = byte };
	    inst_add_arg(inst, arg);
	    shift += 2;
	  } else if (strcmp(arg, "ss") == 0) {
	    uint8_t byte = (0b11000000 & si) >> 6;
	    struct inst_arg arg = { .type = SS, .value.byte = byte };
	    inst_add_arg(inst, arg);
	    shift += 2;
	  } else if (strcmp(arg, "e") == 0) {
	    uint8_t byte = si;
	    struct inst_arg arg = { .type = E, .value.byte = byte };
	    inst_add_arg(inst, arg);
	    shift += 8;
	  } else if (strcmp(arg, "n") == 0) {
	    uint8_t byte = si;
	    struct inst_arg arg = { .type = N, .value.byte = byte };
	    inst_add_arg(inst, arg);
	    shift += 8;
	  } else if (strcmp(arg, "nn") == 0) {
	    uint8_t lower = si;
	    uint8_t upper = bytes[c++];
	    struct inst_arg arg = { .type = NN, .value.word = {lower, upper}};
	    inst_add_arg(inst, arg);
	    shift += 8;
	  } else {
	    fprintf(stderr, "Error decoding instruction: argument unknown: %s\n", arg);
	    return 0;
	  }
	  break;
        default:
	  inst_clear_args(inst);
	  return 0;
        }
        pattern++;
    }

    // should never occur
    return 0;
}

int init_inst_from_bytes(struct inst* inst, char *bytes) {
  for (int i = 0; i < sizeof(instructions) / sizeof(struct inst); i++) {
    struct inst root_inst = instructions[i];
    *inst = root_inst;
    if (_match_bit_pattern(inst, bytes))
      return 1;
  }
  return 0;
}


int arg_to_str(struct inst_arg *arg, char *buf) {
  char *arg_str;
  char ebuf[8];
  int16_t evalue;
  switch (arg->type) {
  case R:
    arg_str = map_reg_to_str[arg->value.byte];
    strcpy(buf, arg_str);
    return strlen(arg_str);
  case B:
  case T:
    sprintf(buf, "%d", arg->value.byte);
    return 1;
  case CC:
    arg_str = map_cond_to_str[arg->value.byte];
    strcpy(buf, arg_str);
    return strlen(arg_str);
  case DD:
    arg_str = map_dd_to_str[arg->value.byte];
    strcpy(buf, arg_str);
    return 2;
  case QQ:
    arg_str = map_qq_to_str[arg->value.byte];
    strcpy(buf, arg_str);
    return 2;
  case SS:
    arg_str = map_ss_to_str[arg->value.byte];
    strcpy(buf, arg_str);
    return 2;
  case E:
    evalue = (int8_t)arg->value.byte;
    evalue+=2;
    sprintf(ebuf, "%d", evalue);
    strcpy(buf, ebuf);
    return strlen(ebuf);
  case N:
    sprintf(buf, "0x%02X", arg->value.byte);
    return 4;
    break;
  case NN:
    sprintf(buf, "0x%02X%02X", arg->value.word[1], arg->value.word[0]);
    return 6;
  default:
    // should never occur
    strcpy(buf, "_err2_");
    return 5;
  }
}


int inst_to_str(struct inst * inst, char *buf) {
  int len = 0;
  char *pattern = inst->txt_pattern;
  struct inst_arg *current_arg = inst->args;
  while (*pattern != '\0') {
    if (*pattern == '{') {
      pattern++;
      len += arg_to_str(current_arg++, buf+len);
      while (*pattern++ != '}')
	;
    } else {
      buf[len++] = *pattern++;
    }
  }
  buf[len++] = '\0';
  return len;
}
