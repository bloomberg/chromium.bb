/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ragel_tester.c
 * Implements a ragel decoder that can be used as a NaClEnumeratorDecoder.
 */
#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB.")
#endif

#include "native_client/src/trusted/validator/x86/testing/enuminsts/enuminsts.h"

#include <string.h>
#include "native_client/src/trusted/validator/types_memory_model.h"
#include "native_client/src/trusted/validator/x86/ncinstbuffer.h"
#include "native_client/src/trusted/validator/x86/testing/enuminsts/str_utils.h"
#include "native_client/src/trusted/validator_ragel/decoder.h"
#include "native_client/src/trusted/validator_ragel/validator.h"

#define kBufferSize 1024

/* Defines the virtual table for the ragel decoder. */
struct {
  /* The virtual table that implements this decoder. */
  NaClEnumeratorDecoder _base;
} ragel_decoder;

/* Initialize ragel state before we try to decode anything. */
static void RagelSetup(void) {
}

struct RagelDecodeState {
  const uint8_t *inst_offset;
  uint8_t inst_num_bytes;
  uint8_t valid_state;  /* indicates if this struct describes an instruction */
  const char *inst_name;
  int inst_is_legal;  /* legal means decodes correctly */
  int inst_is_valid;  /* valid means validator is happy */
};
struct RagelDecodeState RState;

static const char* RGetInstMnemonic(const NaClEnumerator* enumerator) {
  UNREFERENCED_PARAMETER(enumerator);
  return RState.inst_name;
}

static void RagelPrintInst(void) {
  int i;
  int print_num_bytes = RState.inst_num_bytes;

  if (print_num_bytes == 0) print_num_bytes = 4;
  for (i = 0; i < print_num_bytes; i++) {
    printf("%02x ", RState.inst_offset[i]);
  }
  printf(": %s\n", RState.inst_name);
}



/* It's difficult to use this to detect actual decoder errors because for */
/* enuminst we want to ignore errors for all but the first instruction.   */
void RagelDecodeError (const uint8_t *ptr, void *userdata) {
  UNREFERENCED_PARAMETER(ptr);
  UNREFERENCED_PARAMETER(userdata);
  return;
}

Bool RagelValidateError(const uint8_t *begin, const uint8_t *end,
                        uint32_t error, void *callback_data) {
  UNREFERENCED_PARAMETER(begin);
  UNREFERENCED_PARAMETER(end);
  UNREFERENCED_PARAMETER(callback_data);
  if (error & (UNRECOGNIZED_INSTRUCTION |
               CPUID_UNSUPPORTED_INSTRUCTION |
               FORBIDDEN_BASE_REGISTER |
               UNRESTRICTED_INDEX_REGISTER |
               R15_MODIFIED | BP_MODIFIED | SP_MODIFIED |
               UNRESTRICTED_RBP_PROCESSED | UNRESTRICTED_RSP_PROCESSED |
               RESTRICTED_RSP_UNPROCESSED | RESTRICTED_RBP_UNPROCESSED)) {
    return FALSE;
  } else {
    return TRUE;
  }
}

void RagelInstruction(const uint8_t *begin, const uint8_t *end,
                      struct Instruction *instruction, void *userdata) {
  struct RagelDecodeState *rstate = (struct RagelDecodeState *)userdata;
  UNREFERENCED_PARAMETER(instruction);
  /* Only look at the first instruction. */
  if (rstate->valid_state) return;
  if (end > begin) {
    rstate->inst_num_bytes = (uint8_t)(end - begin);
    rstate->inst_name = instruction->name;
  } else {
    rstate->inst_num_bytes = 0;
  }
  rstate->valid_state = 1;
}

static void InitializeRagelDecodeState(struct RagelDecodeState *rs,
                                       const uint8_t *itext) {
  rs->valid_state = 0;
  rs->inst_offset = itext;
  rs->inst_num_bytes = 0;
  rs->inst_is_legal = 0;
  rs->inst_is_valid = 0;
  rs->inst_name = "undefined";
}

/* Defines the function to parse the first instruction. Note RState.ready */
/* mechanism forces parsing of at most one instruction. */
static void RParseInst(const NaClEnumerator* enumerator, const int pc_address) {
  int res;
  struct RagelDecodeState tempstate;

  UNREFERENCED_PARAMETER(pc_address);
  InitializeRagelDecodeState(&tempstate, enumerator->_itext);
  InitializeRagelDecodeState(&RState, enumerator->_itext);

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_TARGET_SUBARCH == 64
#define DecodeChunkArch DecodeChunkAMD64
#define ValidateChunkArch ValidateChunkAMD64
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_TARGET_SUBARCH == 32
#define DecodeChunkArch DecodeChunkIA32
#define ValidateChunkArch ValidateChunkIA32
#else
#error("Unsupported architecture")
#endif
  /* Since DecodeChunkArch looks at multiple instructions and we only */
  /* care about the first instruction, ignore the return code here. */
  (void)DecodeChunkArch(enumerator->_itext, enumerator->_num_bytes,
                        RagelInstruction, RagelDecodeError, &tempstate);

  if (tempstate.inst_num_bytes == 0) {
    /* This indicates a decoder error. */
  } else {
    /* Decode again, this time specifying length of first instruction. */
    res = DecodeChunkArch(enumerator->_itext, tempstate.inst_num_bytes,
                          RagelInstruction, RagelDecodeError, &RState);
    RState.inst_is_legal = res;
  }

  if (RState.inst_is_legal) {
    uint8_t chunk[(NACL_ENUM_MAX_INSTRUCTION_BYTES + kBundleMask) &
                  ~kBundleMask];

    /* Copy the command.  */
    memcpy(chunk, enumerator->_itext, tempstate.inst_num_bytes);
    /* Fill the rest with HLTs.  */
    memset(chunk + tempstate.inst_num_bytes, 0xf4,
           sizeof(chunk) - tempstate.inst_num_bytes);
    res = ValidateChunkArch(chunk, sizeof(chunk), 0 /*options*/,
                            &kFullCPUIDFeatures,
                            RagelValidateError, NULL);
    RState.inst_is_valid = res;
  }

#undef DecodeChunkArch
#undef ValidateChunkArch
}

/* Returns true if the instruction parsed a legal instruction. */
static Bool RIsInstLegal(const NaClEnumerator* enumerator) {
  UNREFERENCED_PARAMETER(enumerator);
  return RState.inst_is_legal;
}

/* Returns true if the instruction parsed a legal instruction. */
static Bool RIsInstValid(const NaClEnumerator* enumerator) {
  UNREFERENCED_PARAMETER(enumerator);
  return RState.inst_is_valid;
}

/* Prints out the disassembled instruction. */
static void RPrintInst(const NaClEnumerator* enumerator) {
  UNREFERENCED_PARAMETER(enumerator);
  printf("Ragel: ");
  RagelPrintInst();
}

static size_t RInstLength(const NaClEnumerator* enumerator) {
  UNREFERENCED_PARAMETER(enumerator);
  return (size_t)RState.inst_num_bytes;
}

static void InstallFlag(const NaClEnumerator* enumerator,
                        const char* flag_name,
                        const void* flag_address) {
  UNREFERENCED_PARAMETER(enumerator);
  UNREFERENCED_PARAMETER(flag_name);
  UNREFERENCED_PARAMETER(flag_address);
}

/* Defines the registry function that creates a ragel decoder, and returns
 * the decoder to be registered.
 */
NaClEnumeratorDecoder* RegisterRagelDecoder(void) {
  RagelSetup();
  ragel_decoder._base._id_name = "ragel";
  ragel_decoder._base._parse_inst_fn = RParseInst;
  ragel_decoder._base._inst_length_fn = RInstLength;
  ragel_decoder._base._print_inst_fn = RPrintInst;
  ragel_decoder._base._is_inst_legal_fn = RIsInstLegal;
  ragel_decoder._base._install_flag_fn = InstallFlag;
  ragel_decoder._base._get_inst_mnemonic_fn = RGetInstMnemonic;
  ragel_decoder._base._get_inst_num_operands_fn = NULL;
  ragel_decoder._base._get_inst_operands_text_fn = NULL;
  ragel_decoder._base._writes_to_reserved_reg_fn = NULL;
  ragel_decoder._base._maybe_inst_validates_fn = RIsInstValid;
  ragel_decoder._base._segment_validates_fn = NULL;
  ragel_decoder._base._usage_message = "Runs ragel to decode instructions.";
  return &ragel_decoder._base;
}
