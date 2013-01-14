/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCVALIDATE_INTERNALTYPES_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCVALIDATE_INTERNALTYPES_H__

/*
 * ncvalidate_internaltypes.h
 * Declarations intimate to ncvalidate.h, exposed for testing and other files
 * that define the validator.
 *
 */
#include "native_client/src/trusted/cpu_features/arch/x86/cpu_x86.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode.h"

/* statistics */
typedef struct SummaryStats {
  /* these are just information */
  uint32_t instructions;
  uint32_t checktarget;
  uint32_t targetindirect;
  uint32_t segments;
  /* the following indicate safety defects */
  uint32_t badtarget;
  uint32_t unsafeindirect;
  uint32_t returns;
  uint32_t illegalinst;
  uint32_t badalignment;
  uint32_t segfaults;
  uint32_t badprefix;
  uint32_t badinstlength;
  uint32_t internalerrors;
  int didstubout; /* boolean */
  int sawfailure; /* boolean */
} SummaryStats;

/* We track instructions in a three-entry circular buffer,
 * allowing us to see the two previous instructions and to
 * check the safe call sequence. I rounded up to
 * four so we can use a mask, even though we only need to
 * remember three instructions.
 * This is #defined rather than const int because it is used
 * as an array dimension
 */
#define kNCValidatorInstBufferSize 4

/* Defines a jump summarization function. When in sel_ldr, this will
 * be the minimal code needed to detect issues. When in ncval, this
 * will expend more effort and generate more readable error messages.
 */
typedef void (*NCValidateJumpSummarizeFn)(struct NCValidatorState* vstate);

/* put all formerly global data into a struct */
typedef struct NCValidatorState {
  /* NOTE: Decoder state (dstate) must appear first so that we can use it like
   * C++ inheritance, where a pointer to a validator state will be the
   * same as a pointer to a decoder state.
   */
  NCDecoderState dstate;
  NCDecoderInst inst_buffer[kNCValidatorInstBufferSize];
  NaClCPUFeaturesX86 cpufeatures;  /* from CPUID bit masks; see cpu_x86.c */
  NaClPcAddress iadrbase;
  NaClMemorySize codesize;
  uint8_t bundle_size;
  uint32_t bundle_mask;
  SummaryStats stats;
  uint32_t opcodehisto[256];
  uint8_t *vttable;
  uint8_t *kttable;
  /* If non-null, then in detailed mode. Keeps track of addresses
   * to instructions in the middle of a NaCl (atomic) pattern.
   * This allows detailed mode to give better error messages (i.e.
   * whether the jump isn't to an instruction boundary,
   * or if the jump is into the middle of a nacl pattern).
   */
  uint8_t *pattern_nonfirst_insts_table;
  int do_stub_out;  /* boolean */
  int readonly_text; /* boolean */
  int num_diagnostics; /* How many error messages to print. */
  /* Defines the summarization function to apply. Defaults to
   * NCSelLDrJumpSummarizeFn, which is the summarize function
   * for sel_ldr (i.e. non-detailed).
   */
  NCValidateJumpSummarizeFn summarize_fn;
} NCValidatorState;

/* The following macro is used to clarify the derived class relationship
 * of NCValidateState and NCDecoderState. That is, &this->dstate is also
 * an instance of a validator state. Hence one can downcast this pointer.
 */
#define NCVALIDATOR_STATE_DOWNCAST(this_dstate) \
  ((NCValidatorState*) (this_dstate))

/* Masks used to access bits within a byte. */
extern const uint8_t nc_iadrmasks[8];

/* Converts address to corresponding byte in jump table. */
#define NCIATOffset(__IA) ((__IA) >> 3)

/* Gets mask for bit associated with corresponding byte in jump table. */
#define NCIATMask(__IA) (nc_iadrmasks[(__IA) & 0x7])

/* Sets bit __IOFF in jump table __TABLE. */
#define NCSetAdrTable(__IOFF, __TABLE) \
  (__TABLE)[NCIATOffset(__IOFF)] |= NCIATMask(__IOFF)

/* Clears bit __IOFF in jump table __TABLE. */
#define NCClearAdrTable(__IOFF, __TABLE) \
  (__TABLE)[NCIATOffset(__IOFF)] &= ~(NCIATMask(__IOFF))

/* Gets bit __IOFF in jump table __TABLE. */
#define NCGetAdrTable(__IOFF, __TABLE) \
  ((__TABLE)[NCIATOffset(__IOFF)] & NCIATMask(__IOFF))

/* Report that the given instruction is illegal in native client, using
 * the given error message.
 */
void NCBadInstructionError(const struct NCDecoderInst *dinst, const char *msg);

/* Update statistics to show that another bad jump target was found. */
void NCStatsBadTarget(struct NCValidatorState *vstate);

/* Update statistics to show that another bad address alignment issues has been
 * found.
 */
void NCStatsBadAlignment(struct NCValidatorState *vstate);

/* Update statistics to show that some (unexpected) internal error occurred
 * while running the validator.
 */
void NCStatsInternalError(struct NCValidatorState *vstate);

/* Provide a partial-validation operation, checking a single instruction
 * but ignoring inter-instruction considerations, useful for validator
 * testing.
 */
Bool UnsafePartialValidateInst(const NCDecoderInst *dinst);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_SEG_SFI_NCVALIDATE_INTERNALTYPES_H__ */
