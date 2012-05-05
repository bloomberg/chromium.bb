/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/include/elf64.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/decoder.h"

/* This is a copy of NaClLog_Function from shared/platform/nacl_log.c to avoid
 * linking in code in NaCl shared code in the unreviewed/Makefile and be able to
 *  use CHECK().

 * TODO(khim): remove the copy of NaClLog_Function implementation as soon as
 * unreviewed/Makefile is eliminated.
 */
void NaClLog_Function(int detail_level, char const  *fmt, ...) {
  va_list ap;

  UNREFERENCED_PARAMETER(detail_level);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  exit(1);
}

static void CheckBounds(unsigned char *data, size_t data_size,
                        void *ptr, size_t inside_size) {
  CHECK(data <= (unsigned char *) ptr);
  CHECK((unsigned char *) ptr + inside_size <= data + data_size);
}

void ReadImage(const char *filename, uint8_t **result, size_t *result_size) {
  FILE *fp;
  uint8_t *data;
  size_t file_size;
  size_t got;

  fp = fopen(filename, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open input file: %s\n", filename);
    exit(1);
  }
  /* Find the file size. */
  fseek(fp, 0, SEEK_END);
  file_size = ftell(fp);
  data = malloc(file_size);
  if (data == NULL) {
    fprintf(stderr, "Unable to create memory image of input file: %s\n",
            filename);
    exit(1);
  }
  fseek(fp, 0, SEEK_SET);
  got = fread(data, 1, file_size, fp);
  if (got != file_size) {
    fprintf(stderr, "Unable to read data from input file: %s\n",
            filename);
    exit(1);
  }
  fclose(fp);

  *result = data;
  *result_size = file_size;
}

struct DecodeState {
  uint8_t width;
  const uint8_t *fwait; /* Set to true if fwait is detetected. */
  const uint8_t *offset;
  int ia32_mode;
};

void ProcessInstruction(const uint8_t *begin, const uint8_t *end,
                        struct instruction *instruction, void *userdata) {
  const char *instruction_name;
  unsigned char operands_count;
  const uint8_t *p;
  char delimeter = ' ';
  int print_rip = FALSE;
  int rex_bits = 0;
  int maybe_rex_bits = 0;
  int show_name_suffix = FALSE;
  int empty_rex_prefix_ok = FALSE;
#define print_name(x) (printf((x)), shown_name += strlen((x)))
  size_t shown_name = 0;
  int i, operand_type;

  /* "fwait" is nasty: any number of them will be included in other X87
     instructions ("fclex", "finit", "fstcw", "fstsw", "fsave" have two
     names, other instructions are unchanged) - but if after them we see
     regular instruction then we must print all them.  This convoluted
     logic is not needed when we  don't print anything so decoder does
     not include it.  */
  if ((end == begin + 1) && (begin[0] == 0x9b)) {
    if (!(((struct DecodeState *)userdata)->fwait)) {
      ((struct DecodeState *)userdata)->fwait = begin;
    }
    return;
  } else if (((struct DecodeState *)userdata)->fwait) {
    if ((begin[0] < 0xd8) || (begin[0] > 0xdf)) {
      while ((((struct DecodeState *)userdata)->fwait) < begin) {
        printf("%*lx:\t                   \tfwait\n",
          ((struct DecodeState *)userdata)->width,
          (long)((((struct DecodeState *)userdata)->fwait++) -
                                   (((struct DecodeState *)userdata)->offset)));
      }
    } else {
      begin = ((struct DecodeState *)userdata)->fwait;
    }
    ((struct DecodeState *)userdata)->fwait = FALSE;
  }
  printf("%*lx:\t", ((struct DecodeState *)userdata)->width,
                    (long)(begin - (((struct DecodeState *)userdata)->offset)));
  for (p = begin; p < begin + 7; ++p) {
    if (p >= end) {
      printf("   ");
    } else {
      printf("%02x ", *p);
    }
  }
  printf("\t");
  instruction_name = instruction->name;
  operands_count = instruction->operands_count;
  /* "cmppd" has two-operand mnemonic names for "imm8" equal to 0x0, ...0x7. */
  if (!strcmp(instruction_name, "cmppd")) {
    if (instruction->imm[0] < 0x08) {
      switch (instruction->imm[0]) {
        case 0x00: instruction_name = "cmpeqpd"; break;
        case 0x01: instruction_name = "cmpltpd"; break;
        case 0x02: instruction_name = "cmplepd"; break;
        case 0x03: instruction_name = "cmpunordpd"; break;
        case 0x04: instruction_name = "cmpneqpd"; break;
        case 0x05: instruction_name = "cmpnltpd"; break;
        case 0x06: instruction_name = "cmpnlepd"; break;
        case 0x07: instruction_name = "cmpordpd"; break;
      }
      --operands_count;
    }
  }
  /* "vcmppd" has two-operand mnemonic names for "imm8"
   * equal to 0x0, ... 0x1f. */
  if (!strcmp(instruction_name, "vcmppd")) {
    if (instruction->imm[0] < 0x20) {
      switch (instruction->imm[0]) {
        case 0x00: instruction_name = "vcmpeqpd"; break;
        case 0x01: instruction_name = "vcmpltpd"; break;
        case 0x02: instruction_name = "vcmplepd"; break;
        case 0x03: instruction_name = "vcmpunordpd"; break;
        case 0x04: instruction_name = "vcmpneqpd"; break;
        case 0x05: instruction_name = "vcmpnltpd"; break;
        case 0x06: instruction_name = "vcmpnlepd"; break;
        case 0x07: instruction_name = "vcmpordpd"; break;
        case 0x08: instruction_name = "vcmpeq_uqpd"; break;
        case 0x09: instruction_name = "vcmpngepd"; break;
        case 0x0a: instruction_name = "vcmpngtpd"; break;
        case 0x0b: instruction_name = "vcmpfalsepd"; break;
        case 0x0c: instruction_name = "vcmpneq_oqpd"; break;
        case 0x0d: instruction_name = "vcmpgepd"; break;
        case 0x0e: instruction_name = "vcmpgtpd"; break;
        case 0x0f: instruction_name = "vcmptruepd"; break;
        case 0x10: instruction_name = "vcmpeq_ospd"; break;
        case 0x11: instruction_name = "vcmplt_oqpd"; break;
        case 0x12: instruction_name = "vcmple_oqpd"; break;
        case 0x13: instruction_name = "vcmpunord_spd"; break;
        case 0x14: instruction_name = "vcmpneq_uspd"; break;
        case 0x15: instruction_name = "vcmpnlt_uqpd"; break;
        case 0x16: instruction_name = "vcmpnle_uqpd"; break;
        case 0x17: instruction_name = "vcmpord_spd"; break;
        case 0x18: instruction_name = "vcmpeq_uspd"; break;
        case 0x19: instruction_name = "vcmpnge_uqpd"; break;
        case 0x1a: instruction_name = "vcmpngt_uqpd"; break;
        case 0x1b: instruction_name = "vcmpfalse_ospd"; break;
        case 0x1c: instruction_name = "vcmpneq_ospd"; break;
        case 0x1d: instruction_name = "vcmpge_oqpd"; break;
        case 0x1e: instruction_name = "vcmpgt_oqpd"; break;
        case 0x1f: instruction_name = "vcmptrue_uspd"; break;
      }
      --operands_count;
    }
  }
  /* "cmpps" has two-operand mnemonic names for "imm8" equal to 0x0, ... 0x7. */
  if (!strcmp(instruction_name, "cmpps")) {
    if (instruction->imm[0] < 0x08) {
      switch (instruction->imm[0]) {
        case 0x00: instruction_name = "cmpeqps"; break;
        case 0x01: instruction_name = "cmpltps"; break;
        case 0x02: instruction_name = "cmpleps"; break;
        case 0x03: instruction_name = "cmpunordps"; break;
        case 0x04: instruction_name = "cmpneqps"; break;
        case 0x05: instruction_name = "cmpnltps"; break;
        case 0x06: instruction_name = "cmpnleps"; break;
        case 0x07: instruction_name = "cmpordps"; break;
      }
      --operands_count;
    }
  }
  /* "vcmpps" has two-operand mnemonic names for "imm8" equal to 0x0, ...0x1f. */
  if (!strcmp(instruction_name, "vcmpps")) {
    if (instruction->imm[0] < 0x20) {
      switch (instruction->imm[0]) {
        case 0x00: instruction_name = "vcmpeqps"; break;
        case 0x01: instruction_name = "vcmpltps"; break;
        case 0x02: instruction_name = "vcmpleps"; break;
        case 0x03: instruction_name = "vcmpunordps"; break;
        case 0x04: instruction_name = "vcmpneqps"; break;
        case 0x05: instruction_name = "vcmpnltps"; break;
        case 0x06: instruction_name = "vcmpnleps"; break;
        case 0x07: instruction_name = "vcmpordps"; break;
        case 0x08: instruction_name = "vcmpeq_uqps"; break;
        case 0x09: instruction_name = "vcmpngeps"; break;
        case 0x0a: instruction_name = "vcmpngtps"; break;
        case 0x0b: instruction_name = "vcmpfalseps"; break;
        case 0x0c: instruction_name = "vcmpneq_oqps"; break;
        case 0x0d: instruction_name = "vcmpgeps"; break;
        case 0x0e: instruction_name = "vcmpgtps"; break;
        case 0x0f: instruction_name = "vcmptrueps"; break;
        case 0x10: instruction_name = "vcmpeq_osps"; break;
        case 0x11: instruction_name = "vcmplt_oqps"; break;
        case 0x12: instruction_name = "vcmple_oqps"; break;
        case 0x13: instruction_name = "vcmpunord_sps"; break;
        case 0x14: instruction_name = "vcmpneq_usps"; break;
        case 0x15: instruction_name = "vcmpnlt_uqps"; break;
        case 0x16: instruction_name = "vcmpnle_uqps"; break;
        case 0x17: instruction_name = "vcmpord_sps"; break;
        case 0x18: instruction_name = "vcmpeq_usps"; break;
        case 0x19: instruction_name = "vcmpnge_uqps"; break;
        case 0x1a: instruction_name = "vcmpngt_uqps"; break;
        case 0x1b: instruction_name = "vcmpfalse_osps"; break;
        case 0x1c: instruction_name = "vcmpneq_osps"; break;
        case 0x1d: instruction_name = "vcmpge_oqps"; break;
        case 0x1e: instruction_name = "vcmpgt_oqps"; break;
        case 0x1f: instruction_name = "vcmptrue_usps"; break;
      }
      --operands_count;
    }
  }
  /* "cmpsd" has two-operand mnemonic names for "imm8" equal to 0x0, ...0x7. */
  if (!strcmp(instruction_name, "cmpsd")) {
    if (instruction->imm[0] < 0x08) {
      switch (instruction->imm[0]) {
        case 0x00: instruction_name = "cmpeqsd"; break;
        case 0x01: instruction_name = "cmpltsd"; break;
        case 0x02: instruction_name = "cmplesd"; break;
        case 0x03: instruction_name = "cmpunordsd"; break;
        case 0x04: instruction_name = "cmpneqsd"; break;
        case 0x05: instruction_name = "cmpnltsd"; break;
        case 0x06: instruction_name = "cmpnlesd"; break;
        case 0x07: instruction_name = "cmpordsd"; break;
      }
      --operands_count;
    }
  }
  /* "vcmpsd" has two-operand mnemonic names for "imm8" equal to 0x0, ...0x1f. */
  if (!strcmp(instruction_name, "vcmpsd")) {
    if (instruction->imm[0] < 0x20) {
      switch (instruction->imm[0]) {
        case 0x00: instruction_name = "vcmpeqsd"; break;
        case 0x01: instruction_name = "vcmpltsd"; break;
        case 0x02: instruction_name = "vcmplesd"; break;
        case 0x03: instruction_name = "vcmpunordsd"; break;
        case 0x04: instruction_name = "vcmpneqsd"; break;
        case 0x05: instruction_name = "vcmpnltsd"; break;
        case 0x06: instruction_name = "vcmpnlesd"; break;
        case 0x07: instruction_name = "vcmpordsd"; break;
        case 0x08: instruction_name = "vcmpeq_uqsd"; break;
        case 0x09: instruction_name = "vcmpngesd"; break;
        case 0x0a: instruction_name = "vcmpngtsd"; break;
        case 0x0b: instruction_name = "vcmpfalsesd"; break;
        case 0x0c: instruction_name = "vcmpneq_oqsd"; break;
        case 0x0d: instruction_name = "vcmpgesd"; break;
        case 0x0e: instruction_name = "vcmpgtsd"; break;
        case 0x0f: instruction_name = "vcmptruesd"; break;
        case 0x10: instruction_name = "vcmpeq_ossd"; break;
        case 0x11: instruction_name = "vcmplt_oqsd"; break;
        case 0x12: instruction_name = "vcmple_oqsd"; break;
        case 0x13: instruction_name = "vcmpunord_ssd"; break;
        case 0x14: instruction_name = "vcmpneq_ussd"; break;
        case 0x15: instruction_name = "vcmpnlt_uqsd"; break;
        case 0x16: instruction_name = "vcmpnle_uqsd"; break;
        case 0x17: instruction_name = "vcmpord_ssd"; break;
        case 0x18: instruction_name = "vcmpeq_ussd"; break;
        case 0x19: instruction_name = "vcmpnge_uqsd"; break;
        case 0x1a: instruction_name = "vcmpngt_uqsd"; break;
        case 0x1b: instruction_name = "vcmpfalse_ossd"; break;
        case 0x1c: instruction_name = "vcmpneq_ossd"; break;
        case 0x1d: instruction_name = "vcmpge_oqsd"; break;
        case 0x1e: instruction_name = "vcmpgt_oqsd"; break;
        case 0x1f: instruction_name = "vcmptrue_ussd"; break;
      }
      --operands_count;
    }
  }
  /* "cmpss" has two-operand mnemonic names for "imm8" equal to 0x0, ... 0x7. */
  if (!strcmp(instruction_name, "cmpss")) {
    if (instruction->imm[0] < 0x08) {
      switch (instruction->imm[0]) {
        case 0x00: instruction_name = "cmpeqss"; break;
        case 0x01: instruction_name = "cmpltss"; break;
        case 0x02: instruction_name = "cmpless"; break;
        case 0x03: instruction_name = "cmpunordss"; break;
        case 0x04: instruction_name = "cmpneqss"; break;
        case 0x05: instruction_name = "cmpnltss"; break;
        case 0x06: instruction_name = "cmpnless"; break;
        case 0x07: instruction_name = "cmpordss"; break;
      }
      --operands_count;
    }
  }
  /* "vcmpss" has two-operand mnemonic names for "imm8" equal to 0x0, ...0x1f. */
  if (!strcmp(instruction_name, "vcmpss")) {
    if (instruction->imm[0] < 0x20) {
      switch (instruction->imm[0]) {
        case 0x00: instruction_name = "vcmpeqss"; break;
        case 0x01: instruction_name = "vcmpltss"; break;
        case 0x02: instruction_name = "vcmpless"; break;
        case 0x03: instruction_name = "vcmpunordss"; break;
        case 0x04: instruction_name = "vcmpneqss"; break;
        case 0x05: instruction_name = "vcmpnltss"; break;
        case 0x06: instruction_name = "vcmpnless"; break;
        case 0x07: instruction_name = "vcmpordss"; break;
        case 0x08: instruction_name = "vcmpeq_uqss"; break;
        case 0x09: instruction_name = "vcmpngess"; break;
        case 0x0a: instruction_name = "vcmpngtss"; break;
        case 0x0b: instruction_name = "vcmpfalsess"; break;
        case 0x0c: instruction_name = "vcmpneq_oqss"; break;
        case 0x0d: instruction_name = "vcmpgess"; break;
        case 0x0e: instruction_name = "vcmpgtss"; break;
        case 0x0f: instruction_name = "vcmptruess"; break;
        case 0x10: instruction_name = "vcmpeq_osss"; break;
        case 0x11: instruction_name = "vcmplt_oqss"; break;
        case 0x12: instruction_name = "vcmple_oqss"; break;
        case 0x13: instruction_name = "vcmpunord_sss"; break;
        case 0x14: instruction_name = "vcmpneq_usss"; break;
        case 0x15: instruction_name = "vcmpnlt_uqss"; break;
        case 0x16: instruction_name = "vcmpnle_uqss"; break;
        case 0x17: instruction_name = "vcmpord_sss"; break;
        case 0x18: instruction_name = "vcmpeq_usss"; break;
        case 0x19: instruction_name = "vcmpnge_uqss"; break;
        case 0x1a: instruction_name = "vcmpngt_uqss"; break;
        case 0x1b: instruction_name = "vcmpfalse_osss"; break;
        case 0x1c: instruction_name = "vcmpneq_osss"; break;
        case 0x1d: instruction_name = "vcmpge_oqss"; break;
        case 0x1e: instruction_name = "vcmpgt_oqss"; break;
        case 0x1f: instruction_name = "vcmptrue_usss"; break;
      }
      --operands_count;
    }
  }
  /* "pclmulqdq" has two-operand mnemonic names for "imm8"
   * equal to 0x0, ... 0x3. */
  if (!strcmp(instruction_name, "pclmulqdq")) {
    if (instruction->imm[0] < 0x04) {
      switch (instruction->imm[0]) {
        case 0x00: instruction_name = "pclmullqlqdq"; break;
        case 0x01: instruction_name = "pclmulhqlqdq"; break;
        case 0x02: instruction_name = "pclmullqhqdq"; break;
        case 0x03: instruction_name = "pclmulhqhqdq"; break;
      }
      --operands_count;
    }
  }
  /* "vpclmulqdq" has two-operand mnemonic names for "imm8"
   * equal to 0x0, ... 0x3. */
  if (!strcmp(instruction_name, "vpclmulqdq")) {
    if (instruction->imm[0] < 0x04) {
      switch (instruction->imm[0]) {
        case 0x00: instruction_name = "vpclmullqlqdq"; break;
        case 0x01: instruction_name = "vpclmulhqlqdq"; break;
        case 0x02: instruction_name = "vpclmullqhqdq"; break;
        case 0x03: instruction_name = "vpclmulhqhqdq"; break;
      }
      --operands_count;
    }
  }
  if (operands_count > 0) {
    show_name_suffix = TRUE;
    for (i=operands_count-1; i>=0; --i) {
      if (instruction->operands[i].name == JMP_TO) {
        /* Most control flow instructions never use suffixes, but "call" and
           "jmp" do... unless byte offset is used.  */
        if ((!strcmp(instruction_name, "call")) ||
            (!strcmp(instruction_name, "jmp"))) {
          switch (instruction->operands[i].type) {
            case OperandSize8bit: show_name_suffix = FALSE; break;
            case OperandSize16bit: show_name_suffix = 'w'; break;
            case OperandSize32bit:
              if (((struct DecodeState *)userdata)->ia32_mode) {
                show_name_suffix = FALSE;
              } else {
                show_name_suffix = 'q';
              }
              break;
            case OperandSize2bit:
            case OperandSize64bit:
            case OperandSize128bit:
            case OperandSize256bit:
            case OperandFloatSize16bit:
            case OperandFloatSize32bit:
            case OperandFloatSize64bit:
            case OperandFloatSize80bit:
            case OperandX87Size16bit:
            case OperandX87Size32bit:
            case OperandX87Size64bit:
            case OperandX87BCD:
            case OperandX87ENV:
            case OperandX87STATE:
            case OperandX87MMXXMMSTATE:
            case OperandST:
            case OperandSelector:
            case OperandFarPtr:
            case OperandSegmentRegister:
            case OperandControlRegister:
            case OperandDebugRegister:
            case OperandMMX:
            case OperandXMM:
            case OperandYMM:
              assert(FALSE);
          }
        } else {
          show_name_suffix = FALSE;
        }
      } else if ((instruction->operands[i].name == REG_IMM) ||
                 (instruction->operands[i].name == REG_IMM2) ||
                 (instruction->operands[i].name == REG_RM) ||
                 (instruction->operands[i].name == REG_PORT_DX) ||
                 (instruction->operands[i].name == REG_ES_RDI) ||
                 (instruction->operands[i].name == REG_DS_RSI)) {
        if (show_name_suffix) {
          switch (instruction->operands[i].type) {
            case OperandSize8bit: show_name_suffix = 'b'; break;
            case OperandSize16bit: show_name_suffix = 'w'; break;
            case OperandSize32bit: show_name_suffix = 'l'; break;
            case OperandSize64bit: show_name_suffix = 'q'; break;
            case OperandFloatSize32bit: show_name_suffix = 's'; break;
            case OperandFloatSize64bit: show_name_suffix = 'l'; break;
            case OperandFloatSize80bit:show_name_suffix = 't'; break;
            case OperandX87Size32bit: show_name_suffix = 'l'; break;
            case OperandX87Size64bit: show_name_suffix = 'L'; break;
            case OperandSize2bit:
            case OperandX87Size16bit:
            case OperandX87BCD:
            case OperandX87ENV:
            case OperandX87STATE:
            case OperandX87MMXXMMSTATE:
            case OperandSize128bit:
            case OperandSize256bit:
            case OperandFarPtr:
            case OperandMMX:
            case OperandXMM:
            case OperandYMM:
            case OperandSelector: show_name_suffix = FALSE; break;
            case OperandFloatSize16bit:
            case OperandST:
            case OperandSegmentRegister:
            case OperandControlRegister:
            case OperandDebugRegister:
              assert(FALSE);
          }
        }
      } else {
        /* "Empty" rex prefix (0x40) is used to select "sil"/"dil"/"spl"/"bpl".
         */
        if (instruction->operands[i].type == OperandSize8bit &&
            instruction->operands[i].name <= REG_R15) {
          empty_rex_prefix_ok = TRUE;
        }
        /* First argument of "rcl"/"rcr"/"rol"/"ror"/"sar/""shl"/"shr"
           can not be used to determine size of command.  */
        if (((i != 1) || (strcmp(instruction_name, "rcl") &&
                          strcmp(instruction_name, "rcr") &&
                          strcmp(instruction_name, "rol") &&
                          strcmp(instruction_name, "ror") &&
                          strcmp(instruction_name, "sal") &&
                          strcmp(instruction_name, "sar") &&
                          strcmp(instruction_name, "shl") &&
                          strcmp(instruction_name, "shr"))) &&
        /* Second argument of "crc32" can not be used to determine size of
           command.  */
            ((i != 0) || strcmp(instruction_name, "crc32"))) {
          show_name_suffix = FALSE;
        }
        /* First argument of "crc32" can be used for that but objdump uses
           suffix anyway. */
        if ((i == 1) && (!strcmp(instruction_name, "crc32"))) {
          switch (instruction->operands[i].type) {
            case OperandSize8bit: show_name_suffix = 'b'; break;
            case OperandSize16bit: show_name_suffix = 'w'; break;
            case OperandSize32bit: show_name_suffix = 'l'; break;
            case OperandSize64bit: show_name_suffix = 'q'; break;
            case OperandSize2bit:
            case OperandSize128bit:
            case OperandSize256bit:
            case OperandFloatSize16bit:
            case OperandFloatSize32bit:
            case OperandFloatSize64bit:
            case OperandFloatSize80bit:
            case OperandX87Size16bit:
            case OperandX87Size32bit:
            case OperandX87Size64bit:
            case OperandX87BCD:
            case OperandX87ENV:
            case OperandX87STATE:
            case OperandX87MMXXMMSTATE:
            case OperandST:
            case OperandSelector:
            case OperandFarPtr:
            case OperandSegmentRegister:
            case OperandControlRegister:
            case OperandDebugRegister:
            case OperandMMX:
            case OperandXMM:
            case OperandYMM:
              assert(FALSE);
          }
        }
      }
      if ((instruction->operands[i].name >= REG_R8) &&
          (instruction->operands[i].name <= REG_R15) &&
          (instruction->operands[i].type != OperandMMX)) {
        if (!((struct DecodeState *)userdata)->ia32_mode) {
          ++rex_bits;
          /* HACK: objdump mistakenly allows "lock" with "mov %crX,%rXX" only in
             32bit mode.  It's perfectly valid in 64bit mode, too, so instead of
             changing the decoder we fix it here.  */
          if (instruction->operands[i].type == OperandControlRegister) {
            if ((*begin == 0xf0) && !(instruction->prefix.lock)) {
              print_name("lock ");
              if (!(instruction->prefix.rex & 0x04)) {
                instruction->operands[i].name -= 8;
                --rex_bits;
              }
            }
          }
        }
      } else if (instruction->operands[i].name == REG_RM) {
        if ((instruction->rm.base >= REG_R8) &&
            (instruction->rm.base <= REG_R15)) {
          ++rex_bits;
        } else if ((instruction->rm.base == NO_REG) ||
                   (instruction->rm.base == REG_RIP)) {
          ++maybe_rex_bits;
        }
        if ((instruction->rm.index >= REG_R8) &&
            (instruction->rm.index <= REG_R15)) {
          ++rex_bits;
        }
      }
    }
  }
  if ((!strcmp(instruction_name, "cvtsi2sd") ||
       !strcmp(instruction_name, "cvtsi2ss")) &&
      instruction->operands[1].name == REG_RM) {
    if (instruction->operands[1].type == OperandSize32bit) {
      show_name_suffix = 'l';
    } else {
      show_name_suffix = 'q';
    }
  }
  if ((!strcmp(instruction_name, "vcvtpd2dq") ||
       !strcmp(instruction_name, "vcvtpd2ps") ||
       !strcmp(instruction_name, "vcvttpd2dq") ||
       !strcmp(instruction_name, "vcvttpd2ps")) &&
      instruction->operands[1].name == REG_RM) {
    if (instruction->operands[1].type == OperandXMM) {
      show_name_suffix = 'x';
    } else {
      show_name_suffix = 'y';
    }
  }
  if ((!strcmp(instruction_name, "vcvtsi2sd") ||
       !strcmp(instruction_name, "vcvtsi2ss")) &&
      instruction->operands[2].name == REG_RM) {
    if (instruction->operands[2].type == OperandSize32bit) {
      show_name_suffix = 'l';
    } else {
      show_name_suffix = 'q';
    }
  }
  if (instruction->prefix.lock) {
    print_name("lock ");
  }
  if (instruction->prefix.repnz) {
    print_name("repnz ");
  }
  if (instruction->prefix.repz) {
    /* This prefix is "rep" for "ins", "lods", "movs", "outs", "stos". For other
     * instructions print "repz".
     */
    if ((!strcmp(instruction_name, "ins")) ||
        (!strcmp(instruction_name, "lods")) ||
        (!strcmp(instruction_name, "movs")) ||
        (!strcmp(instruction_name, "outs")) ||
        (!strcmp(instruction_name, "stos"))) {
      print_name("rep ");
    } else {
      print_name("repz ");
    }
  }
  if (instruction->prefix.rex == 0x40) {
    /* First argument of "rcl"/"rcr"/"rol"/"ror"/"sar"/"shl"/"shr"
       confuses objdump: it does not show it in this case.  */
    if ((!empty_rex_prefix_ok ||
         !strcmp(instruction_name, "movsbl") ||
         !strcmp(instruction_name, "movsbw") ||
         !strcmp(instruction_name, "movzbl") ||
         !strcmp(instruction_name, "movzbw") ||
         !strcmp(instruction_name, "pextrb") ||
         !strcmp(instruction_name, "pinsrb")) &&
        ((strcmp(instruction_name, "movsbl") &&
          strcmp(instruction_name, "movsbw") &&
          strcmp(instruction_name, "movzbl") &&
          strcmp(instruction_name, "movzbw") &&
          strcmp(instruction_name, "rcl") &&
          strcmp(instruction_name, "rcr") &&
          strcmp(instruction_name, "rol") &&
          strcmp(instruction_name, "ror") &&
          strcmp(instruction_name, "sal") &&
          strcmp(instruction_name, "sar") &&
          strcmp(instruction_name, "shl") &&
          strcmp(instruction_name, "shr")) ||
         (instruction->operands[1].name > REG_R15))) {
      print_name("rex ");
    }
  }
  if ((instruction->prefix.rex & 0x08) == 0x08) {
    /* rex.W is ignored by "in"/"out", and "pop"/"push" commands.  */
    if ((!strcmp(instruction_name, "in")) ||
        (!strcmp(instruction_name, "ins")) ||
        (!strcmp(instruction_name, "out")) ||
        (!strcmp(instruction_name, "outs")) ||
        (!strcmp(instruction_name, "pop")) ||
        (!strcmp(instruction_name, "push"))) {
      rex_bits = -1;
    }
  }
  if (show_name_suffix == 'b') {
    /* "cflush", "int", "invlpg", "prefetch*", and "setcc" never use suffix. */
    if ((!strcmp(instruction_name, "clflush")) ||
        (!strcmp(instruction_name, "int")) ||
        (!strcmp(instruction_name, "invlpg")) ||
        (!strcmp(instruction_name, "prefetch")) ||
        (!strcmp(instruction_name, "prefetchnta")) ||
        (!strcmp(instruction_name, "prefetcht0")) ||
        (!strcmp(instruction_name, "prefetcht1")) ||
        (!strcmp(instruction_name, "prefetcht2")) ||
        (!strcmp(instruction_name, "prefetchw")) ||
        (!strcmp(instruction_name, "seta")) ||
        (!strcmp(instruction_name, "setae")) ||
        (!strcmp(instruction_name, "setbe")) ||
        (!strcmp(instruction_name, "setb")) ||
        (!strcmp(instruction_name, "sete")) ||
        (!strcmp(instruction_name, "setg")) ||
        (!strcmp(instruction_name, "setge")) ||
        (!strcmp(instruction_name, "setle")) ||
        (!strcmp(instruction_name, "setl")) ||
        (!strcmp(instruction_name, "setne")) ||
        (!strcmp(instruction_name, "setno")) ||
        (!strcmp(instruction_name, "setnp")) ||
        (!strcmp(instruction_name, "setns")) ||
        (!strcmp(instruction_name, "seto")) ||
        (!strcmp(instruction_name, "setp")) ||
        (!strcmp(instruction_name, "sets"))) {
      show_name_suffix = FALSE;
    /* Instruction enter accepts two immediates: word and byte. But
       objdump always uses suffix "q". This is supremely strange, but
       we want to match objdump exactly, so... here goes.  */
    } else if (!strcmp(instruction_name, "enter")) {
      if (((struct DecodeState *)userdata)->ia32_mode) {
        show_name_suffix = FALSE;
      } else {
        show_name_suffix = 'q';
      }
    }
  }
  if ((show_name_suffix == 'b') || (show_name_suffix == 'l')) {
    /* objdump always shows "6a 01" as "pushq $1", "66 68 01 00" as
       "pushw $1" yet "68 01 00 00 00" as "pushq $1" again.  This makes no
       sense whatsoever so we'll just hack around here to make sure we
       produce objdump-compatible output.  */
    if (!strcmp(instruction_name, "push")) {
      if (((struct DecodeState *)userdata)->ia32_mode) {
        if (instruction->operands[0].name != REG_RM) {
          show_name_suffix = FALSE;
        }
      } else {
        show_name_suffix = 'q';
      }
    }
  }
  if (show_name_suffix == 'w') {
    /* "lldt", "[ls]msw", "lret", "ltr", and "ver[rw]" newer use suffixes at
       all.  */
    if ((!strcmp(instruction_name, "lldt")) ||
        (!strcmp(instruction_name, "lmsw")) ||
        (!strcmp(instruction_name, "lret")) ||
        (!strcmp(instruction_name, "ltr")) ||
        (!strcmp(instruction_name, "smsw")) ||
        (!strcmp(instruction_name, "verr")) ||
        (!strcmp(instruction_name, "verw"))) {
       show_name_suffix = FALSE;
    /* "callw"/"jmpw" already includes suffix in the nanme.  */
    } else if ((!strcmp(instruction_name, "callw")) ||
               (!strcmp(instruction_name, "jmpw"))) {
      show_name_suffix = FALSE;
    /* "ret" always uses suffix "q" no matter what.  */
    } else if (!strcmp(instruction_name, "ret")) {
      if (((struct DecodeState *)userdata)->ia32_mode) {
        show_name_suffix = FALSE;
      } else {
        show_name_suffix = 'q';
      }
    }
  }
  if ((show_name_suffix == 'w') || (show_name_suffix == 'l')) {
    /* "sldt" and "str" newer uses suffixes at all.  */
    if ((!strcmp(instruction_name, "sldt")) ||
        (!strcmp(instruction_name, "str"))) {
      show_name_suffix = FALSE;
    }
  }
  if (show_name_suffix == 'l') {
    /* “calll”/“jmpl” do not exist, only “call” do.  */
    if (((struct DecodeState *)userdata)->ia32_mode &&
        (!strcmp(instruction_name, "call") ||
         !strcmp(instruction_name, "jmp"))) {
      show_name_suffix = FALSE;
    /* “popl” does not exist, only “popq” do.  */
    } else if (!((struct DecodeState *)userdata)->ia32_mode &&
               !strcmp(instruction_name, "pop")) {
      show_name_suffix = 'q';
    } else if (!strcmp(instruction_name, "ldmxcsr") ||
               !strcmp(instruction_name, "stmxcsr") ||
               !strcmp(instruction_name, "vldmxcsr") ||
               !strcmp(instruction_name, "vstmxcsr")) {
      show_name_suffix = FALSE;
    }
  }
  if (show_name_suffix == 'q') {
    /* "callq","cmpxchg8b"/"jmpq" already include suffix in the nanme.  */
    if ((!strcmp(instruction_name, "callq")) ||
        (!strcmp(instruction_name, "cmpxchg8b")) ||
        (!strcmp(instruction_name, "jmpq"))) {
       show_name_suffix = FALSE;
    }
  }
  i = (instruction->prefix.rex & 0x01) +
      ((instruction->prefix.rex & 0x02) >> 1) +
      ((instruction->prefix.rex & 0x04) >> 2);
  if (instruction->prefix.rex &&
      !((i == rex_bits) ||
        (maybe_rex_bits &&
         (instruction->prefix.rex & 0x01) && (i == rex_bits + 1)))) {
    print_name("rex.");
    if (instruction->prefix.rex & 0x08) {
      print_name("W");
    }
    if (instruction->prefix.rex & 0x04) {
      print_name("R");
    }
    if (instruction->prefix.rex & 0x02) {
      print_name("X");
    }
    if (instruction->prefix.rex & 0x01) {
      print_name("B");
    }
    print_name(" ");
  }
  printf("%s", instruction_name);
  shown_name += strlen(instruction_name);
  if (show_name_suffix) {
    if (show_name_suffix == 'L') {
      print_name("ll");
    } else {
      printf("%c", show_name_suffix);
      ++shown_name;
    }
  }
  if (!strcmp(instruction_name, "mov")) {
    if ((instruction->operands[1].name == REG_IMM) &&
       (instruction->operands[1].type == OperandSize64bit)) {
      print_name("abs");
    }
  }
  {
    size_t i;
    /* Print branch hint suffixes for conditional jump instructions (Jcc).  */
    const char* jcc_jumps[] = {
      "ja", "jae", "jbe", "jb", "je", "jg", "jge", "jle",
      "jl", "jne", "jno", "jnp", "jns", "jo", "jp", "js", NULL};
    for (i = 0; jcc_jumps[i] != NULL; ++i) {
      if (!strcmp(instruction_name, jcc_jumps[i])) {
        if (instruction->prefix.branch_not_taken) {
          print_name(",pn");
        } else if (instruction->prefix.branch_taken) {
          print_name(",pt");
        }
        break;
      }
    }
  }
#undef print_name
  if ((strcmp(instruction_name, "nop") || operands_count != 0) &&
      strcmp(instruction_name, "fwait") &&
      strcmp(instruction_name, "pop    %fs") &&
      strcmp(instruction_name, "pop    %gs") &&
      strcmp(instruction_name, "popq   %fs") &&
      strcmp(instruction_name, "popq   %gs") &&
      strcmp(instruction_name, "push   %fs") &&
      strcmp(instruction_name, "push   %gs") &&
      strcmp(instruction_name, "pushq  %fs") &&
      strcmp(instruction_name, "pushq  %gs")) {
    while (shown_name < 6) {
      printf(" ");
      ++shown_name;
    }
    if (operands_count == 0) {
      printf(" ");
    }
  }
  for (i=operands_count-1; i>=0; --i) {
    printf("%c", delimeter);
    if ((!strcmp(instruction_name, "call")) ||
        (!strcmp(instruction_name, "jmp")) ||
        (!strcmp(instruction_name, "lcall")) ||
        (!strcmp(instruction_name, "ljmp"))) {
      if (instruction->operands[i].name != JMP_TO) {
        printf("*");
      }
    } else if ((!strcmp(instruction_name, "callw")) ||
               (!strcmp(instruction_name, "callq")) ||
               (!strcmp(instruction_name, "jmpw")) ||
               (!strcmp(instruction_name, "jmpq")) ||
               (!strcmp(instruction_name, "ljmpw")) ||
               (!strcmp(instruction_name, "ljmpq")) ||
               (!strcmp(instruction_name, "lcallw")) ||
               (!strcmp(instruction_name, "lcallq"))) {
      printf("*");
    }
    /* Dirty hack: both AMD manual and Intel manual agree that mov from general
       purpose register to segment register has signature "mov Ew Sw", but
       objdump insist on 32bit.  This is clearly error in objdump so we fix it
       here and not in decoder.  */
    if (((begin[0] == 0x8e) ||
         ((begin[0] >= 0x40) && (begin[0] <= 0x4f) && (begin[1] == 0x8e))) &&
        (instruction->operands[i].type == OperandSize16bit)) {
      operand_type = OperandSize32bit;
    } else {
      operand_type = instruction->operands[i].type;
    }
    switch (instruction->operands[i].name) {
      case REG_RAX: switch (operand_type) {
        case OperandSize8bit: printf("%%al"); break;
        case OperandSize16bit: printf("%%ax"); break;
        case OperandSize32bit: printf("%%eax"); break;
        case OperandSize64bit: printf("%%rax"); break;
        case OperandST: printf("%%st(0)"); break;
        case OperandMMX: printf("%%mm0"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm0"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm0"); break;
        case OperandSegmentRegister: printf("%%es"); break;
        case OperandControlRegister: printf("%%cr0"); break;
        case OperandDebugRegister: printf("%%db0"); break;
        default: assert(FALSE);
      }
      break;
      case REG_RCX: switch (operand_type) {
        case OperandSize8bit: printf("%%cl"); break;
        case OperandSize16bit: printf("%%cx"); break;
        case OperandSize32bit: printf("%%ecx"); break;
        case OperandSize64bit: printf("%%rcx"); break;
        case OperandST: printf("%%st(1)"); break;
        case OperandMMX: printf("%%mm1"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm1"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm1"); break;
        case OperandSegmentRegister: printf("%%cs"); break;
        case OperandControlRegister: printf("%%cr1"); break;
        case OperandDebugRegister: printf("%%db1"); break;
        default: assert(FALSE);
      }
      break;
      case REG_RDX: switch (operand_type) {
        case OperandSize8bit: printf("%%dl"); break;
        case OperandSize16bit: printf("%%dx"); break;
        case OperandSize32bit: printf("%%edx"); break;
        case OperandSize64bit: printf("%%rdx"); break;
        case OperandST: printf("%%st(2)"); break;
        case OperandMMX: printf("%%mm2"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm2"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm2"); break;
        case OperandSegmentRegister: printf("%%ss"); break;
        case OperandControlRegister: printf("%%cr2"); break;
        case OperandDebugRegister: printf("%%db2"); break;
        default: assert(FALSE);
      }
      break;
      case REG_RBX: switch (operand_type) {
        case OperandSize8bit: printf("%%bl"); break;
        case OperandSize16bit: printf("%%bx"); break;
        case OperandSize32bit: printf("%%ebx"); break;
        case OperandSize64bit: printf("%%rbx"); break;
        case OperandST: printf("%%st(3)"); break;
        case OperandMMX: printf("%%mm3"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm3"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm3"); break;
        case OperandSegmentRegister: printf("%%ds"); break;
        case OperandControlRegister: printf("%%cr3"); break;
        case OperandDebugRegister: printf("%%db3"); break;
        default: assert(FALSE);
      }
      break;
      case REG_RSP: switch (operand_type) {
        case OperandSize8bit: if (instruction->prefix.rex)
            printf("%%spl");
          else
            printf("%%ah");
          break;
        case OperandSize16bit: printf("%%sp"); break;
        case OperandSize32bit: printf("%%esp"); break;
        case OperandSize64bit: printf("%%rsp"); break;
        case OperandST: printf("%%st(4)"); break;
        case OperandMMX: printf("%%mm4"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm4"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm4"); break;
        case OperandSegmentRegister: printf("%%fs"); break;
        case OperandControlRegister: printf("%%cr4"); break;
        case OperandDebugRegister: printf("%%db4"); break;
        default: assert(FALSE);
      }
      break;
      case REG_RBP: switch (operand_type) {
        case OperandSize8bit: if (instruction->prefix.rex)
            printf("%%bpl");
          else
            printf("%%ch");
          break;
        case OperandSize16bit: printf("%%bp"); break;
        case OperandSize32bit: printf("%%ebp"); break;
        case OperandSize64bit: printf("%%rbp"); break;
        case OperandST: printf("%%st(5)"); break;
        case OperandMMX: printf("%%mm5"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm5"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm5"); break;
        case OperandSegmentRegister: printf("%%gs"); break;
        case OperandControlRegister: printf("%%cr5"); break;
        case OperandDebugRegister: printf("%%db5"); break;
        default: assert(FALSE);
      }
      break;
      case REG_RSI: switch (operand_type) {
        case OperandSize8bit: if (instruction->prefix.rex)
            printf("%%sil");
          else
            printf("%%dh");
          break;
        case OperandSize16bit: printf("%%si"); break;
        case OperandSize32bit: printf("%%esi"); break;
        case OperandSize64bit: printf("%%rsi"); break;
        case OperandST: printf("%%st(6)"); break;
        case OperandMMX: printf("%%mm6"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm6"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm6"); break;
        case OperandControlRegister: printf("%%cr6"); break;
        case OperandDebugRegister: printf("%%db6"); break;
        default: assert(FALSE);
      }
      break;
      case REG_RDI: switch (operand_type) {
        case OperandSize8bit: if (instruction->prefix.rex)
            printf("%%dil");
          else
            printf("%%bh");
          break;
        case OperandSize16bit: printf("%%di"); break;
        case OperandSize32bit: printf("%%edi"); break;
        case OperandSize64bit: printf("%%rdi"); break;
        case OperandST: printf("%%st(7)"); break;
        case OperandMMX: printf("%%mm7"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm7"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm7"); break;
        case OperandControlRegister: printf("%%cr7"); break;
        case OperandDebugRegister: printf("%%db7"); break;
        default: assert(FALSE);
      }
      break;
      case REG_R8: switch (operand_type) {
        case OperandSize8bit: printf("%%r8b"); break;
        case OperandSize16bit: printf("%%r8w"); break;
        case OperandSize32bit: printf("%%r8d"); break;
        case OperandSize64bit: printf("%%r8"); break;
        case OperandMMX: printf("%%mm0"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm8"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm8"); break;
        case OperandControlRegister: printf("%%cr8"); break;
        case OperandDebugRegister: printf("%%db8"); break;
        default: assert(FALSE);
      }
      break;
      case REG_R9: switch (operand_type) {
        case OperandSize8bit: printf("%%r9b"); break;
        case OperandSize16bit: printf("%%r9w"); break;
        case OperandSize32bit: printf("%%r9d"); break;
        case OperandSize64bit: printf("%%r9"); break;
        case OperandMMX: printf("%%mm1"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm9"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm9"); break;
        case OperandControlRegister: printf("%%cr9"); break;
        case OperandDebugRegister: printf("%%db9"); break;
        default: assert(FALSE);
      }
      break;
      case REG_R10: switch (operand_type) {
        case OperandSize8bit: printf("%%r10b"); break;
        case OperandSize16bit: printf("%%r10w"); break;
        case OperandSize32bit: printf("%%r10d"); break;
        case OperandSize64bit: printf("%%r10"); break;
        case OperandMMX: printf("%%mm2"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm10"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm10"); break;
        case OperandControlRegister: printf("%%cr10"); break;
        case OperandDebugRegister: printf("%%db10"); break;
        default: assert(FALSE);
      }
      break;
      case REG_R11: switch (operand_type) {
        case OperandSize8bit: printf("%%r11b"); break;
        case OperandSize16bit: printf("%%r11w"); break;
        case OperandSize32bit: printf("%%r11d"); break;
        case OperandSize64bit: printf("%%r11"); break;
        case OperandMMX: printf("%%mm3"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm11"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm11"); break;
        case OperandControlRegister: printf("%%cr11"); break;
        case OperandDebugRegister: printf("%%db11"); break;
        default: assert(FALSE);
      }
      break;
      case REG_R12: switch (operand_type) {
        case OperandSize8bit: printf("%%r12b"); break;
        case OperandSize16bit: printf("%%r12w"); break;
        case OperandSize32bit: printf("%%r12d"); break;
        case OperandSize64bit: printf("%%r12"); break;
        case OperandMMX: printf("%%mm4"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm12"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm12"); break;
        case OperandControlRegister: printf("%%cr12"); break;
        case OperandDebugRegister: printf("%%db12"); break;
        default: assert(FALSE);
      }
      break;
      case REG_R13: switch (operand_type) {
        case OperandSize8bit: printf("%%r13b"); break;
        case OperandSize16bit: printf("%%r13w"); break;
        case OperandSize32bit: printf("%%r13d"); break;
        case OperandSize64bit: printf("%%r13"); break;
        case OperandMMX: printf("%%mm5"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm13"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm13"); break;
        case OperandControlRegister: printf("%%cr13"); break;
        case OperandDebugRegister: printf("%%db13"); break;
        default: assert(FALSE);
      }
      break;
      case REG_R14: switch (operand_type) {
        case OperandSize8bit: printf("%%r14b"); break;
        case OperandSize16bit: printf("%%r14w"); break;
        case OperandSize32bit: printf("%%r14d"); break;
        case OperandSize64bit: printf("%%r14"); break;
        case OperandMMX: printf("%%mm6"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm14"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm14"); break;
        case OperandControlRegister: printf("%%cr14"); break;
        case OperandDebugRegister: printf("%%db14"); break;
        default: assert(FALSE);
      }
      break;
      case REG_R15: switch (operand_type) {
        case OperandSize8bit: printf("%%r15b"); break;
        case OperandSize16bit: printf("%%r15w"); break;
        case OperandSize32bit: printf("%%r15d"); break;
        case OperandSize64bit: printf("%%r15"); break;
        case OperandMMX: printf("%%mm7"); break;
        case OperandFloatSize32bit:
        case OperandFloatSize64bit:
        case OperandSize128bit:
        case OperandXMM: printf("%%xmm15"); break;
        case OperandSize256bit:
        case OperandYMM: printf("%%ymm15"); break;
        case OperandControlRegister: printf("%%cr15"); break;
        case OperandDebugRegister: printf("%%db15"); break;
        default: assert(FALSE);
      }
      break;
      case REG_ST:
        assert(operand_type == OperandST);
        printf("%%st");
        break;
      case REG_RM: {
        if (instruction->rm.offset) {
          printf("0x%"NACL_PRIx64, instruction->rm.offset);
        }
        if (((struct DecodeState *)userdata)->ia32_mode) {
          if ((instruction->rm.base != NO_REG) ||
              (instruction->rm.index != NO_REG) ||
              (instruction->rm.scale != 0)) {
            printf("(");
          }
          switch (instruction->rm.base) {
            case REG_RAX: printf("%%eax"); break;
            case REG_RCX: printf("%%ecx"); break;
            case REG_RDX: printf("%%edx"); break;
            case REG_RBX: printf("%%ebx"); break;
            case REG_RSP: printf("%%esp"); break;
            case REG_RBP: printf("%%ebp"); break;
            case REG_RSI: printf("%%esi"); break;
            case REG_RDI: printf("%%edi"); break;
            case NO_REG: break;
            case REG_R8:
            case REG_R9:
            case REG_R10:
            case REG_R11:
            case REG_R12:
            case REG_R13:
            case REG_R14:
            case REG_R15:
            case REG_RIP:
            case REG_RM:
            case REG_RIZ:
            case REG_IMM:
            case REG_IMM2:
            case REG_DS_RBX:
            case REG_ES_RDI:
            case REG_DS_RSI:
            case REG_PORT_DX:
            case REG_ST:
            case JMP_TO:
              assert(FALSE);
          }
          switch (instruction->rm.index) {
            case REG_RAX: printf(",%%eax,%d",1<<instruction->rm.scale); break;
            case REG_RCX: printf(",%%ecx,%d",1<<instruction->rm.scale); break;
            case REG_RDX: printf(",%%edx,%d",1<<instruction->rm.scale); break;
            case REG_RBX: printf(",%%ebx,%d",1<<instruction->rm.scale); break;
            case REG_RSP: printf(",%%esp,%d",1<<instruction->rm.scale); break;
            case REG_RBP: printf(",%%ebp,%d",1<<instruction->rm.scale); break;
            case REG_RSI: printf(",%%esi,%d",1<<instruction->rm.scale); break;
            case REG_RDI: printf(",%%edi,%d",1<<instruction->rm.scale); break;
            case REG_RIZ: if ((instruction->rm.base != REG_RSP) ||
                              (instruction->rm.scale != 0))
                printf(",%%eiz,%d",1<<instruction->rm.scale);
              break;
            case NO_REG: break;
            case REG_R8:
            case REG_R9:
            case REG_R10:
            case REG_R11:
            case REG_R12:
            case REG_R13:
            case REG_R14:
            case REG_R15:
            case REG_RM:
            case REG_RIP:
            case REG_IMM:
            case REG_IMM2:
            case REG_DS_RBX:
            case REG_ES_RDI:
            case REG_DS_RSI:
            case REG_PORT_DX:
            case REG_ST:
            case JMP_TO:
              assert(FALSE);
          }
          if ((instruction->rm.base != NO_REG) ||
              (instruction->rm.index != NO_REG) ||
              (instruction->rm.scale != 0)) {
            printf(")");
          }
        } else {
          if ((instruction->rm.base != NO_REG) ||
              (instruction->rm.index != REG_RIZ) ||
              (instruction->rm.scale != 0)) {
            printf("(");
          }
          switch (instruction->rm.base) {
            case REG_RAX: printf("%%rax"); break;
            case REG_RCX: printf("%%rcx"); break;
            case REG_RDX: printf("%%rdx"); break;
            case REG_RBX: printf("%%rbx"); break;
            case REG_RSP: printf("%%rsp"); break;
            case REG_RBP: printf("%%rbp"); break;
            case REG_RSI: printf("%%rsi"); break;
            case REG_RDI: printf("%%rdi"); break;
            case REG_R8: printf("%%r8"); break;
            case REG_R9: printf("%%r9"); break;
            case REG_R10: printf("%%r10"); break;
            case REG_R11: printf("%%r11"); break;
            case REG_R12: printf("%%r12"); break;
            case REG_R13: printf("%%r13"); break;
            case REG_R14: printf("%%r14"); break;
            case REG_R15: printf("%%r15"); break;
            case REG_RIP: printf("%%rip"); print_rip = TRUE; break;
            case NO_REG: break;
            case REG_RM:
            case REG_RIZ:
            case REG_IMM:
            case REG_IMM2:
            case REG_DS_RBX:
            case REG_ES_RDI:
            case REG_DS_RSI:
            case REG_PORT_DX:
            case REG_ST:
            case JMP_TO:
              assert(FALSE);
          }
          switch (instruction->rm.index) {
            case REG_RAX: printf(",%%rax,%d",1<<instruction->rm.scale); break;
            case REG_RCX: printf(",%%rcx,%d",1<<instruction->rm.scale); break;
            case REG_RDX: printf(",%%rdx,%d",1<<instruction->rm.scale); break;
            case REG_RBX: printf(",%%rbx,%d",1<<instruction->rm.scale); break;
            case REG_RSP: printf(",%%rsp,%d",1<<instruction->rm.scale); break;
            case REG_RBP: printf(",%%rbp,%d",1<<instruction->rm.scale); break;
            case REG_RSI: printf(",%%rsi,%d",1<<instruction->rm.scale); break;
            case REG_RDI: printf(",%%rdi,%d",1<<instruction->rm.scale); break;
            case REG_R8: printf(",%%r8,%d",1<<instruction->rm.scale); break;
            case REG_R9: printf(",%%r9,%d",1<<instruction->rm.scale); break;
            case REG_R10: printf(",%%r10,%d",1<<instruction->rm.scale); break;
            case REG_R11: printf(",%%r11,%d",1<<instruction->rm.scale); break;
            case REG_R12: printf(",%%r12,%d",1<<instruction->rm.scale); break;
            case REG_R13: printf(",%%r13,%d",1<<instruction->rm.scale); break;
            case REG_R14: printf(",%%r14,%d",1<<instruction->rm.scale); break;
            case REG_R15: printf(",%%r15,%d",1<<instruction->rm.scale); break;
            case REG_RIZ: if (((instruction->rm.base != NO_REG) &&
                               (instruction->rm.base != REG_RSP) &&
                               (instruction->rm.base != REG_R12)) ||
                               (instruction->rm.scale != 0))
                printf(",%%riz,%d",1<<instruction->rm.scale);
              break;
            case NO_REG: break;
            case REG_RM:
            case REG_RIP:
            case REG_IMM:
            case REG_IMM2:
            case REG_DS_RBX:
            case REG_ES_RDI:
            case REG_DS_RSI:
            case REG_PORT_DX:
            case REG_ST:
            case JMP_TO:
              assert(FALSE);
          }
          if ((instruction->rm.base != NO_REG) ||
              (instruction->rm.index != REG_RIZ) ||
              (instruction->rm.scale != 0)) {
            printf(")");
          }
        }
      }
      break;
      case REG_IMM: {
        printf("$0x%"NACL_PRIx64,instruction->imm[0]);
        break;
      }
      case REG_IMM2: {
        printf("$0x%"NACL_PRIx64,instruction->imm[1]);
        break;
      }
      case REG_PORT_DX: printf("(%%dx)"); break;
      case REG_DS_RBX: if (((struct DecodeState *)userdata)->ia32_mode) {
          printf("%%ds:(%%ebx)");
        } else {
          printf("%%ds:(%%rbx)");
        }
        break;
      case REG_ES_RDI: if (((struct DecodeState *)userdata)->ia32_mode) {
          printf("%%es:(%%edi)");
        } else {
          printf("%%es:(%%rdi)");
        }
        break;
      case REG_DS_RSI: if (((struct DecodeState *)userdata)->ia32_mode) {
          printf("%%ds:(%%esi)");
        } else {
          printf("%%ds:(%%rsi)");
        }
        break;
      case JMP_TO: if (instruction->operands[0].type == OperandSize16bit)
          printf("0x%lx", (long)((end + instruction->rm.offset -
                         (((struct DecodeState *)userdata)->offset)) & 0xffff));
        else
          printf("0x%lx", (long)(end + instruction->rm.offset -
                                   (((struct DecodeState *)userdata)->offset)));
        break;
      case REG_RIP:
      case REG_RIZ:
      case NO_REG:
        assert(FALSE);
    }
    delimeter = ',';
  }
  if (print_rip) {
    printf("        # 0x%8"NACL_PRIx64,
           (uint64_t) (end + instruction->rm.offset -
               (((struct DecodeState *)userdata)->offset)));
  }
  printf("\n");
  begin += 7;
  while (begin < end) {
    printf("%*"NACL_PRIx64":\t", ((struct DecodeState *)userdata)->width,
           (uint64_t) (begin - (((struct DecodeState *)userdata)->offset)));
    for (p = begin; p < begin + 7; ++p) {
      if (p >= end) {
        printf("\n");
        return;
      } else {
        printf("%02x ", *p);
      }
    }
    if (p >= end) {
      printf("\n");
      return;
    }
    begin += 7;
  }
}

void ProcessError (const uint8_t *ptr, void *userdata) {
  printf("rejected at %"NACL_PRIx64" (byte 0x%02"NACL_PRIx32")\n",
         (uint64_t) (ptr - (((struct DecodeState *)userdata)->offset)),
         *ptr);
}

int DecodeFile(const char *filename, int repeat_count) {
  size_t data_size;
  uint8_t *data;
  int count;

  ReadImage(filename, &data, &data_size);
  if (data[4] == 1) {
    for (count = 0; count < repeat_count; ++count) {
      Elf32_Ehdr *header;
      int index;

      header = (Elf32_Ehdr *) data;
      CheckBounds(data, data_size, header, sizeof(*header));
      assert(memcmp(header->e_ident, ELFMAG, strlen(ELFMAG)) == 0);

      for (index = 0; index < header->e_shnum; ++index) {
        Elf32_Shdr *section = (Elf32_Shdr *) (data + header->e_shoff +
                                                   header->e_shentsize * index);
        CheckBounds(data, data_size, section, sizeof(*section));

        if ((section->sh_flags & SHF_EXECINSTR) != 0) {
          struct DecodeState state;
          int res;

          state.ia32_mode = TRUE;
          state.fwait = FALSE;
          state.offset = data + section->sh_offset - section->sh_addr;
          if (section->sh_size <= 0xfff) {
            state.width = 4;
          } else if (section->sh_size <= 0xfffffff) {
            state.width = 8;
          } else {
            state.width = 12;
          }
          CheckBounds(data, data_size,
                      data + section->sh_offset, section->sh_size);
          res = DecodeChunkIA32(data + section->sh_offset, section->sh_size,
                                      ProcessInstruction, ProcessError, &state);
          if (res != 0) {
            return res;
          } else if (state.fwait) {
            while (state.fwait < data + section->sh_offset + section->sh_size) {
              printf("%*lx:\t9b                   \tfwait\n",
                             state.width, (long)(state.fwait++ - state.offset));
            }
          }
        }
      }
    }
  } else if (data[4] == 2) {
    for (count = 0; count < repeat_count; ++count) {
      Elf64_Ehdr *header;
      int index;

      header = (Elf64_Ehdr *) data;
      CheckBounds(data, data_size, header, sizeof(*header));
      assert(memcmp(header->e_ident, ELFMAG, strlen(ELFMAG)) == 0);

      for (index = 0; index < header->e_shnum; ++index) {
        Elf64_Shdr *section = (Elf64_Shdr *) (data + header->e_shoff +
                                                   header->e_shentsize * index);
        CheckBounds(data, data_size, section, sizeof(*section));

        if ((section->sh_flags & SHF_EXECINSTR) != 0) {
          struct DecodeState state;
          int res;

          state.ia32_mode = FALSE;
          state.fwait = FALSE;
          state.offset = data + section->sh_offset - section->sh_addr;
          if (section->sh_size <= 0xfff) {
            state.width = 4;
          } else if (section->sh_size <= 0xfffffff) {
            state.width = 8;
          } else if (section->sh_size <= 0xfffffffffffLL) {
            state.width = 12;
          } else {
            state.width = 16;
          }
          CheckBounds(data, data_size,
                      data + section->sh_offset, (size_t)section->sh_size);
          res = DecodeChunkAMD64(data + section->sh_offset,
                                 (size_t)section->sh_size,
                                 ProcessInstruction, ProcessError, &state);
          if (res != 0) {
            return res;
          } else if (state.fwait) {
            while (state.fwait < data + section->sh_offset + section->sh_size) {
              printf("%*lx:\t9b                   \tfwait\n",
                             state.width, (long)(state.fwait++ - state.offset));
            }
          }
        }
      }
    }
  } else {
    printf("Unknown ELF class: %s\n", filename);
    exit(1);
  }
  return 0;
}

int main(int argc, char **argv) {
  int index, initial_index = 1, repeat_count = 1;
  if (argc == 1) {
    printf("%s: no input files\n", argv[0]);
    exit(1);
  }
  if (!strcmp(argv[1], "--repeat"))
    repeat_count = atoi(argv[2]),
    initial_index += 2;
  for (index = initial_index; index < argc; ++index) {
    const char *filename = argv[index];
    int rc = DecodeFile(filename, repeat_count);
    if (rc != 0) {
      printf("file '%s' can not be fully decoded\n", filename);
      return 1;
    }
  }
  return 0;
}
