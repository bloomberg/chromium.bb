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
#include "native_client/src/trusted/validator_ragel/unreviewed/decoder.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator.h"

#define kBufferSize 1024

/* Defines the virtual table for the ragel decoder. */
struct {
  /* The virtual table that implements this decoder. */
  NaClEnumeratorDecoder _base;
} ragel_decoder;

/* Emulate features of old validator to simplify testing */
NaClCPUFeaturesX86 old_validator_features = { { 1, 1 }, {
  1, /* NaClCPUFeature_3DNOW */  /* AMD-specific */
  0, /* NaClCPUFeature_AES */
  0, /* NaClCPUFeature_AVX */
  0, /* NaClCPUFeature_BMI1 */
  1, /* NaClCPUFeature_CLFLUSH */
  0, /* NaClCPUFeature_CLMUL */
  1, /* NaClCPUFeature_CMOV */
  1, /* NaClCPUFeature_CX16 */
  1, /* NaClCPUFeature_CX8 */
  1, /* NaClCPUFeature_E3DNOW */ /* AMD-specific */
  1, /* NaClCPUFeature_EMMX */   /* AMD-specific */
  0, /* NaClCPUFeature_F16C */
  0, /* NaClCPUFeature_FMA */
  0, /* NaClCPUFeature_FMA4 */ /* AMD-specific */
  1, /* NaClCPUFeature_FXSR */
  0, /* NaClCPUFeature_LAHF */
  0, /* NaClCPUFeature_LM */
  0, /* NaClCPUFeature_LWP */ /* AMD-specific */
  1, /* NaClCPUFeature_LZCNT */  /* AMD-specific */
  1, /* NaClCPUFeature_MMX */
  1, /* NaClCPUFeature_MON */
  1, /* NaClCPUFeature_MOVBE */
  1, /* NaClCPUFeature_OSXSAVE */
  1, /* NaClCPUFeature_POPCNT */
  0, /* NaClCPUFeature_PRE */ /* AMD-specific */
  1, /* NaClCPUFeature_SSE */
  1, /* NaClCPUFeature_SSE2 */
  1, /* NaClCPUFeature_SSE3 */
  1, /* NaClCPUFeature_SSE41 */
  1, /* NaClCPUFeature_SSE42 */
  1, /* NaClCPUFeature_SSE4A */  /* AMD-specific */
  1, /* NaClCPUFeature_SSSE3 */
  0, /* NaClCPUFeature_TBM */ /* AMD-specific */
  1, /* NaClCPUFeature_TSC */
  1, /* NaClCPUFeature_x87 */
  0  /* NaClCPUFeature_XOP */ /* AMD-specific */
} };

/* Initialize ragel state before we try to decode anything. */
static void RagelSetup() {
}

struct RagelDecodeState {
  const uint8_t *inst_offset;
  uint8_t inst_num_bytes;
  uint8_t valid_state;  /* indicates if this struct describes an instruction */
  const char *inst_name;
  int inst_is_legal;
};
struct RagelDecodeState RState;

void RagelPrintInst() {
  int i;

  for (i = 0; i < RState.inst_num_bytes; i++) {
    printf("%02x ", RState.inst_offset[i]);
  }
  printf(": %s\n", RState.inst_name);
}

void RagelDecodeError (const uint8_t *ptr, void *userdata) {
  UNREFERENCED_PARAMETER(ptr);
  UNREFERENCED_PARAMETER(userdata);
  return;
  printf("DFA error in decoder: ");
  RagelPrintInst();
}

void RagelValidateError (const uint8_t *ptr, int error, void *userdata) {
  UNREFERENCED_PARAMETER(ptr);
  if ((error & UNRECOGNIZED_INSTRUCTION) ||
      (error & CPUID_UNSUPPORTED_INSTRUCTION)) {
    ((struct RagelDecodeState*)userdata)->inst_is_legal = 0;
  }
  return;
}

void RagelInstruction(const uint8_t *begin, const uint8_t *end,
                      struct instruction *instruction, void *userdata) {
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

  /* Decode again, this time specifying length of first instruction. */
  res = DecodeChunkArch(enumerator->_itext, tempstate.inst_num_bytes,
                        RagelInstruction, RagelDecodeError, &RState);

  RState.inst_is_legal = res;
  if (res) {
    uint8_t chunk[(NACL_ENUM_MAX_INSTRUCTION_BYTES + kBundleMask) &
                  ~kBundleMask];

    /* Copy the command.  */
    memcpy(chunk, enumerator->_itext, enumerator->_num_bytes);
    /* Fill the rest with HLTs.  */
    memset(chunk + enumerator->_num_bytes, 0xf4,
           sizeof(chunk) - enumerator->_num_bytes);
    ValidateChunkArch(chunk, sizeof(chunk), &old_validator_features,
                      RagelValidateError, &RState);
  }

#undef DecodeChunkArch
#undef ValidateChunkArch
}

/* Returns true if the instruction parsed a legal instruction. */
static Bool RIsInstLegal(const NaClEnumerator* enumerator) {
  UNREFERENCED_PARAMETER(enumerator);
  return RState.inst_is_legal;
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
NaClEnumeratorDecoder* RegisterRagelDecoder() {
  RagelSetup();
  ragel_decoder._base._id_name = "ragel";
  ragel_decoder._base._parse_inst_fn = RParseInst;
  ragel_decoder._base._inst_length_fn = RInstLength;
  ragel_decoder._base._print_inst_fn = RPrintInst;
  ragel_decoder._base._is_inst_legal_fn = RIsInstLegal;
  ragel_decoder._base._install_flag_fn = InstallFlag;
  ragel_decoder._base._get_inst_mnemonic_fn = NULL;
  ragel_decoder._base._get_inst_num_operands_fn = NULL;
  ragel_decoder._base._get_inst_operands_text_fn = NULL;
  ragel_decoder._base._writes_to_reserved_reg_fn = NULL;
  ragel_decoder._base._maybe_inst_validates_fn = NULL;
  ragel_decoder._base._segment_validates_fn = NULL;
  ragel_decoder._base._usage_message = "Runs ragel to decode instructions.";
  return &ragel_decoder._base;
}
