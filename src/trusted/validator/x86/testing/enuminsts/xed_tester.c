/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * xed_tester.c
 * Implements a xed decoder that can be used as a NaClEnumeratorDecoder.
 */

#include "native_client/src/trusted/validator/x86/testing/enuminsts/enuminsts.h"

#include <string.h>
#include "xed-interface.h"
#include "native_client/src/trusted/validator/types_memory_model.h"
#include "native_client/src/trusted/validator/x86/ncinstbuffer.h"
#include "native_client/src/trusted/validator/x86/testing/enuminsts/str_utils.h"

#define kBufferSize 1024

/* Defines the virtual table for the xed decoder. */
struct {
  /* The virtual table that implements this decoder. */
  NaClEnumeratorDecoder _base;
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
} xed_decoder;



/* Initialize xed state before we try to decode anything. */
static void XedSetup(void) {
  xed_tables_init();
  xed_state_zero(&xed_decoder._xed_state);

  /* dstate.stack_addr_width = XED_ADDRESS_WIDTH_32b; */
#if (NACL_TARGET_SUBARCH == 32)
  xed_decoder._xed_state.mmode = XED_MACHINE_MODE_LONG_COMPAT_32;
#endif
#if (NACL_TARGET_SUBARCH == 64)
  xed_decoder._xed_state.mmode = XED_MACHINE_MODE_LONG_64;
#endif
}

/* Defines the function to parse the first instruction. */
static void ParseInst(const NaClEnumerator* enumerator, const int pc_address) {
  xed_decoder._has_xed_disasm = FALSE;
  xed_decoder._pc_address = pc_address;
  xed_decoder._xed_disasm[0] = 0;
  xed_decoder._xed_opcode[0] = 0;
  xed_decoder._xed_operands[0] = 0;
  xed_decoder._xed_inst = NULL;
  xed_decoded_inst_set_input_chip(&xed_decoder._xedd, XED_CHIP_CORE2);
  xed_decoded_inst_zero_set_mode(&xed_decoder._xedd, &xed_decoder._xed_state);
  xed_decoder._xed_error = xed_decode
      (&xed_decoder._xedd, (const xed_uint8_t*)enumerator->_itext,
       enumerator->_num_bytes);
}

/* Returns true if the instruction parsed a legal instruction. */
static Bool IsInstLegal(const NaClEnumerator* enumerator) {
  return (xed_decoder._xedd._decoded_length != 0) &&
      (XED_ERROR_NONE == xed_decoder._xed_error);
}

/* Returns the disassembled instruction. */
static const char* Disassemble(const NaClEnumerator* enumerator) {
  if (!xed_decoder._has_xed_disasm) {
    if (xed_decoder._xedd._decoded_length == 0) {
      strcpy(xed_decoder._xed_disasm, "[illegal instruction]");
    }
    xed_format_intel(&xed_decoder._xedd, xed_decoder._xed_disasm,
                     kBufferSize, xed_decoder._pc_address);
    xed_decoder._has_xed_disasm = TRUE;
  }
  return xed_decoder._xed_disasm;
}

/* Returns the mnemonic name for the disassembled instruction. */
static const char* GetInstMnemonic(const NaClEnumerator* enumerator) {
  char *allocated;
  char *xtmp;
  char *prev_xtmp;
  int char0 = 0;

  /* First see if we have cached it. If so, return it. */
  if (xed_decoder._xed_opcode[0] != 0) return xed_decoder._xed_opcode;

  /* If reached, we haven't cached it, so find the name from the
   * disassembled instruction, and cache it.
   */
  xtmp = allocated = strdup(Disassemble(enumerator));

  /* Remove while prefixes found (i.e. ignore ordering) to get opcode. */
  while (1) {
    xtmp = SkipPrefix(xtmp, "lock");
    xtmp = SkipPrefix(xtmp, "repne");
    xtmp = SkipPrefix(xtmp, "rep");
    xtmp = SkipPrefix(xtmp, "hint-not-taken");
    xtmp = SkipPrefix(xtmp, "hint-taken");
    xtmp = SkipPrefix(xtmp, "addr16");
    xtmp = SkipPrefix(xtmp, "addr32");
    xtmp = SkipPrefix(xtmp, "data16");
    if (xtmp == prev_xtmp) break;
    prev_xtmp = xtmp;
  }
  strncpyto(xed_decoder._xed_opcode, xtmp, kBufferSize - char0, ' ');

  /* Cache operand text to be processed before returning. */
  xtmp += strlen(xed_decoder._xed_opcode);

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

  cstrncpy(xed_decoder._xed_operands, strip(xtmp), kBufferSize);
  free(allocated);
  return xed_decoder._xed_opcode;
}

static const char* GetInstOperandsText(const NaClEnumerator* enumerator) {
  /* Force caching of operands and return. */
  if (xed_decoder._xed_operands[0] == 0) GetInstMnemonic(enumerator);
  return xed_decoder._xed_operands;
}

/* Prints out the disassembled instruction. */
static void PrintInst(const NaClEnumerator* enumerator) {
  int i;
  size_t opcode_size;
  NaClPcAddress pc_address = (NaClPcAddress) xed_decoder._pc_address;
  printf("  XED: %"NACL_PRIxNaClPcAddressAll": ", pc_address);

  /* Since xed doesn't print out opcode sequence, and it is
   * useful to know, add it to the print out. Note: Use same
   * spacing scheme as nacl decoder, so things line up.
   */
  size_t num_bytes = MAX_INST_LENGTH;
  if (enumerator->_num_bytes > num_bytes)
    num_bytes = enumerator->_num_bytes;
  for (i = 0; i < num_bytes; ++i) {
    if (i < xed_decoder._xedd._decoded_length) {
      printf("%02x ", enumerator->_itext[i]);
    } else {
      printf("   ");
    }
  }
  printf("%s\n", Disassemble(enumerator));
}

static size_t InstLength(const NaClEnumerator* enumerator) {
  return (size_t) xed_decoder._xedd._decoded_length;
}

static inline xed_inst_t const* GetXedInst(void) {
  if (xed_decoder._xed_inst == NULL) {
    xed_decoder._xed_inst = xed_decoded_inst_inst(&xed_decoder._xedd);
  }
  return xed_decoder._xed_inst;
}

static size_t GetNumOperands(const NaClEnumerator* enumerator) {
  return (size_t) xed_inst_noperands(GetXedInst());
}

#if NACL_TARGET_SUBARCH == 64
static int IsReservedReg(const xed_reg_enum_t reg) {
  switch (reg) {
    case XED_REG_RSP:
    case XED_REG_RBP:
    case XED_REG_R15:
      return 1;
  }
  return 0;
}


static int IsWriteAction(const xed_operand_action_enum_t rw) {
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

static Bool WritesToReservedReg(const NaClEnumerator* enumerator,
                                const size_t n) {
  xed_inst_t const* xi = GetXedInst();
  xed_operand_t const* op = xed_inst_operand(xi, n);
  xed_operand_enum_t op_name = xed_operand_name(op);
  return xed_operand_is_register(op_name) &&
      IsReservedReg(xed_decoded_inst_get_reg(&xed_decoder._xedd, op_name)) &&
      IsWriteAction(xed_operand_rw(op));
}
#elif NACL_TARGET_SUBARCH == 32
static Bool WritesToReservedReg(const NaClEnumerator* enumerator,
                                const size_t n) {
  return FALSE;
}
#else
#error("Bad NACL_TARGET_SUBARCH")
#endif

static void InstallFlag(const NaClEnumerator* enumerator,
                        const char* flag_name,
                        const void* flag_address) {
}

/* Defines the registry function that creates a xed decoder, and returns
 * the decoder to be registered.
 */
NaClEnumeratorDecoder* RegisterXedDecoder(void) {
  XedSetup();
  xed_decoder._base._id_name = "xed";
  xed_decoder._base._parse_inst_fn = ParseInst;
  xed_decoder._base._inst_length_fn = InstLength;
  xed_decoder._base._print_inst_fn = PrintInst;
  xed_decoder._base._get_inst_mnemonic_fn = GetInstMnemonic;
  xed_decoder._base._get_inst_num_operands_fn = GetNumOperands;
  xed_decoder._base._get_inst_operands_text_fn = GetInstOperandsText;
  xed_decoder._base._writes_to_reserved_reg_fn = WritesToReservedReg;
  xed_decoder._base._is_inst_legal_fn = IsInstLegal;
  xed_decoder._base._maybe_inst_validates_fn = NULL;
  xed_decoder._base._segment_validates_fn = NULL;
  xed_decoder._base._install_flag_fn = InstallFlag;
  xed_decoder._base._usage_message = "Runs xed to decode instructions.";
  return &xed_decoder._base;
}
