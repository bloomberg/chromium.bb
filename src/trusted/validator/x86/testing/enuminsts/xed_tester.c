/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * xed_tester.c
 * Implements a xed decoder that can be used as a NaClEnumeratorTester.
 */

#include <string.h>
#include "xed-interface.h"

#include "native_client/src/trusted/validator/x86/testing/enuminsts/enuminsts.h"

#define kBufferSize 1024

/* Defines the virtual table for this tester. */
struct {
  /* The virtual table that implements this tester. */
  NaClEnumeratorTester _base;
  /* Defines the xed state to use to parse instructions. */
  xed_state_t _xed_state;
  /* Defines the pc_address to assume when disassembling. */
  int _pc_address;
  /* Defines the disassembled xed instruction. */
  xed_decoded_inst_t _xedd;
  /* Defines the lower-level model of the disassembled xed instruction. */
  xed_inst_t const *_xed_inst;
  /* Defines if there were errors parsing the instruction. */
  xed_error_enum_t _xed_error;

  /* Defines whether we have disassembled the xed instruction. */
  Bool _has_xed_disasm;
  /* If _has_xed_disam is true, the corresponding disassembly. */
  char _xed_disasm[kBufferSize];
  /* If non-empty, the corresponding instruction mnemonic. */
  char _xed_opcode[kBufferSize];
  /* Stores the corresponding operands of the instruction mnemonic. */
  char _xed_operands[kBufferSize];
  /* True if ConditionallyPrintInst should print a value. */
  Bool _print_conditionally;
} xed_tester;



/* Initialize xed state before we try to decode anything. */
static void XedSetup() {
  xed_tables_init();
  xed_state_zero(&xed_tester._xed_state);

  /* dstate.stack_addr_width = XED_ADDRESS_WIDTH_32b; */
#if (NACL_TARGET_SUBARCH == 32)
  xed_tester._xed_state.mmode = XED_MACHINE_MODE_LONG_COMPAT_32;
#endif
#if (NACL_TARGET_SUBARCH == 64)
  xed_tester._xed_state.mmode = XED_MACHINE_MODE_LONG_64;
#endif
}

/* Defines the function to parse the first instruction. */
static void ParseInst(NaClEnumerator* enumerator, int pc_address) {
  xed_tester._has_xed_disasm = FALSE;
  xed_tester._pc_address = pc_address;
  xed_tester._xed_disasm[0] = 0;
  xed_tester._xed_opcode[0] = 0;
  xed_tester._xed_operands[0] = 0;
  xed_tester._xed_inst = NULL;
  xed_decoded_inst_set_input_chip(&xed_tester._xedd, XED_CHIP_CORE2);
  xed_decoded_inst_zero_set_mode(&xed_tester._xedd, &xed_tester._xed_state);
  xed_tester._xed_error = xed_decode
      (&xed_tester._xedd, (const xed_uint8_t*)enumerator->_itext,
       enumerator->_num_bytes);
}

/* Returns the disassembled instruction. */
static const char* Disassemble(NaClEnumerator* enumerator) {
  if (!xed_tester._has_xed_disasm) {
    if (xed_tester._xedd._decoded_length == 0) {
      strcpy(xed_tester._xed_disasm, "[illegal instruction]");
    }
    xed_format_intel(&xed_tester._xedd, xed_tester._xed_disasm,
                     kBufferSize, xed_tester._pc_address);
    xed_tester._has_xed_disasm = TRUE;
  }
  return xed_tester._xed_disasm;
}

/* Returns the mnemonic name for the disassembled instruction. */
static const char* GetInstMnemonic(NaClEnumerator* enumerator) {
  char *allocated;
  char *xtmp;
  char *prev_xtmp;
  int char0 = 0;

  /* First see if we have cached it. If so, return it. */
  if (xed_tester._xed_opcode[0] != 0) return xed_tester._xed_opcode;

  /* If reached, we haven't cached it, so find the name from the
   * disassembled instruction, and cache it.
   */
  xtmp = allocated = strdup(Disassemble(enumerator));

  /* Remove while prefixes found (i.e. ignore ordering) to get opcode. */
  while (1) {
    xtmp = SkipPrefix(xtmp, "lock", strlen("lock"));
    xtmp = SkipPrefix(xtmp, "repne", strlen("repne"));
    xtmp = SkipPrefix(xtmp, "rep", strlen("rep"));
    xtmp = SkipPrefix(xtmp, "hint-not-taken", strlen("hint-not-taken"));
    xtmp = SkipPrefix(xtmp, "hint-taken", strlen("hint-taken"));
    xtmp = SkipPrefix(xtmp, "addr16", strlen("addr16"));
    xtmp = SkipPrefix(xtmp, "addr32", strlen("addr32"));
    xtmp = SkipPrefix(xtmp, "data16", strlen("data16"));
    if (xtmp == prev_xtmp) break;
    prev_xtmp = xtmp;
  }
  strncpyto(xed_tester._xed_opcode, xtmp, kBufferSize - char0, ' ');

  /* Cache operand text to be processed before returning. */
  xtmp += strlen(xed_tester._xed_opcode);

  /* Remove uninteresting decorations.
   * NOTE: these patterns need to be ordered from most to least specific
   */
  CleanString(xtmp, "byte ptr ");
  CleanString(xtmp, "dword ptr ");
  CleanString(xtmp, "qword ptr ");
  CleanString(xtmp, "xmmword ptr ");
  CleanString(xtmp, "word ptr ");
  CleanString(xtmp, "ptr ");
  CleanString(xtmp, "far ");

  strncpy(xed_tester._xed_operands, strip(xtmp), kBufferSize);
  free(allocated);
  return xed_tester._xed_opcode;
}

static const char* GetInstOperandsText(NaClEnumerator* enumerator) {
  /* Force caching of operands and return. */
  if (xed_tester._xed_operands[0] == 0) GetInstMnemonic(enumerator);
  return xed_tester._xed_operands;
}

/* Prints out the disassembled instruction. */
static void PrintInst(NaClEnumerator* enumerator) {
  if (xed_tester._xedd._decoded_length) {
    int i;
    if (enumerator->_print_opcode_bytes_only) {
      for (i = 0; i < xed_tester._xedd._decoded_length; ++i) {
        printf("%02x", enumerator->_itext[i]);
      }
      printf("\n");
    } else {
      if (!enumerator->_print_enumerated_instruction) {
        printf("  XED: ");
      }
      /* Since xed doesn't print out opcode sequence, and it is
       * useful to know, add it to the print out.
       */
      for (i = 0; i < enumerator->_num_bytes; ++i) {
        if (i < xed_tester._xedd._decoded_length) {
          printf("%02x ", enumerator->_itext[i]);
        } else if (!enumerator->_print_enumerated_instruction) {
          printf("   ");
        }
      }
      if (enumerator->_print_enumerated_instruction) {
        printf("#%s %s\n", GetInstMnemonic(enumerator),
               GetInstOperandsText(enumerator));
      } else {
        /* Note: spacing added so that it lines up with nacl output. */
        printf(":\t\t      %s\n", Disassemble(enumerator));
      }
    }
  }
}

static Bool ConditionallyPrintInst(NaClEnumerator* enumerator) {
  if (xed_tester._print_conditionally) PrintInst(enumerator);
  return xed_tester._print_conditionally;
}

static void SetConditionalPrinting(NaClEnumerator* enumerator,
                                   Bool new_value) {
  xed_tester._print_conditionally = new_value;
}

static size_t InstLength(NaClEnumerator* enumerator) {
  return (size_t) xed_tester._xedd._decoded_length;
}

static inline xed_inst_t const* GetXedInst() {
  if (xed_tester._xed_inst == NULL) {
    xed_tester._xed_inst = xed_decoded_inst_inst(&xed_tester._xedd);
  }
  return xed_tester._xed_inst;
}

static size_t GetNumOperands(NaClEnumerator* enumerator) {
  return (size_t) xed_inst_noperands(GetXedInst());
}

#if NACL_TARGET_SUBARCH == 64
static int IsReservedReg(xed_reg_enum_t reg) {
  switch (reg) {
    case XED_REG_RSP:
    case XED_REG_RBP:
    case XED_REG_R15:
      return 1;
  }
  return 0;
}


static int IsWriteAction(xed_operand_action_enum_t rw) {
  switch (rw) {
    case XED_OPERAND_ACTION_RW:
    case XED_OPERAND_ACTION_W:
    case XED_OPERAND_ACTION_RCW:
    case XED_OPERAND_ACTION_CW:
    case XED_OPERAND_ACTION_CRW:
      return 1;
  }
  return 0;
}

static Bool WritesToReservedReg(NaClEnumerator* enumerator,
                                size_t n) {
  xed_inst_t const* xi = GetXedInst();
  xed_operand_t const* op = xed_inst_operand(xi, n);
  xed_operand_enum_t op_name = xed_operand_name(op);
  return xed_operand_is_register(op_name) &&
      IsReservedReg(xed_decoded_inst_get_reg(&xed_tester._xedd, op_name)) &&
      IsWriteAction(xed_operand_rw(op));
}
#elif NACL_TARGET_SUBARCH == 32
static Bool WritesToReservedReg(NaClEnumerator* enumerator,
                                size_t n) {
  return FALSE;
}
#else
#error("Bad NACL_TARGET_SUBARCH")
#endif

static Bool IsInstLegal(NaClEnumerator* enumerator) {
  return (xed_tester._xedd._decoded_length != 0) &&
      (XED_ERROR_NONE == xed_tester._xed_error);
}

/* Defines the registry function that creates a xed tester, and returns
 * the tester to be registered.
 */
NaClEnumeratorTester* RegisterXedTester() {
  XedSetup();
  xed_tester._print_conditionally = FALSE;
  xed_tester._base._id_name = "xed";
  xed_tester._base._parse_inst_fn = ParseInst;
  xed_tester._base._inst_length_fn = InstLength;
  xed_tester._base._print_inst_fn = PrintInst;
  xed_tester._base._conditionally_print_inst_fn = ConditionallyPrintInst;
  xed_tester._base._set_conditional_printing_fn = SetConditionalPrinting;
  xed_tester._base._get_inst_mnemonic_fn = GetInstMnemonic;
  xed_tester._base._get_inst_num_operands_fn = GetNumOperands;
  xed_tester._base._get_inst_operands_text_fn = GetInstOperandsText;
  xed_tester._base._writes_to_reserved_reg_fn = WritesToReservedReg;
  xed_tester._base._maybe_inst_validates_fn = IsInstLegal;
  return &xed_tester._base;
}
