/* native_client/src/trusted/validator_ragel/gen/validator_x86_32.c
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
 * Compiled for ia32 mode.
 */

/*
 * This is the core of ia32-mode validator.  Please note that this file
 * combines ragel machine description and C language actions.  Please read
 * validator_internals.html first to understand how the whole thing is built:
 * it explains how the byte sequences are constructed, what constructs like
 * “@{}” or “REX_WRX?” mean, etc.
 */

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/validator_ragel/bitmap.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator_internal.h"

/* Ignore this information: it's not used by security model in IA32 mode.  */
#undef GET_VEX_PREFIX3
#define GET_VEX_PREFIX3 0
#undef SET_VEX_PREFIX3
#define SET_VEX_PREFIX3(P)





static const int x86_32_validator_start = 242;
static const int x86_32_validator_first_final = 242;
static const int x86_32_validator_error = 0;

static const int x86_32_validator_en_main = 242;




Bool ValidateChunkIA32(const uint8_t *data, size_t size,
                       uint32_t options,
                       const NaClCPUFeaturesX86 *cpu_features,
                       ValidationCallbackFunc user_callback,
                       void *callback_data) {
  bitmap_word valid_targets_small[2];
  bitmap_word jump_dests_small[2];
  bitmap_word *valid_targets;
  bitmap_word *jump_dests;
  const uint8_t *current_position;
  const uint8_t *end_of_bundle;
  int result = TRUE;

  CHECK(sizeof valid_targets_small == sizeof jump_dests_small);
  CHECK(size % kBundleSize == 0);

  /*
   * For a very small sequences (one bundle) malloc is too expensive.
   *
   * Note1: we allocate one extra bit, because we set valid jump target bits
   * _after_ instructions, so there will be one at the end of the chunk.
   *
   * Note2: we don't ever mark first bit as a valid jump target but this is
   * not a problem because any aligned address is valid jump target.
   */
  if ((size + 1) <= (sizeof valid_targets_small * 8)) {
    memset(valid_targets_small, 0, sizeof valid_targets_small);
    valid_targets = valid_targets_small;
    memset(jump_dests_small, 0, sizeof jump_dests_small);
    jump_dests = jump_dests_small;
  } else {
    valid_targets = BitmapAllocate(size + 1);
    jump_dests = BitmapAllocate(size + 1);
    if (!valid_targets || !jump_dests) {
      free(jump_dests);
      free(valid_targets);
      errno = ENOMEM;
      return FALSE;
    }
  }

  /*
   * This option is usually used in tests: we will process the whole chunk
   * in one pass. Usually each bundle is processed separately which means
   * instructions (and super-instructions) can not cross borders of the bundle.
   */
  if (options & PROCESS_CHUNK_AS_A_CONTIGUOUS_STREAM)
    end_of_bundle = data + size;
  else
    end_of_bundle = data + kBundleSize;

  /*
   * Main loop.  Here we process the data array bundle-after-bundle.
   * Ragel-produced DFA does all the checks with one exception: direct jumps.
   * It collects the two arrays: valid_targets and jump_dests which are used
   * to test direct jumps later.
   */
  for (current_position = data;
       current_position < data + size;
       current_position = end_of_bundle,
       end_of_bundle = current_position + kBundleSize) {
    /* Start of the instruction being processed.  */
    const uint8_t *instruction_start = current_position;
    uint32_t instruction_info_collected = 0;
    int current_state;

    
	{
	( current_state) = x86_32_validator_start;
	}

    
	{
	if ( ( current_position) == ( end_of_bundle) )
		goto _test_eof;
	switch ( ( current_state) )
	{
tr0:
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr9:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr10:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr11:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr15:
	{
    SET_IMM_TYPE(IMM32);
    SET_IMM_PTR(current_position - 3);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr19:
	{ SET_CPU_FEATURE(CPUFeature_3DNOW);     }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr27:
	{ SET_CPU_FEATURE(CPUFeature_TSC);       }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr36:
	{ SET_CPU_FEATURE(CPUFeature_MMX);       }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr50:
	{ SET_CPU_FEATURE(CPUFeature_MON);       }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr51:
	{ SET_CPU_FEATURE(CPUFeature_FXSR);      }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr52:
	{ SET_CPU_FEATURE(CPUFeature_3DPRFTCH);  }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr64:
	{
    instruction_info_collected |= LAST_BYTE_IS_NOT_IMMEDIATE;
  }
	{ SET_CPU_FEATURE(CPUFeature_E3DNOW);    }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr65:
	{
    instruction_info_collected |= LAST_BYTE_IS_NOT_IMMEDIATE;
  }
	{ SET_CPU_FEATURE(CPUFeature_3DNOW);     }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr71:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr95:
	{
    Rel32Operand(current_position + 1, data, jump_dests, size,
                 &instruction_info_collected);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr98:
	{ SET_CPU_FEATURE(CPUFeature_CLFLUSH);   }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr107:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr108:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr115:
	{ SET_CPU_FEATURE(CPUFeature_CX8);       }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr123:
	{
    Rel8Operand(current_position + 1, data, jump_dests, size,
                &instruction_info_collected);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr144:
	{
    SET_IMM_TYPE(IMM16);
    SET_IMM_PTR(current_position - 1);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr161:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr246:
	{
    instruction_info_collected |= LAST_BYTE_IS_NOT_IMMEDIATE;
  }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr259:
	{ SET_CPU_FEATURE(CPUFeature_TBM);       }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr266:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr300:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr327:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr353:
	{
    SET_IMM_TYPE(IMM2);
    SET_IMM_PTR(current_position);
  }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr377:
	{ SET_CPU_FEATURE(CPUFeature_x87);       }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr383:
	{ SET_CPU_FEATURE(CPUFeature_CMOVx87);   }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr387:
	{
    Rel32Operand(current_position + 1, data, jump_dests, size,
                 &instruction_info_collected);
  }
	{}
	{
      if (((current_position - data) & kBundleMask) != kBundleMask)
        instruction_info_collected |= BAD_CALL_ALIGNMENT;
    }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr404:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr415:
	{
      UnmarkValidJumpTarget((current_position - data) - 1, valid_targets);
      instruction_start -= 3;
      instruction_info_collected |= SPECIAL_INSTRUCTION;
    }
	{
      if (((current_position - data) & kBundleMask) != kBundleMask)
        instruction_info_collected |= BAD_CALL_ALIGNMENT;
    }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
tr416:
	{
      UnmarkValidJumpTarget((current_position - data) - 1, valid_targets);
      instruction_start -= 3;
      instruction_info_collected |= SPECIAL_INSTRUCTION;
    }
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st242;
st242:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof242;
case 242:
	switch( (*( current_position)) ) {
		case 4u: goto st10;
		case 5u: goto st11;
		case 12u: goto st10;
		case 13u: goto st11;
		case 15u: goto st15;
		case 20u: goto st10;
		case 21u: goto st11;
		case 28u: goto st10;
		case 29u: goto st11;
		case 36u: goto st10;
		case 37u: goto st11;
		case 44u: goto st10;
		case 45u: goto st11;
		case 46u: goto st54;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st54;
		case 101u: goto st57;
		case 102u: goto st63;
		case 104u: goto st11;
		case 105u: goto st110;
		case 106u: goto st10;
		case 107u: goto st34;
		case 128u: goto st34;
		case 129u: goto st110;
		case 131u: goto st119;
		case 141u: goto st29;
		case 143u: goto st121;
		case 155u: goto tr377;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st149;
		case 197u: goto st199;
		case 198u: goto st200;
		case 199u: goto st201;
		case 201u: goto tr0;
		case 216u: goto st202;
		case 217u: goto st203;
		case 218u: goto st204;
		case 219u: goto st205;
		case 220u: goto st206;
		case 221u: goto st207;
		case 222u: goto st208;
		case 223u: goto st209;
		case 232u: goto st210;
		case 233u: goto st46;
		case 235u: goto st56;
		case 240u: goto st214;
		case 242u: goto st217;
		case 243u: goto st222;
		case 246u: goto st224;
		case 247u: goto st225;
		case 254u: goto st226;
		case 255u: goto st109;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) <= 3u )
					goto st1;
			} else if ( (*( current_position)) > 11u ) {
				if ( (*( current_position)) > 19u ) {
					if ( 24u <= (*( current_position)) && (*( current_position)) <= 27u )
						goto st1;
				} else if ( (*( current_position)) >= 16u )
					goto st1;
			} else
				goto st1;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 56u ) {
				if ( (*( current_position)) > 43u ) {
					if ( 48u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto st1;
				} else if ( (*( current_position)) >= 40u )
					goto st1;
			} else if ( (*( current_position)) > 59u ) {
				if ( (*( current_position)) > 95u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto st56;
				} else if ( (*( current_position)) >= 64u )
					goto tr0;
			} else
				goto st1;
		} else
			goto st1;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 176u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 153u ) {
					if ( 158u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto tr0;
				} else if ( (*( current_position)) >= 144u )
					goto tr0;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) > 171u ) {
					if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
						goto tr0;
				} else if ( (*( current_position)) >= 164u )
					goto tr0;
			} else
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto st96;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st98;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
tr21:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st1;
tr26:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st1;
tr30:
	{ SET_CPU_FEATURE(CPUFeature_CMOV);      }
	goto st1;
tr32:
	{ SET_CPU_FEATURE(CPUFeature_MMX);       }
	goto st1;
tr47:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st1;
tr77:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st1;
tr145:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st1;
tr155:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE3);      }
	goto st1;
tr164:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st1;
tr165:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st1;
tr167:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st1;
tr168:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st1;
tr210:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st1;
tr258:
	{ SET_CPU_FEATURE(CPUFeature_XOP);       }
	goto st1;
tr325:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st1;
tr297:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st1;
tr333:
	{ SET_CPU_FEATURE(CPUFeature_FMA);       }
	goto st1;
tr334:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st1;
tr335:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st1;
tr391:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st1;
tr392:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE3);      }
	goto st1;
tr398:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st1;
tr405:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st1;
tr406:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE3);      }
	goto st1;
tr408:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st1;
tr410:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_POPCNT);    }
	goto st1;
tr411:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_TZCNT);     }
	goto st1;
tr412:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_LZCNT);     }
	goto st1;
st1:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof1;
case 1:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st3;
	} else if ( (*( current_position)) >= 64u )
		goto st7;
	goto tr0;
tr72:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st2;
tr53:
	{ SET_CPU_FEATURE(CPUFeature_3DPRFTCH);  }
	goto st2;
tr96:
	{ SET_CPU_FEATURE(CPUFeature_FXSR);      }
	goto st2;
tr99:
	{ SET_CPU_FEATURE(CPUFeature_CLFLUSH);   }
	goto st2;
tr116:
	{ SET_CPU_FEATURE(CPUFeature_CX8);       }
	goto st2;
tr260:
	{ SET_CPU_FEATURE(CPUFeature_TBM);       }
	goto st2;
tr328:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st2;
tr301:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st2;
tr378:
	{ SET_CPU_FEATURE(CPUFeature_x87);       }
	goto st2;
st2:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof2;
case 2:
	switch( (*( current_position)) ) {
		case 5u: goto st3;
		case 13u: goto st3;
		case 21u: goto st3;
		case 29u: goto st3;
		case 37u: goto st3;
		case 45u: goto st3;
		case 53u: goto st3;
		case 61u: goto st3;
		case 69u: goto st3;
		case 77u: goto st3;
		case 85u: goto st3;
		case 93u: goto st3;
		case 101u: goto st3;
		case 109u: goto st3;
		case 117u: goto st3;
		case 125u: goto st3;
		case 133u: goto st3;
		case 141u: goto st3;
		case 149u: goto st3;
		case 157u: goto st3;
		case 165u: goto st3;
		case 173u: goto st3;
		case 181u: goto st3;
		case 189u: goto st3;
		case 197u: goto st3;
		case 205u: goto st3;
		case 213u: goto st3;
		case 221u: goto st3;
		case 229u: goto st3;
		case 237u: goto st3;
		case 245u: goto st3;
		case 253u: goto st3;
	}
	goto tr0;
tr73:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st3;
tr54:
	{ SET_CPU_FEATURE(CPUFeature_3DPRFTCH);  }
	goto st3;
tr97:
	{ SET_CPU_FEATURE(CPUFeature_FXSR);      }
	goto st3;
tr100:
	{ SET_CPU_FEATURE(CPUFeature_CLFLUSH);   }
	goto st3;
tr117:
	{ SET_CPU_FEATURE(CPUFeature_CX8);       }
	goto st3;
tr261:
	{ SET_CPU_FEATURE(CPUFeature_TBM);       }
	goto st3;
tr329:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st3;
tr302:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st3;
tr379:
	{ SET_CPU_FEATURE(CPUFeature_x87);       }
	goto st3;
st3:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof3;
case 3:
	goto tr6;
tr6:
	{}
	goto st4;
st4:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof4;
case 4:
	goto tr7;
tr7:
	{}
	goto st5;
st5:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof5;
case 5:
	goto tr8;
tr8:
	{}
	goto st6;
st6:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof6;
case 6:
	goto tr9;
tr74:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st7;
tr55:
	{ SET_CPU_FEATURE(CPUFeature_3DPRFTCH);  }
	goto st7;
tr101:
	{ SET_CPU_FEATURE(CPUFeature_FXSR);      }
	goto st7;
tr103:
	{ SET_CPU_FEATURE(CPUFeature_CLFLUSH);   }
	goto st7;
tr118:
	{ SET_CPU_FEATURE(CPUFeature_CX8);       }
	goto st7;
tr262:
	{ SET_CPU_FEATURE(CPUFeature_TBM);       }
	goto st7;
tr330:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st7;
tr303:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st7;
tr380:
	{ SET_CPU_FEATURE(CPUFeature_x87);       }
	goto st7;
st7:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof7;
case 7:
	goto tr10;
tr75:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st8;
tr56:
	{ SET_CPU_FEATURE(CPUFeature_3DPRFTCH);  }
	goto st8;
tr102:
	{ SET_CPU_FEATURE(CPUFeature_FXSR);      }
	goto st8;
tr104:
	{ SET_CPU_FEATURE(CPUFeature_CLFLUSH);   }
	goto st8;
tr119:
	{ SET_CPU_FEATURE(CPUFeature_CX8);       }
	goto st8;
tr263:
	{ SET_CPU_FEATURE(CPUFeature_TBM);       }
	goto st8;
tr331:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st8;
tr304:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st8;
tr381:
	{ SET_CPU_FEATURE(CPUFeature_x87);       }
	goto st8;
st8:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof8;
case 8:
	goto st7;
tr76:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st9;
tr57:
	{ SET_CPU_FEATURE(CPUFeature_3DPRFTCH);  }
	goto st9;
tr105:
	{ SET_CPU_FEATURE(CPUFeature_FXSR);      }
	goto st9;
tr106:
	{ SET_CPU_FEATURE(CPUFeature_CLFLUSH);   }
	goto st9;
tr120:
	{ SET_CPU_FEATURE(CPUFeature_CX8);       }
	goto st9;
tr264:
	{ SET_CPU_FEATURE(CPUFeature_TBM);       }
	goto st9;
tr332:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st9;
tr305:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st9;
tr382:
	{ SET_CPU_FEATURE(CPUFeature_x87);       }
	goto st9;
st9:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof9;
case 9:
	goto st3;
tr91:
	{ SET_CPU_FEATURE(CPUFeature_MMX);       }
	goto st10;
tr109:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st10;
tr89:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	goto st10;
tr90:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{}
	goto st10;
tr181:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st10;
tr175:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st10;
tr182:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st10;
tr308:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st10;
tr402:
	{
    SET_IMM2_TYPE(IMM8);
    SET_IMM2_PTR(current_position);
  }
	{}
	goto st10;
st10:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof10;
case 10:
	goto tr11;
tr220:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	goto st11;
tr221:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{}
	goto st11;
tr270:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	goto st11;
st11:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof11;
case 11:
	goto tr12;
tr12:
	{}
	goto st12;
st12:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof12;
case 12:
	goto tr13;
tr13:
	{}
	goto st13;
st13:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof13;
case 13:
	goto tr14;
tr14:
	{}
	goto st14;
st14:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof14;
case 14:
	goto tr15;
tr16:
	{
    result &= user_callback(instruction_start, current_position,
                            UNRECOGNIZED_INSTRUCTION, callback_data);
    /*
     * Process the next bundle: “continue” here is for the “for” cycle in
     * the ValidateChunkIA32 function.
     *
     * It does not affect the case which we really care about (when code
     * is validatable), but makes it possible to detect more errors in one
     * run in tools like ncval.
     */
    continue;
  }
	goto st0;
st0:
( current_state) = 0;
	goto _out;
st15:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof15;
case 15:
	switch( (*( current_position)) ) {
		case 1u: goto st16;
		case 11u: goto tr0;
		case 13u: goto st17;
		case 14u: goto tr19;
		case 15u: goto st18;
		case 18u: goto st28;
		case 19u: goto tr23;
		case 22u: goto st28;
		case 23u: goto tr23;
		case 24u: goto st30;
		case 31u: goto st31;
		case 43u: goto tr23;
		case 49u: goto tr27;
		case 56u: goto st32;
		case 58u: goto st33;
		case 80u: goto tr31;
		case 112u: goto tr33;
		case 115u: goto st45;
		case 119u: goto tr36;
		case 162u: goto tr0;
		case 164u: goto st34;
		case 165u: goto st1;
		case 172u: goto st34;
		case 174u: goto st50;
		case 195u: goto tr42;
		case 196u: goto st51;
		case 197u: goto tr44;
		case 199u: goto st53;
		case 212u: goto tr26;
		case 215u: goto tr46;
		case 218u: goto tr47;
		case 222u: goto tr47;
		case 224u: goto tr47;
		case 229u: goto tr32;
		case 231u: goto tr48;
		case 234u: goto tr47;
		case 238u: goto tr47;
		case 244u: goto tr26;
		case 246u: goto tr47;
		case 247u: goto tr49;
		case 251u: goto tr26;
	}
	if ( (*( current_position)) < 126u ) {
		if ( (*( current_position)) < 81u ) {
			if ( (*( current_position)) < 42u ) {
				if ( (*( current_position)) > 21u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 41u )
						goto tr21;
				} else if ( (*( current_position)) >= 16u )
					goto tr21;
			} else if ( (*( current_position)) > 45u ) {
				if ( (*( current_position)) > 47u ) {
					if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
						goto tr30;
				} else if ( (*( current_position)) >= 46u )
					goto tr21;
			} else
				goto tr26;
		} else if ( (*( current_position)) > 89u ) {
			if ( (*( current_position)) < 96u ) {
				if ( (*( current_position)) > 91u ) {
					if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
						goto tr21;
				} else if ( (*( current_position)) >= 90u )
					goto tr26;
			} else if ( (*( current_position)) > 107u ) {
				if ( (*( current_position)) < 113u ) {
					if ( 110u <= (*( current_position)) && (*( current_position)) <= 111u )
						goto tr32;
				} else if ( (*( current_position)) > 114u ) {
					if ( 116u <= (*( current_position)) && (*( current_position)) <= 118u )
						goto tr32;
				} else
					goto st44;
			} else
				goto tr32;
		} else
			goto tr21;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 173u ) {
				if ( (*( current_position)) > 143u ) {
					if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto tr30;
				} else if ( (*( current_position)) >= 128u )
					goto st46;
			} else if ( (*( current_position)) > 177u ) {
				if ( (*( current_position)) > 183u ) {
					if ( 188u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto st1;
				} else if ( (*( current_position)) >= 182u )
					goto st1;
			} else
				goto st1;
		} else if ( (*( current_position)) > 198u ) {
			if ( (*( current_position)) < 216u ) {
				if ( (*( current_position)) > 207u ) {
					if ( 209u <= (*( current_position)) && (*( current_position)) <= 213u )
						goto tr32;
				} else if ( (*( current_position)) >= 200u )
					goto tr0;
			} else if ( (*( current_position)) > 226u ) {
				if ( (*( current_position)) < 232u ) {
					if ( 227u <= (*( current_position)) && (*( current_position)) <= 228u )
						goto tr47;
				} else if ( (*( current_position)) > 239u ) {
					if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
						goto tr32;
				} else
					goto tr32;
			} else
				goto tr32;
		} else
			goto tr41;
	} else
		goto tr32;
	goto tr16;
st16:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof16;
case 16:
	if ( (*( current_position)) == 208u )
		goto tr51;
	if ( 200u <= (*( current_position)) && (*( current_position)) <= 201u )
		goto tr50;
	goto tr16;
st17:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof17;
case 17:
	switch( (*( current_position)) ) {
		case 4u: goto tr53;
		case 5u: goto tr54;
		case 12u: goto tr53;
		case 13u: goto tr54;
		case 68u: goto tr56;
		case 76u: goto tr56;
		case 132u: goto tr57;
		case 140u: goto tr57;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr52;
	} else if ( (*( current_position)) > 79u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
			goto tr54;
	} else
		goto tr55;
	goto tr16;
st18:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof18;
case 18:
	switch( (*( current_position)) ) {
		case 4u: goto st20;
		case 5u: goto st21;
		case 12u: goto st20;
		case 13u: goto st21;
		case 20u: goto st20;
		case 21u: goto st21;
		case 28u: goto st20;
		case 29u: goto st21;
		case 36u: goto st20;
		case 37u: goto st21;
		case 44u: goto st20;
		case 45u: goto st21;
		case 52u: goto st20;
		case 53u: goto st21;
		case 60u: goto st20;
		case 61u: goto st21;
		case 68u: goto st26;
		case 76u: goto st26;
		case 84u: goto st26;
		case 92u: goto st26;
		case 100u: goto st26;
		case 108u: goto st26;
		case 116u: goto st26;
		case 124u: goto st26;
		case 132u: goto st27;
		case 140u: goto st27;
		case 148u: goto st27;
		case 156u: goto st27;
		case 164u: goto st27;
		case 172u: goto st27;
		case 180u: goto st27;
		case 188u: goto st27;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st21;
	} else if ( (*( current_position)) >= 64u )
		goto st25;
	goto st19;
tr69:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	goto st19;
tr70:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{}
	goto st19;
st19:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof19;
case 19:
	switch( (*( current_position)) ) {
		case 12u: goto tr64;
		case 13u: goto tr65;
		case 28u: goto tr64;
		case 29u: goto tr65;
		case 138u: goto tr64;
		case 142u: goto tr64;
		case 144u: goto tr65;
		case 148u: goto tr65;
		case 154u: goto tr65;
		case 158u: goto tr65;
		case 160u: goto tr65;
		case 164u: goto tr65;
		case 170u: goto tr65;
		case 174u: goto tr65;
		case 176u: goto tr65;
		case 180u: goto tr65;
		case 187u: goto tr64;
		case 191u: goto tr65;
	}
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 151u )
			goto tr65;
	} else if ( (*( current_position)) > 167u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto tr65;
	} else
		goto tr65;
	goto tr16;
st20:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof20;
case 20:
	switch( (*( current_position)) ) {
		case 5u: goto st21;
		case 13u: goto st21;
		case 21u: goto st21;
		case 29u: goto st21;
		case 37u: goto st21;
		case 45u: goto st21;
		case 53u: goto st21;
		case 61u: goto st21;
		case 69u: goto st21;
		case 77u: goto st21;
		case 85u: goto st21;
		case 93u: goto st21;
		case 101u: goto st21;
		case 109u: goto st21;
		case 117u: goto st21;
		case 125u: goto st21;
		case 133u: goto st21;
		case 141u: goto st21;
		case 149u: goto st21;
		case 157u: goto st21;
		case 165u: goto st21;
		case 173u: goto st21;
		case 181u: goto st21;
		case 189u: goto st21;
		case 197u: goto st21;
		case 205u: goto st21;
		case 213u: goto st21;
		case 221u: goto st21;
		case 229u: goto st21;
		case 237u: goto st21;
		case 245u: goto st21;
		case 253u: goto st21;
	}
	goto st19;
st21:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof21;
case 21:
	goto tr66;
tr66:
	{}
	goto st22;
st22:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof22;
case 22:
	goto tr67;
tr67:
	{}
	goto st23;
st23:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof23;
case 23:
	goto tr68;
tr68:
	{}
	goto st24;
st24:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof24;
case 24:
	goto tr69;
st25:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof25;
case 25:
	goto tr70;
st26:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof26;
case 26:
	goto st25;
st27:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof27;
case 27:
	goto st21;
st28:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof28;
case 28:
	switch( (*( current_position)) ) {
		case 4u: goto tr72;
		case 5u: goto tr73;
		case 12u: goto tr72;
		case 13u: goto tr73;
		case 20u: goto tr72;
		case 21u: goto tr73;
		case 28u: goto tr72;
		case 29u: goto tr73;
		case 36u: goto tr72;
		case 37u: goto tr73;
		case 44u: goto tr72;
		case 45u: goto tr73;
		case 52u: goto tr72;
		case 53u: goto tr73;
		case 60u: goto tr72;
		case 61u: goto tr73;
		case 68u: goto tr75;
		case 76u: goto tr75;
		case 84u: goto tr75;
		case 92u: goto tr75;
		case 100u: goto tr75;
		case 108u: goto tr75;
		case 116u: goto tr75;
		case 124u: goto tr75;
		case 132u: goto tr76;
		case 140u: goto tr76;
		case 148u: goto tr76;
		case 156u: goto tr76;
		case 164u: goto tr76;
		case 172u: goto tr76;
		case 180u: goto tr76;
		case 188u: goto tr76;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr73;
	} else if ( (*( current_position)) >= 64u )
		goto tr74;
	goto tr71;
tr23:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st29;
tr42:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st29;
tr48:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st29;
tr78:
	{ SET_CPU_FEATURE(CPUFeature_MOVBE);     }
	goto st29;
tr146:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st29;
tr166:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st29;
tr298:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st29;
tr400:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE3);      }
	goto st29;
tr393:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE4A);     }
	goto st29;
tr407:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE4A);     }
	goto st29;
st29:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof29;
case 29:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 12u: goto st2;
		case 20u: goto st2;
		case 28u: goto st2;
		case 36u: goto st2;
		case 44u: goto st2;
		case 52u: goto st2;
		case 60u: goto st2;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr0;
			} else
				goto tr0;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr0;
			} else if ( (*( current_position)) >= 22u )
				goto tr0;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr0;
			} else if ( (*( current_position)) >= 46u )
				goto tr0;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 127u ) {
				if ( 192u <= (*( current_position)) )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto st7;
		} else
			goto tr0;
	} else
		goto tr0;
	goto st3;
st30:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof30;
case 30:
	switch( (*( current_position)) ) {
		case 4u: goto tr72;
		case 5u: goto tr73;
		case 12u: goto tr72;
		case 13u: goto tr73;
		case 20u: goto tr72;
		case 21u: goto tr73;
		case 28u: goto tr72;
		case 29u: goto tr73;
		case 68u: goto tr75;
		case 76u: goto tr75;
		case 84u: goto tr75;
		case 92u: goto tr75;
		case 132u: goto tr76;
		case 140u: goto tr76;
		case 148u: goto tr76;
		case 156u: goto tr76;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 31u )
			goto tr71;
	} else if ( (*( current_position)) > 95u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto tr73;
	} else
		goto tr74;
	goto tr16;
st31:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof31;
case 31:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 68u: goto st8;
		case 132u: goto st9;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto tr0;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto tr0;
		} else if ( (*( current_position)) >= 128u )
			goto st3;
	} else
		goto st7;
	goto tr16;
st32:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof32;
case 32:
	if ( (*( current_position)) == 4u )
		goto tr32;
	if ( (*( current_position)) < 28u ) {
		if ( (*( current_position)) <= 11u )
			goto tr77;
	} else if ( (*( current_position)) > 30u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 241u )
			goto tr78;
	} else
		goto tr77;
	goto tr16;
st33:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof33;
case 33:
	if ( (*( current_position)) == 15u )
		goto tr79;
	goto tr16;
tr41:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st34;
tr33:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st34;
tr79:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st34;
tr151:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st34;
tr170:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st34;
tr169:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st34;
tr173:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st34;
tr174:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st34;
tr172:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_CLMUL);     }
	goto st34;
tr239:
	{ SET_CPU_FEATURE(CPUFeature_XOP);       }
	goto st34;
tr299:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st34;
tr346:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st34;
tr360:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st34;
tr342:
	{ SET_CPU_FEATURE(CPUFeature_CLMULAVX);  }
	goto st34;
tr395:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st34;
tr413:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st34;
tr409:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st34;
st34:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof34;
case 34:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 12u: goto st35;
		case 13u: goto st36;
		case 20u: goto st35;
		case 21u: goto st36;
		case 28u: goto st35;
		case 29u: goto st36;
		case 36u: goto st35;
		case 37u: goto st36;
		case 44u: goto st35;
		case 45u: goto st36;
		case 52u: goto st35;
		case 53u: goto st36;
		case 60u: goto st35;
		case 61u: goto st36;
		case 68u: goto st41;
		case 76u: goto st41;
		case 84u: goto st41;
		case 92u: goto st41;
		case 100u: goto st41;
		case 108u: goto st41;
		case 116u: goto st41;
		case 124u: goto st41;
		case 132u: goto st42;
		case 140u: goto st42;
		case 148u: goto st42;
		case 156u: goto st42;
		case 164u: goto st42;
		case 172u: goto st42;
		case 180u: goto st42;
		case 188u: goto st42;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st36;
	} else if ( (*( current_position)) >= 64u )
		goto st40;
	goto st10;
tr110:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st35;
tr176:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st35;
tr183:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st35;
tr313:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st35;
st35:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof35;
case 35:
	switch( (*( current_position)) ) {
		case 5u: goto st36;
		case 13u: goto st36;
		case 21u: goto st36;
		case 29u: goto st36;
		case 37u: goto st36;
		case 45u: goto st36;
		case 53u: goto st36;
		case 61u: goto st36;
		case 69u: goto st36;
		case 77u: goto st36;
		case 85u: goto st36;
		case 93u: goto st36;
		case 101u: goto st36;
		case 109u: goto st36;
		case 117u: goto st36;
		case 125u: goto st36;
		case 133u: goto st36;
		case 141u: goto st36;
		case 149u: goto st36;
		case 157u: goto st36;
		case 165u: goto st36;
		case 173u: goto st36;
		case 181u: goto st36;
		case 189u: goto st36;
		case 197u: goto st36;
		case 205u: goto st36;
		case 213u: goto st36;
		case 221u: goto st36;
		case 229u: goto st36;
		case 237u: goto st36;
		case 245u: goto st36;
		case 253u: goto st36;
	}
	goto st10;
tr111:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st36;
tr177:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st36;
tr184:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st36;
tr314:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st36;
st36:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof36;
case 36:
	goto tr86;
tr86:
	{}
	goto st37;
st37:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof37;
case 37:
	goto tr87;
tr87:
	{}
	goto st38;
st38:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof38;
case 38:
	goto tr88;
tr88:
	{}
	goto st39;
st39:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof39;
case 39:
	goto tr89;
tr112:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st40;
tr178:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st40;
tr185:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st40;
tr315:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st40;
st40:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof40;
case 40:
	goto tr90;
tr113:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st41;
tr179:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st41;
tr186:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st41;
tr316:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st41;
st41:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof41;
case 41:
	goto st40;
tr114:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st42;
tr180:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st42;
tr187:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st42;
tr317:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st42;
st42:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof42;
case 42:
	goto st36;
tr31:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st43;
tr46:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st43;
tr49:
	{ SET_CPU_FEATURE(CPUFeature_EMMX);      }
	goto st43;
tr150:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st43;
tr154:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE4A);     }
	goto st43;
tr309:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st43;
tr399:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st43;
tr397:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE4A);     }
	goto st43;
tr414:
	{
    SET_REPZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st43;
st43:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof43;
case 43:
	if ( 192u <= (*( current_position)) )
		goto tr0;
	goto tr16;
st44:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof44;
case 44:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr91;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr91;
	} else
		goto tr91;
	goto tr16;
st45:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof45;
case 45:
	if ( (*( current_position)) > 215u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr91;
	} else if ( (*( current_position)) >= 208u )
		goto tr91;
	goto tr16;
st46:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof46;
case 46:
	goto tr92;
tr92:
	{}
	goto st47;
st47:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof47;
case 47:
	goto tr93;
tr93:
	{}
	goto st48;
st48:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof48;
case 48:
	goto tr94;
tr94:
	{}
	goto st49;
st49:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof49;
case 49:
	goto tr95;
st50:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof50;
case 50:
	switch( (*( current_position)) ) {
		case 4u: goto tr96;
		case 5u: goto tr97;
		case 12u: goto tr96;
		case 13u: goto tr97;
		case 20u: goto tr72;
		case 21u: goto tr73;
		case 28u: goto tr72;
		case 29u: goto tr73;
		case 36u: goto tr96;
		case 37u: goto tr97;
		case 44u: goto tr96;
		case 45u: goto tr97;
		case 52u: goto tr96;
		case 53u: goto tr97;
		case 60u: goto tr99;
		case 61u: goto tr100;
		case 68u: goto tr102;
		case 76u: goto tr102;
		case 84u: goto tr75;
		case 92u: goto tr75;
		case 100u: goto tr102;
		case 108u: goto tr102;
		case 116u: goto tr102;
		case 124u: goto tr104;
		case 132u: goto tr105;
		case 140u: goto tr105;
		case 148u: goto tr76;
		case 156u: goto tr76;
		case 164u: goto tr105;
		case 172u: goto tr105;
		case 180u: goto tr105;
		case 188u: goto tr106;
		case 232u: goto tr107;
		case 240u: goto tr107;
		case 248u: goto tr108;
	}
	if ( (*( current_position)) < 80u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) > 15u ) {
				if ( 16u <= (*( current_position)) && (*( current_position)) <= 31u )
					goto tr71;
			} else
				goto tr51;
		} else if ( (*( current_position)) > 55u ) {
			if ( (*( current_position)) > 63u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr101;
			} else if ( (*( current_position)) >= 56u )
				goto tr98;
		} else
			goto tr51;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) < 128u ) {
			if ( (*( current_position)) > 119u ) {
				if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr103;
			} else if ( (*( current_position)) >= 96u )
				goto tr101;
		} else if ( (*( current_position)) > 143u ) {
			if ( (*( current_position)) < 160u ) {
				if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
					goto tr73;
			} else if ( (*( current_position)) > 183u ) {
				if ( 184u <= (*( current_position)) && (*( current_position)) <= 191u )
					goto tr100;
			} else
				goto tr97;
		} else
			goto tr97;
	} else
		goto tr74;
	goto tr16;
st51:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof51;
case 51:
	switch( (*( current_position)) ) {
		case 4u: goto tr110;
		case 5u: goto tr111;
		case 12u: goto tr110;
		case 13u: goto tr111;
		case 20u: goto tr110;
		case 21u: goto tr111;
		case 28u: goto tr110;
		case 29u: goto tr111;
		case 36u: goto tr110;
		case 37u: goto tr111;
		case 44u: goto tr110;
		case 45u: goto tr111;
		case 52u: goto tr110;
		case 53u: goto tr111;
		case 60u: goto tr110;
		case 61u: goto tr111;
		case 68u: goto tr113;
		case 76u: goto tr113;
		case 84u: goto tr113;
		case 92u: goto tr113;
		case 100u: goto tr113;
		case 108u: goto tr113;
		case 116u: goto tr113;
		case 124u: goto tr113;
		case 132u: goto tr114;
		case 140u: goto tr114;
		case 148u: goto tr114;
		case 156u: goto tr114;
		case 164u: goto tr114;
		case 172u: goto tr114;
		case 180u: goto tr114;
		case 188u: goto tr114;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr111;
	} else if ( (*( current_position)) >= 64u )
		goto tr112;
	goto tr109;
tr44:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st52;
tr157:
	{
    SET_DATA16_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st52;
tr312:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st52;
st52:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof52;
case 52:
	if ( 192u <= (*( current_position)) )
		goto st10;
	goto tr16;
st53:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof53;
case 53:
	switch( (*( current_position)) ) {
		case 12u: goto tr116;
		case 13u: goto tr117;
		case 76u: goto tr119;
		case 140u: goto tr120;
	}
	if ( (*( current_position)) < 72u ) {
		if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
			goto tr115;
	} else if ( (*( current_position)) > 79u ) {
		if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
			goto tr117;
	} else
		goto tr118;
	goto tr16;
st54:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof54;
case 54:
	if ( (*( current_position)) == 15u )
		goto st55;
	if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
		goto st56;
	goto tr16;
st55:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof55;
case 55:
	if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
		goto st46;
	goto tr16;
st56:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof56;
case 56:
	goto tr123;
st57:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof57;
case 57:
	switch( (*( current_position)) ) {
		case 139u: goto st58;
		case 161u: goto st59;
	}
	goto tr16;
st58:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof58;
case 58:
	switch( (*( current_position)) ) {
		case 5u: goto st59;
		case 13u: goto st59;
		case 21u: goto st59;
		case 29u: goto st59;
		case 37u: goto st59;
		case 45u: goto st59;
		case 53u: goto st59;
		case 61u: goto st59;
	}
	goto tr16;
st59:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof59;
case 59:
	switch( (*( current_position)) ) {
		case 0u: goto st60;
		case 4u: goto st60;
	}
	goto tr16;
st60:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof60;
case 60:
	if ( (*( current_position)) == 0u )
		goto st61;
	goto tr16;
st61:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof61;
case 61:
	if ( (*( current_position)) == 0u )
		goto st62;
	goto tr16;
st62:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof62;
case 62:
	if ( (*( current_position)) == 0u )
		goto tr0;
	goto tr16;
st63:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof63;
case 63:
	switch( (*( current_position)) ) {
		case 1u: goto st1;
		case 3u: goto st1;
		case 5u: goto st64;
		case 9u: goto st1;
		case 11u: goto st1;
		case 13u: goto st64;
		case 15u: goto st66;
		case 17u: goto st1;
		case 19u: goto st1;
		case 21u: goto st64;
		case 25u: goto st1;
		case 27u: goto st1;
		case 29u: goto st64;
		case 33u: goto st1;
		case 35u: goto st1;
		case 37u: goto st64;
		case 41u: goto st1;
		case 43u: goto st1;
		case 45u: goto st64;
		case 46u: goto st79;
		case 49u: goto st1;
		case 51u: goto st1;
		case 53u: goto st64;
		case 57u: goto st1;
		case 59u: goto st1;
		case 61u: goto st64;
		case 102u: goto st82;
		case 104u: goto st64;
		case 105u: goto st87;
		case 107u: goto st34;
		case 129u: goto st87;
		case 131u: goto st34;
		case 133u: goto st1;
		case 135u: goto st1;
		case 137u: goto st1;
		case 139u: goto st1;
		case 141u: goto st29;
		case 143u: goto st31;
		case 161u: goto st3;
		case 163u: goto st3;
		case 165u: goto tr0;
		case 167u: goto tr0;
		case 169u: goto st64;
		case 171u: goto tr0;
		case 175u: goto tr0;
		case 193u: goto st96;
		case 199u: goto st97;
		case 209u: goto st98;
		case 211u: goto st98;
		case 240u: goto st99;
		case 242u: goto st104;
		case 243u: goto st107;
		case 247u: goto st108;
		case 255u: goto st109;
	}
	if ( (*( current_position)) < 144u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr0;
	} else if ( (*( current_position)) > 153u ) {
		if ( 184u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st64;
	} else
		goto tr0;
	goto tr16;
tr202:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	goto st64;
tr203:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{}
	goto st64;
st64:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof64;
case 64:
	goto tr143;
tr143:
	{}
	goto st65;
st65:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof65;
case 65:
	goto tr144;
st66:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof66;
case 66:
	switch( (*( current_position)) ) {
		case 31u: goto st67;
		case 43u: goto tr146;
		case 56u: goto st73;
		case 58u: goto st74;
		case 80u: goto tr150;
		case 81u: goto tr145;
		case 112u: goto tr151;
		case 115u: goto st77;
		case 121u: goto tr154;
		case 175u: goto st1;
		case 194u: goto tr151;
		case 196u: goto st78;
		case 197u: goto tr157;
		case 198u: goto tr151;
		case 215u: goto tr150;
		case 231u: goto tr146;
		case 247u: goto tr150;
	}
	if ( (*( current_position)) < 113u ) {
		if ( (*( current_position)) < 22u ) {
			if ( (*( current_position)) < 18u ) {
				if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
					goto tr145;
			} else if ( (*( current_position)) > 19u ) {
				if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
					goto tr145;
			} else
				goto tr146;
		} else if ( (*( current_position)) > 23u ) {
			if ( (*( current_position)) < 64u ) {
				if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr145;
			} else if ( (*( current_position)) > 79u ) {
				if ( 84u <= (*( current_position)) && (*( current_position)) <= 111u )
					goto tr145;
			} else
				goto tr30;
		} else
			goto tr146;
	} else if ( (*( current_position)) > 114u ) {
		if ( (*( current_position)) < 182u ) {
			if ( (*( current_position)) < 124u ) {
				if ( 116u <= (*( current_position)) && (*( current_position)) <= 118u )
					goto tr145;
			} else if ( (*( current_position)) > 125u ) {
				if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr145;
			} else
				goto tr155;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( 190u <= (*( current_position)) && (*( current_position)) <= 191u )
					goto st1;
			} else if ( (*( current_position)) > 239u ) {
				if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
					goto tr145;
			} else
				goto tr145;
		} else
			goto st1;
	} else
		goto st76;
	goto tr16;
st67:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof67;
case 67:
	switch( (*( current_position)) ) {
		case 68u: goto st68;
		case 132u: goto st70;
	}
	goto tr16;
st68:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof68;
case 68:
	if ( (*( current_position)) == 0u )
		goto st69;
	goto tr16;
st69:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof69;
case 69:
	if ( (*( current_position)) == 0u )
		goto tr161;
	goto tr16;
st70:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof70;
case 70:
	if ( (*( current_position)) == 0u )
		goto st71;
	goto tr16;
st71:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof71;
case 71:
	if ( (*( current_position)) == 0u )
		goto st72;
	goto tr16;
st72:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof72;
case 72:
	if ( (*( current_position)) == 0u )
		goto st68;
	goto tr16;
st73:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof73;
case 73:
	switch( (*( current_position)) ) {
		case 16u: goto tr165;
		case 23u: goto tr165;
		case 42u: goto tr166;
		case 55u: goto tr167;
	}
	if ( (*( current_position)) < 32u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) <= 11u )
				goto tr164;
		} else if ( (*( current_position)) > 21u ) {
			if ( 28u <= (*( current_position)) && (*( current_position)) <= 30u )
				goto tr164;
		} else
			goto tr165;
	} else if ( (*( current_position)) > 37u ) {
		if ( (*( current_position)) < 48u ) {
			if ( 40u <= (*( current_position)) && (*( current_position)) <= 43u )
				goto tr165;
		} else if ( (*( current_position)) > 53u ) {
			if ( (*( current_position)) > 65u ) {
				if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr168;
			} else if ( (*( current_position)) >= 56u )
				goto tr165;
		} else
			goto tr165;
	} else
		goto tr165;
	goto tr16;
st74:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof74;
case 74:
	switch( (*( current_position)) ) {
		case 15u: goto tr170;
		case 32u: goto st75;
		case 68u: goto tr172;
		case 223u: goto tr174;
	}
	if ( (*( current_position)) < 22u ) {
		if ( (*( current_position)) > 14u ) {
			if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
				goto st75;
		} else if ( (*( current_position)) >= 8u )
			goto tr169;
	} else if ( (*( current_position)) > 23u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 33u <= (*( current_position)) && (*( current_position)) <= 34u )
				goto tr169;
		} else if ( (*( current_position)) > 66u ) {
			if ( 96u <= (*( current_position)) && (*( current_position)) <= 99u )
				goto tr173;
		} else
			goto tr169;
	} else
		goto tr169;
	goto tr16;
st75:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof75;
case 75:
	switch( (*( current_position)) ) {
		case 4u: goto tr176;
		case 5u: goto tr177;
		case 12u: goto tr176;
		case 13u: goto tr177;
		case 20u: goto tr176;
		case 21u: goto tr177;
		case 28u: goto tr176;
		case 29u: goto tr177;
		case 36u: goto tr176;
		case 37u: goto tr177;
		case 44u: goto tr176;
		case 45u: goto tr177;
		case 52u: goto tr176;
		case 53u: goto tr177;
		case 60u: goto tr176;
		case 61u: goto tr177;
		case 68u: goto tr179;
		case 76u: goto tr179;
		case 84u: goto tr179;
		case 92u: goto tr179;
		case 100u: goto tr179;
		case 108u: goto tr179;
		case 116u: goto tr179;
		case 124u: goto tr179;
		case 132u: goto tr180;
		case 140u: goto tr180;
		case 148u: goto tr180;
		case 156u: goto tr180;
		case 164u: goto tr180;
		case 172u: goto tr180;
		case 180u: goto tr180;
		case 188u: goto tr180;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr177;
	} else if ( (*( current_position)) >= 64u )
		goto tr178;
	goto tr175;
st76:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof76;
case 76:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr181;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr181;
	} else
		goto tr181;
	goto tr16;
st77:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof77;
case 77:
	if ( (*( current_position)) > 223u ) {
		if ( 240u <= (*( current_position)) )
			goto tr181;
	} else if ( (*( current_position)) >= 208u )
		goto tr181;
	goto tr16;
st78:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof78;
case 78:
	switch( (*( current_position)) ) {
		case 4u: goto tr183;
		case 5u: goto tr184;
		case 12u: goto tr183;
		case 13u: goto tr184;
		case 20u: goto tr183;
		case 21u: goto tr184;
		case 28u: goto tr183;
		case 29u: goto tr184;
		case 36u: goto tr183;
		case 37u: goto tr184;
		case 44u: goto tr183;
		case 45u: goto tr184;
		case 52u: goto tr183;
		case 53u: goto tr184;
		case 60u: goto tr183;
		case 61u: goto tr184;
		case 68u: goto tr186;
		case 76u: goto tr186;
		case 84u: goto tr186;
		case 92u: goto tr186;
		case 100u: goto tr186;
		case 108u: goto tr186;
		case 116u: goto tr186;
		case 124u: goto tr186;
		case 132u: goto tr187;
		case 140u: goto tr187;
		case 148u: goto tr187;
		case 156u: goto tr187;
		case 164u: goto tr187;
		case 172u: goto tr187;
		case 180u: goto tr187;
		case 188u: goto tr187;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr184;
	} else if ( (*( current_position)) >= 64u )
		goto tr185;
	goto tr182;
st79:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof79;
case 79:
	if ( (*( current_position)) == 15u )
		goto st80;
	goto tr16;
st80:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof80;
case 80:
	if ( (*( current_position)) == 31u )
		goto st81;
	goto tr16;
st81:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof81;
case 81:
	if ( (*( current_position)) == 132u )
		goto st70;
	goto tr16;
st82:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof82;
case 82:
	switch( (*( current_position)) ) {
		case 46u: goto st79;
		case 102u: goto st83;
	}
	goto tr16;
st83:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof83;
case 83:
	switch( (*( current_position)) ) {
		case 46u: goto st79;
		case 102u: goto st84;
	}
	goto tr16;
st84:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof84;
case 84:
	switch( (*( current_position)) ) {
		case 46u: goto st79;
		case 102u: goto st85;
	}
	goto tr16;
st85:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof85;
case 85:
	switch( (*( current_position)) ) {
		case 46u: goto st79;
		case 102u: goto st86;
	}
	goto tr16;
st86:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof86;
case 86:
	if ( (*( current_position)) == 46u )
		goto st79;
	goto tr16;
st87:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof87;
case 87:
	switch( (*( current_position)) ) {
		case 4u: goto st88;
		case 5u: goto st89;
		case 12u: goto st88;
		case 13u: goto st89;
		case 20u: goto st88;
		case 21u: goto st89;
		case 28u: goto st88;
		case 29u: goto st89;
		case 36u: goto st88;
		case 37u: goto st89;
		case 44u: goto st88;
		case 45u: goto st89;
		case 52u: goto st88;
		case 53u: goto st89;
		case 60u: goto st88;
		case 61u: goto st89;
		case 68u: goto st94;
		case 76u: goto st94;
		case 84u: goto st94;
		case 92u: goto st94;
		case 100u: goto st94;
		case 108u: goto st94;
		case 116u: goto st94;
		case 124u: goto st94;
		case 132u: goto st95;
		case 140u: goto st95;
		case 148u: goto st95;
		case 156u: goto st95;
		case 164u: goto st95;
		case 172u: goto st95;
		case 180u: goto st95;
		case 188u: goto st95;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st89;
	} else if ( (*( current_position)) >= 64u )
		goto st93;
	goto st64;
st88:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof88;
case 88:
	switch( (*( current_position)) ) {
		case 5u: goto st89;
		case 13u: goto st89;
		case 21u: goto st89;
		case 29u: goto st89;
		case 37u: goto st89;
		case 45u: goto st89;
		case 53u: goto st89;
		case 61u: goto st89;
		case 69u: goto st89;
		case 77u: goto st89;
		case 85u: goto st89;
		case 93u: goto st89;
		case 101u: goto st89;
		case 109u: goto st89;
		case 117u: goto st89;
		case 125u: goto st89;
		case 133u: goto st89;
		case 141u: goto st89;
		case 149u: goto st89;
		case 157u: goto st89;
		case 165u: goto st89;
		case 173u: goto st89;
		case 181u: goto st89;
		case 189u: goto st89;
		case 197u: goto st89;
		case 205u: goto st89;
		case 213u: goto st89;
		case 221u: goto st89;
		case 229u: goto st89;
		case 237u: goto st89;
		case 245u: goto st89;
		case 253u: goto st89;
	}
	goto st64;
st89:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof89;
case 89:
	goto tr199;
tr199:
	{}
	goto st90;
st90:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof90;
case 90:
	goto tr200;
tr200:
	{}
	goto st91;
st91:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof91;
case 91:
	goto tr201;
tr201:
	{}
	goto st92;
st92:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof92;
case 92:
	goto tr202;
st93:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof93;
case 93:
	goto tr203;
st94:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof94;
case 94:
	goto st93;
st95:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof95;
case 95:
	goto st89;
st96:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof96;
case 96:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 12u: goto st35;
		case 13u: goto st36;
		case 20u: goto st35;
		case 21u: goto st36;
		case 28u: goto st35;
		case 29u: goto st36;
		case 36u: goto st35;
		case 37u: goto st36;
		case 44u: goto st35;
		case 45u: goto st36;
		case 60u: goto st35;
		case 61u: goto st36;
		case 68u: goto st41;
		case 76u: goto st41;
		case 84u: goto st41;
		case 92u: goto st41;
		case 100u: goto st41;
		case 108u: goto st41;
		case 124u: goto st41;
		case 132u: goto st42;
		case 140u: goto st42;
		case 148u: goto st42;
		case 156u: goto st42;
		case 164u: goto st42;
		case 172u: goto st42;
		case 188u: goto st42;
	}
	if ( (*( current_position)) < 120u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 48u <= (*( current_position)) && (*( current_position)) <= 55u )
				goto tr16;
		} else if ( (*( current_position)) > 111u ) {
			if ( 112u <= (*( current_position)) && (*( current_position)) <= 119u )
				goto tr16;
		} else
			goto st40;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 175u )
				goto st36;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr16;
			} else if ( (*( current_position)) >= 184u )
				goto st36;
		} else
			goto tr16;
	} else
		goto st40;
	goto st10;
st97:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof97;
case 97:
	switch( (*( current_position)) ) {
		case 4u: goto st88;
		case 5u: goto st89;
		case 68u: goto st94;
		case 132u: goto st95;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st64;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st64;
		} else if ( (*( current_position)) >= 128u )
			goto st89;
	} else
		goto st93;
	goto tr16;
st98:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof98;
case 98:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 124u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 188u: goto st9;
	}
	if ( (*( current_position)) < 120u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 48u <= (*( current_position)) && (*( current_position)) <= 55u )
				goto tr16;
		} else if ( (*( current_position)) > 111u ) {
			if ( 112u <= (*( current_position)) && (*( current_position)) <= 119u )
				goto tr16;
		} else
			goto st7;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 175u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr16;
			} else if ( (*( current_position)) >= 184u )
				goto st3;
		} else
			goto tr16;
	} else
		goto st7;
	goto tr0;
st99:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof99;
case 99:
	switch( (*( current_position)) ) {
		case 1u: goto st29;
		case 9u: goto st29;
		case 17u: goto st29;
		case 25u: goto st29;
		case 33u: goto st29;
		case 41u: goto st29;
		case 49u: goto st29;
		case 129u: goto st100;
		case 131u: goto st101;
		case 135u: goto st29;
		case 247u: goto st102;
		case 255u: goto st103;
	}
	goto tr16;
st100:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof100;
case 100:
	switch( (*( current_position)) ) {
		case 4u: goto st88;
		case 5u: goto st89;
		case 12u: goto st88;
		case 13u: goto st89;
		case 20u: goto st88;
		case 21u: goto st89;
		case 28u: goto st88;
		case 29u: goto st89;
		case 36u: goto st88;
		case 37u: goto st89;
		case 44u: goto st88;
		case 45u: goto st89;
		case 52u: goto st88;
		case 53u: goto st89;
		case 68u: goto st94;
		case 76u: goto st94;
		case 84u: goto st94;
		case 92u: goto st94;
		case 100u: goto st94;
		case 108u: goto st94;
		case 116u: goto st94;
		case 132u: goto st95;
		case 140u: goto st95;
		case 148u: goto st95;
		case 156u: goto st95;
		case 164u: goto st95;
		case 172u: goto st95;
		case 180u: goto st95;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st64;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st89;
	} else
		goto st93;
	goto tr16;
st101:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof101;
case 101:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 12u: goto st35;
		case 13u: goto st36;
		case 20u: goto st35;
		case 21u: goto st36;
		case 28u: goto st35;
		case 29u: goto st36;
		case 36u: goto st35;
		case 37u: goto st36;
		case 44u: goto st35;
		case 45u: goto st36;
		case 52u: goto st35;
		case 53u: goto st36;
		case 68u: goto st41;
		case 76u: goto st41;
		case 84u: goto st41;
		case 92u: goto st41;
		case 100u: goto st41;
		case 108u: goto st41;
		case 116u: goto st41;
		case 132u: goto st42;
		case 140u: goto st42;
		case 148u: goto st42;
		case 156u: goto st42;
		case 164u: goto st42;
		case 172u: goto st42;
		case 180u: goto st42;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st10;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st36;
	} else
		goto st40;
	goto tr16;
st102:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof102;
case 102:
	switch( (*( current_position)) ) {
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 84u: goto st8;
		case 92u: goto st8;
		case 148u: goto st9;
		case 156u: goto st9;
	}
	if ( (*( current_position)) < 80u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 31u )
			goto tr0;
	} else if ( (*( current_position)) > 95u ) {
		if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto st3;
	} else
		goto st7;
	goto tr16;
st103:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof103;
case 103:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr0;
	} else if ( (*( current_position)) > 79u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
			goto st3;
	} else
		goto st7;
	goto tr16;
st104:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof104;
case 104:
	switch( (*( current_position)) ) {
		case 15u: goto st105;
		case 167u: goto tr0;
		case 175u: goto tr0;
	}
	goto tr16;
st105:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof105;
case 105:
	if ( (*( current_position)) == 56u )
		goto st106;
	goto tr16;
st106:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof106;
case 106:
	if ( (*( current_position)) == 241u )
		goto tr210;
	goto tr16;
st107:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof107;
case 107:
	switch( (*( current_position)) ) {
		case 165u: goto tr0;
		case 167u: goto tr0;
		case 171u: goto tr0;
		case 175u: goto tr0;
	}
	goto tr16;
st108:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof108;
case 108:
	switch( (*( current_position)) ) {
		case 4u: goto st88;
		case 5u: goto st89;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st94;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st95;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*( current_position)) < 80u ) {
		if ( (*( current_position)) < 8u ) {
			if ( (*( current_position)) <= 7u )
				goto st64;
		} else if ( (*( current_position)) > 15u ) {
			if ( (*( current_position)) > 71u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto st93;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr16;
			} else if ( (*( current_position)) >= 128u )
				goto st89;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 199u ) {
				if ( 200u <= (*( current_position)) && (*( current_position)) <= 207u )
					goto tr16;
			} else if ( (*( current_position)) >= 192u )
				goto st64;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
st109:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof109;
case 109:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
tr276:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st110;
st110:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof110;
case 110:
	switch( (*( current_position)) ) {
		case 4u: goto st111;
		case 5u: goto st112;
		case 12u: goto st111;
		case 13u: goto st112;
		case 20u: goto st111;
		case 21u: goto st112;
		case 28u: goto st111;
		case 29u: goto st112;
		case 36u: goto st111;
		case 37u: goto st112;
		case 44u: goto st111;
		case 45u: goto st112;
		case 52u: goto st111;
		case 53u: goto st112;
		case 60u: goto st111;
		case 61u: goto st112;
		case 68u: goto st117;
		case 76u: goto st117;
		case 84u: goto st117;
		case 92u: goto st117;
		case 100u: goto st117;
		case 108u: goto st117;
		case 116u: goto st117;
		case 124u: goto st117;
		case 132u: goto st118;
		case 140u: goto st118;
		case 148u: goto st118;
		case 156u: goto st118;
		case 164u: goto st118;
		case 172u: goto st118;
		case 180u: goto st118;
		case 188u: goto st118;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st112;
	} else if ( (*( current_position)) >= 64u )
		goto st116;
	goto st11;
tr271:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	goto st111;
st111:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof111;
case 111:
	switch( (*( current_position)) ) {
		case 5u: goto st112;
		case 13u: goto st112;
		case 21u: goto st112;
		case 29u: goto st112;
		case 37u: goto st112;
		case 45u: goto st112;
		case 53u: goto st112;
		case 61u: goto st112;
		case 69u: goto st112;
		case 77u: goto st112;
		case 85u: goto st112;
		case 93u: goto st112;
		case 101u: goto st112;
		case 109u: goto st112;
		case 117u: goto st112;
		case 125u: goto st112;
		case 133u: goto st112;
		case 141u: goto st112;
		case 149u: goto st112;
		case 157u: goto st112;
		case 165u: goto st112;
		case 173u: goto st112;
		case 181u: goto st112;
		case 189u: goto st112;
		case 197u: goto st112;
		case 205u: goto st112;
		case 213u: goto st112;
		case 221u: goto st112;
		case 229u: goto st112;
		case 237u: goto st112;
		case 245u: goto st112;
		case 253u: goto st112;
	}
	goto st11;
tr272:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	goto st112;
st112:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof112;
case 112:
	goto tr217;
tr217:
	{}
	goto st113;
st113:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof113;
case 113:
	goto tr218;
tr218:
	{}
	goto st114;
st114:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof114;
case 114:
	goto tr219;
tr219:
	{}
	goto st115;
st115:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof115;
case 115:
	goto tr220;
tr273:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	goto st116;
st116:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof116;
case 116:
	goto tr221;
tr274:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	goto st117;
st117:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof117;
case 117:
	goto st116;
tr275:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	goto st118;
st118:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof118;
case 118:
	goto st112;
st119:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof119;
case 119:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 12u: goto st35;
		case 13u: goto st36;
		case 20u: goto st35;
		case 21u: goto st36;
		case 28u: goto st35;
		case 29u: goto st36;
		case 36u: goto st35;
		case 37u: goto st36;
		case 44u: goto st35;
		case 45u: goto st36;
		case 52u: goto st35;
		case 53u: goto st36;
		case 60u: goto st35;
		case 61u: goto st36;
		case 68u: goto st41;
		case 76u: goto st41;
		case 84u: goto st41;
		case 92u: goto st41;
		case 100u: goto st41;
		case 108u: goto st41;
		case 116u: goto st41;
		case 124u: goto st41;
		case 132u: goto st42;
		case 140u: goto st42;
		case 148u: goto st42;
		case 156u: goto st42;
		case 164u: goto st42;
		case 172u: goto st42;
		case 180u: goto st42;
		case 188u: goto st42;
		case 224u: goto st120;
		case 225u: goto st228;
		case 226u: goto st230;
		case 227u: goto st232;
		case 228u: goto st234;
		case 229u: goto st236;
		case 230u: goto st238;
		case 231u: goto st240;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st36;
	} else if ( (*( current_position)) >= 64u )
		goto st40;
	goto st10;
st120:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof120;
case 120:
	if ( (*( current_position)) == 224u )
		goto tr230;
	goto tr11;
tr230:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st243;
st243:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof243;
case 243:
	switch( (*( current_position)) ) {
		case 4u: goto st10;
		case 5u: goto st11;
		case 12u: goto st10;
		case 13u: goto st11;
		case 15u: goto st15;
		case 20u: goto st10;
		case 21u: goto st11;
		case 28u: goto st10;
		case 29u: goto st11;
		case 36u: goto st10;
		case 37u: goto st11;
		case 44u: goto st10;
		case 45u: goto st11;
		case 46u: goto st54;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st54;
		case 101u: goto st57;
		case 102u: goto st63;
		case 104u: goto st11;
		case 105u: goto st110;
		case 106u: goto st10;
		case 107u: goto st34;
		case 128u: goto st34;
		case 129u: goto st110;
		case 131u: goto st119;
		case 141u: goto st29;
		case 143u: goto st121;
		case 155u: goto tr377;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st149;
		case 197u: goto st199;
		case 198u: goto st200;
		case 199u: goto st201;
		case 201u: goto tr0;
		case 216u: goto st202;
		case 217u: goto st203;
		case 218u: goto st204;
		case 219u: goto st205;
		case 220u: goto st206;
		case 221u: goto st207;
		case 222u: goto st208;
		case 223u: goto st209;
		case 232u: goto st210;
		case 233u: goto st46;
		case 235u: goto st56;
		case 240u: goto st214;
		case 242u: goto st217;
		case 243u: goto st222;
		case 246u: goto st224;
		case 247u: goto st225;
		case 254u: goto st226;
		case 255u: goto st227;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) <= 3u )
					goto st1;
			} else if ( (*( current_position)) > 11u ) {
				if ( (*( current_position)) > 19u ) {
					if ( 24u <= (*( current_position)) && (*( current_position)) <= 27u )
						goto st1;
				} else if ( (*( current_position)) >= 16u )
					goto st1;
			} else
				goto st1;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 56u ) {
				if ( (*( current_position)) > 43u ) {
					if ( 48u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto st1;
				} else if ( (*( current_position)) >= 40u )
					goto st1;
			} else if ( (*( current_position)) > 59u ) {
				if ( (*( current_position)) > 95u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto st56;
				} else if ( (*( current_position)) >= 64u )
					goto tr0;
			} else
				goto st1;
		} else
			goto st1;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 176u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 153u ) {
					if ( 158u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto tr0;
				} else if ( (*( current_position)) >= 144u )
					goto tr0;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) > 171u ) {
					if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
						goto tr0;
				} else if ( (*( current_position)) >= 164u )
					goto tr0;
			} else
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto st96;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st98;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
st121:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof121;
case 121:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 68u: goto st8;
		case 132u: goto st9;
		case 232u: goto st122;
		case 233u: goto st137;
		case 234u: goto st145;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto tr0;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto tr0;
		} else if ( (*( current_position)) >= 128u )
			goto st3;
	} else
		goto st7;
	goto tr16;
st122:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof122;
case 122:
	switch( (*( current_position)) ) {
		case 64u: goto tr234;
		case 68u: goto tr235;
		case 72u: goto tr234;
		case 76u: goto tr235;
		case 80u: goto tr234;
		case 84u: goto tr235;
		case 88u: goto tr234;
		case 92u: goto tr235;
		case 96u: goto tr234;
		case 100u: goto tr235;
		case 104u: goto tr234;
		case 108u: goto tr235;
		case 112u: goto tr234;
		case 116u: goto tr235;
		case 120u: goto tr236;
		case 124u: goto tr235;
		case 192u: goto tr237;
		case 196u: goto tr235;
		case 200u: goto tr237;
		case 204u: goto tr235;
		case 208u: goto tr237;
		case 212u: goto tr235;
		case 216u: goto tr237;
		case 220u: goto tr235;
		case 224u: goto tr237;
		case 228u: goto tr235;
		case 232u: goto tr237;
		case 236u: goto tr235;
		case 240u: goto tr237;
		case 244u: goto tr235;
		case 248u: goto tr237;
		case 252u: goto tr235;
	}
	goto tr16;
tr234:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st123;
st123:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof123;
case 123:
	switch( (*( current_position)) ) {
		case 166u: goto tr238;
		case 182u: goto tr238;
	}
	if ( (*( current_position)) < 158u ) {
		if ( (*( current_position)) < 142u ) {
			if ( 133u <= (*( current_position)) && (*( current_position)) <= 135u )
				goto tr238;
		} else if ( (*( current_position)) > 143u ) {
			if ( 149u <= (*( current_position)) && (*( current_position)) <= 151u )
				goto tr238;
		} else
			goto tr238;
	} else if ( (*( current_position)) > 159u ) {
		if ( (*( current_position)) < 204u ) {
			if ( 162u <= (*( current_position)) && (*( current_position)) <= 163u )
				goto tr238;
		} else if ( (*( current_position)) > 207u ) {
			if ( 236u <= (*( current_position)) && (*( current_position)) <= 239u )
				goto tr239;
		} else
			goto tr239;
	} else
		goto tr238;
	goto tr16;
tr238:
	{ SET_CPU_FEATURE(CPUFeature_XOP);       }
	goto st124;
tr344:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st124;
tr345:
	{ SET_CPU_FEATURE(CPUFeature_FMA4);      }
	goto st124;
st124:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof124;
case 124:
	switch( (*( current_position)) ) {
		case 4u: goto st126;
		case 5u: goto st127;
		case 12u: goto st126;
		case 13u: goto st127;
		case 20u: goto st126;
		case 21u: goto st127;
		case 28u: goto st126;
		case 29u: goto st127;
		case 36u: goto st126;
		case 37u: goto st127;
		case 44u: goto st126;
		case 45u: goto st127;
		case 52u: goto st126;
		case 53u: goto st127;
		case 60u: goto st126;
		case 61u: goto st127;
		case 68u: goto st132;
		case 76u: goto st132;
		case 84u: goto st132;
		case 92u: goto st132;
		case 100u: goto st132;
		case 108u: goto st132;
		case 116u: goto st132;
		case 124u: goto st132;
		case 132u: goto st133;
		case 140u: goto st133;
		case 148u: goto st133;
		case 156u: goto st133;
		case 164u: goto st133;
		case 172u: goto st133;
		case 180u: goto st133;
		case 188u: goto st133;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st127;
	} else if ( (*( current_position)) >= 64u )
		goto st131;
	goto st125;
tr250:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	goto st125;
tr251:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{}
	goto st125;
st125:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof125;
case 125:
	switch( (*( current_position)) ) {
		case 0u: goto tr246;
		case 16u: goto tr246;
		case 32u: goto tr246;
		case 48u: goto tr246;
		case 64u: goto tr246;
		case 80u: goto tr246;
		case 96u: goto tr246;
		case 112u: goto tr246;
	}
	goto tr16;
st126:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof126;
case 126:
	switch( (*( current_position)) ) {
		case 5u: goto st127;
		case 13u: goto st127;
		case 21u: goto st127;
		case 29u: goto st127;
		case 37u: goto st127;
		case 45u: goto st127;
		case 53u: goto st127;
		case 61u: goto st127;
		case 69u: goto st127;
		case 77u: goto st127;
		case 85u: goto st127;
		case 93u: goto st127;
		case 101u: goto st127;
		case 109u: goto st127;
		case 117u: goto st127;
		case 125u: goto st127;
		case 133u: goto st127;
		case 141u: goto st127;
		case 149u: goto st127;
		case 157u: goto st127;
		case 165u: goto st127;
		case 173u: goto st127;
		case 181u: goto st127;
		case 189u: goto st127;
		case 197u: goto st127;
		case 205u: goto st127;
		case 213u: goto st127;
		case 221u: goto st127;
		case 229u: goto st127;
		case 237u: goto st127;
		case 245u: goto st127;
		case 253u: goto st127;
	}
	goto st125;
st127:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof127;
case 127:
	goto tr247;
tr247:
	{}
	goto st128;
st128:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof128;
case 128:
	goto tr248;
tr248:
	{}
	goto st129;
st129:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof129;
case 129:
	goto tr249;
tr249:
	{}
	goto st130;
st130:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof130;
case 130:
	goto tr250;
st131:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof131;
case 131:
	goto tr251;
st132:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof132;
case 132:
	goto st131;
st133:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof133;
case 133:
	goto st127;
tr235:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st134;
st134:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof134;
case 134:
	if ( (*( current_position)) == 162u )
		goto tr238;
	goto tr16;
tr236:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st135;
st135:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof135;
case 135:
	switch( (*( current_position)) ) {
		case 166u: goto tr238;
		case 182u: goto tr238;
	}
	if ( (*( current_position)) < 158u ) {
		if ( (*( current_position)) < 142u ) {
			if ( 133u <= (*( current_position)) && (*( current_position)) <= 135u )
				goto tr238;
		} else if ( (*( current_position)) > 143u ) {
			if ( 149u <= (*( current_position)) && (*( current_position)) <= 151u )
				goto tr238;
		} else
			goto tr238;
	} else if ( (*( current_position)) > 159u ) {
		if ( (*( current_position)) < 192u ) {
			if ( 162u <= (*( current_position)) && (*( current_position)) <= 163u )
				goto tr238;
		} else if ( (*( current_position)) > 195u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 236u <= (*( current_position)) && (*( current_position)) <= 239u )
					goto tr239;
			} else if ( (*( current_position)) >= 204u )
				goto tr239;
		} else
			goto tr239;
	} else
		goto tr238;
	goto tr16;
tr237:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st136;
st136:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof136;
case 136:
	if ( 162u <= (*( current_position)) && (*( current_position)) <= 163u )
		goto tr238;
	goto tr16;
st137:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof137;
case 137:
	switch( (*( current_position)) ) {
		case 64u: goto tr252;
		case 72u: goto tr252;
		case 80u: goto tr252;
		case 88u: goto tr252;
		case 96u: goto tr252;
		case 104u: goto tr252;
		case 112u: goto tr252;
		case 120u: goto tr253;
		case 124u: goto tr254;
		case 192u: goto tr255;
		case 200u: goto tr255;
		case 208u: goto tr255;
		case 216u: goto tr255;
		case 224u: goto tr255;
		case 232u: goto tr255;
		case 240u: goto tr255;
		case 248u: goto tr255;
	}
	goto tr16;
tr252:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st138;
st138:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof138;
case 138:
	switch( (*( current_position)) ) {
		case 1u: goto st139;
		case 2u: goto st140;
	}
	if ( 144u <= (*( current_position)) && (*( current_position)) <= 155u )
		goto tr258;
	goto tr16;
st139:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof139;
case 139:
	switch( (*( current_position)) ) {
		case 12u: goto tr260;
		case 13u: goto tr261;
		case 20u: goto tr260;
		case 21u: goto tr261;
		case 28u: goto tr260;
		case 29u: goto tr261;
		case 36u: goto tr260;
		case 37u: goto tr261;
		case 44u: goto tr260;
		case 45u: goto tr261;
		case 52u: goto tr260;
		case 53u: goto tr261;
		case 60u: goto tr260;
		case 61u: goto tr261;
		case 76u: goto tr263;
		case 84u: goto tr263;
		case 92u: goto tr263;
		case 100u: goto tr263;
		case 108u: goto tr263;
		case 116u: goto tr263;
		case 124u: goto tr263;
		case 140u: goto tr264;
		case 148u: goto tr264;
		case 156u: goto tr264;
		case 164u: goto tr264;
		case 172u: goto tr264;
		case 180u: goto tr264;
		case 188u: goto tr264;
	}
	if ( (*( current_position)) < 72u ) {
		if ( (*( current_position)) > 7u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 71u )
				goto tr16;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 136u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 135u )
				goto tr16;
		} else if ( (*( current_position)) > 191u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto tr16;
		} else
			goto tr261;
	} else
		goto tr262;
	goto tr259;
st140:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof140;
case 140:
	switch( (*( current_position)) ) {
		case 12u: goto tr260;
		case 13u: goto tr261;
		case 52u: goto tr260;
		case 53u: goto tr261;
		case 76u: goto tr263;
		case 116u: goto tr263;
		case 140u: goto tr264;
		case 180u: goto tr264;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
				goto tr259;
		} else if ( (*( current_position)) > 55u ) {
			if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto tr262;
		} else
			goto tr259;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto tr261;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr259;
			} else if ( (*( current_position)) >= 200u )
				goto tr259;
		} else
			goto tr261;
	} else
		goto tr262;
	goto tr16;
tr253:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st141;
st141:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof141;
case 141:
	switch( (*( current_position)) ) {
		case 1u: goto st139;
		case 2u: goto st140;
		case 18u: goto st142;
		case 203u: goto tr258;
		case 219u: goto tr258;
	}
	if ( (*( current_position)) < 198u ) {
		if ( (*( current_position)) < 144u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 131u )
				goto tr258;
		} else if ( (*( current_position)) > 155u ) {
			if ( 193u <= (*( current_position)) && (*( current_position)) <= 195u )
				goto tr258;
		} else
			goto tr258;
	} else if ( (*( current_position)) > 199u ) {
		if ( (*( current_position)) < 214u ) {
			if ( 209u <= (*( current_position)) && (*( current_position)) <= 211u )
				goto tr258;
		} else if ( (*( current_position)) > 215u ) {
			if ( 225u <= (*( current_position)) && (*( current_position)) <= 227u )
				goto tr258;
		} else
			goto tr258;
	} else
		goto tr258;
	goto tr16;
st142:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof142;
case 142:
	if ( 192u <= (*( current_position)) && (*( current_position)) <= 207u )
		goto tr266;
	goto tr16;
tr254:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st143;
st143:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof143;
case 143:
	if ( 128u <= (*( current_position)) && (*( current_position)) <= 129u )
		goto tr258;
	goto tr16;
tr255:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st144;
st144:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof144;
case 144:
	if ( 144u <= (*( current_position)) && (*( current_position)) <= 155u )
		goto tr258;
	goto tr16;
st145:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof145;
case 145:
	switch( (*( current_position)) ) {
		case 64u: goto tr267;
		case 72u: goto tr267;
		case 80u: goto tr267;
		case 88u: goto tr267;
		case 96u: goto tr267;
		case 104u: goto tr267;
		case 112u: goto tr267;
		case 120u: goto tr268;
	}
	goto tr16;
tr267:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st146;
st146:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof146;
case 146:
	if ( (*( current_position)) == 18u )
		goto st147;
	goto tr16;
st147:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof147;
case 147:
	switch( (*( current_position)) ) {
		case 4u: goto tr271;
		case 5u: goto tr272;
		case 12u: goto tr271;
		case 13u: goto tr272;
		case 68u: goto tr274;
		case 76u: goto tr274;
		case 132u: goto tr275;
		case 140u: goto tr275;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr270;
	} else if ( (*( current_position)) > 79u ) {
		if ( (*( current_position)) > 143u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 207u )
				goto tr270;
		} else if ( (*( current_position)) >= 128u )
			goto tr272;
	} else
		goto tr273;
	goto tr16;
tr268:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st148;
st148:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof148;
case 148:
	switch( (*( current_position)) ) {
		case 16u: goto tr276;
		case 18u: goto st147;
	}
	goto tr16;
st149:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof149;
case 149:
	switch( (*( current_position)) ) {
		case 225u: goto st150;
		case 226u: goto st172;
		case 227u: goto st181;
	}
	goto tr16;
st150:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof150;
case 150:
	switch( (*( current_position)) ) {
		case 65u: goto tr281;
		case 66u: goto tr282;
		case 67u: goto tr283;
		case 68u: goto tr284;
		case 69u: goto tr285;
		case 70u: goto tr286;
		case 71u: goto tr287;
		case 73u: goto tr281;
		case 74u: goto tr282;
		case 75u: goto tr283;
		case 76u: goto tr284;
		case 77u: goto tr285;
		case 78u: goto tr286;
		case 79u: goto tr287;
		case 81u: goto tr281;
		case 82u: goto tr282;
		case 83u: goto tr283;
		case 84u: goto tr284;
		case 85u: goto tr285;
		case 86u: goto tr286;
		case 87u: goto tr287;
		case 89u: goto tr281;
		case 90u: goto tr282;
		case 91u: goto tr283;
		case 92u: goto tr284;
		case 93u: goto tr285;
		case 94u: goto tr286;
		case 95u: goto tr287;
		case 97u: goto tr281;
		case 98u: goto tr282;
		case 99u: goto tr283;
		case 100u: goto tr284;
		case 101u: goto tr285;
		case 102u: goto tr286;
		case 103u: goto tr287;
		case 105u: goto tr281;
		case 106u: goto tr282;
		case 107u: goto tr283;
		case 108u: goto tr284;
		case 109u: goto tr285;
		case 110u: goto tr286;
		case 111u: goto tr287;
		case 113u: goto tr281;
		case 114u: goto tr282;
		case 115u: goto tr283;
		case 116u: goto tr284;
		case 117u: goto tr285;
		case 118u: goto tr286;
		case 119u: goto tr287;
		case 120u: goto tr288;
		case 121u: goto tr289;
		case 122u: goto tr290;
		case 123u: goto tr291;
		case 124u: goto tr292;
		case 125u: goto tr293;
		case 126u: goto tr294;
		case 127u: goto tr295;
	}
	if ( 64u <= (*( current_position)) && (*( current_position)) <= 112u )
		goto tr280;
	goto tr16;
tr280:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st151;
tr361:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st151;
st151:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof151;
case 151:
	switch( (*( current_position)) ) {
		case 18u: goto st152;
		case 22u: goto st152;
		case 23u: goto tr298;
		case 81u: goto tr297;
		case 194u: goto tr299;
		case 198u: goto tr299;
	}
	if ( (*( current_position)) < 46u ) {
		if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr297;
	} else if ( (*( current_position)) > 47u ) {
		if ( (*( current_position)) > 89u ) {
			if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr297;
		} else if ( (*( current_position)) >= 84u )
			goto tr297;
	} else
		goto tr297;
	goto tr16;
st152:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof152;
case 152:
	switch( (*( current_position)) ) {
		case 4u: goto tr301;
		case 5u: goto tr302;
		case 12u: goto tr301;
		case 13u: goto tr302;
		case 20u: goto tr301;
		case 21u: goto tr302;
		case 28u: goto tr301;
		case 29u: goto tr302;
		case 36u: goto tr301;
		case 37u: goto tr302;
		case 44u: goto tr301;
		case 45u: goto tr302;
		case 52u: goto tr301;
		case 53u: goto tr302;
		case 60u: goto tr301;
		case 61u: goto tr302;
		case 68u: goto tr304;
		case 76u: goto tr304;
		case 84u: goto tr304;
		case 92u: goto tr304;
		case 100u: goto tr304;
		case 108u: goto tr304;
		case 116u: goto tr304;
		case 124u: goto tr304;
		case 132u: goto tr305;
		case 140u: goto tr305;
		case 148u: goto tr305;
		case 156u: goto tr305;
		case 164u: goto tr305;
		case 172u: goto tr305;
		case 180u: goto tr305;
		case 188u: goto tr305;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr302;
	} else if ( (*( current_position)) >= 64u )
		goto tr303;
	goto tr300;
tr281:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st153;
tr362:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st153;
st153:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof153;
case 153:
	switch( (*( current_position)) ) {
		case 18u: goto tr298;
		case 81u: goto tr297;
		case 115u: goto st155;
		case 194u: goto tr299;
		case 198u: goto tr299;
	}
	if ( (*( current_position)) < 116u ) {
		if ( (*( current_position)) < 46u ) {
			if ( (*( current_position)) > 21u ) {
				if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
					goto tr298;
			} else if ( (*( current_position)) >= 20u )
				goto tr297;
		} else if ( (*( current_position)) > 47u ) {
			if ( (*( current_position)) < 92u ) {
				if ( 84u <= (*( current_position)) && (*( current_position)) <= 89u )
					goto tr297;
			} else if ( (*( current_position)) > 109u ) {
				if ( 113u <= (*( current_position)) && (*( current_position)) <= 114u )
					goto st154;
			} else
				goto tr297;
		} else
			goto tr297;
	} else if ( (*( current_position)) > 118u ) {
		if ( (*( current_position)) < 216u ) {
			if ( (*( current_position)) > 125u ) {
				if ( 208u <= (*( current_position)) && (*( current_position)) <= 213u )
					goto tr297;
			} else if ( (*( current_position)) >= 124u )
				goto tr297;
		} else if ( (*( current_position)) > 229u ) {
			if ( (*( current_position)) < 241u ) {
				if ( 232u <= (*( current_position)) && (*( current_position)) <= 239u )
					goto tr297;
			} else if ( (*( current_position)) > 246u ) {
				if ( 248u <= (*( current_position)) && (*( current_position)) <= 254u )
					goto tr297;
			} else
				goto tr297;
		} else
			goto tr297;
	} else
		goto tr297;
	goto tr16;
st154:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof154;
case 154:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr308;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr308;
	} else
		goto tr308;
	goto tr16;
st155:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof155;
case 155:
	if ( (*( current_position)) > 223u ) {
		if ( 240u <= (*( current_position)) )
			goto tr308;
	} else if ( (*( current_position)) >= 208u )
		goto tr308;
	goto tr16;
tr282:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st156;
tr363:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st156;
st156:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof156;
case 156:
	switch( (*( current_position)) ) {
		case 42u: goto tr297;
		case 81u: goto tr297;
		case 83u: goto tr297;
		case 194u: goto tr299;
	}
	if ( (*( current_position)) > 90u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr297;
	} else if ( (*( current_position)) >= 88u )
		goto tr297;
	goto tr16;
tr283:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st157;
tr364:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st157;
st157:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof157;
case 157:
	switch( (*( current_position)) ) {
		case 42u: goto tr297;
		case 81u: goto tr297;
		case 194u: goto tr299;
		case 208u: goto tr297;
	}
	if ( (*( current_position)) < 92u ) {
		if ( 88u <= (*( current_position)) && (*( current_position)) <= 90u )
			goto tr297;
	} else if ( (*( current_position)) > 95u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr297;
	} else
		goto tr297;
	goto tr16;
tr284:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st158;
tr365:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st158;
st158:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof158;
case 158:
	switch( (*( current_position)) ) {
		case 81u: goto tr297;
		case 194u: goto tr299;
		case 198u: goto tr299;
	}
	if ( (*( current_position)) < 84u ) {
		if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr297;
	} else if ( (*( current_position)) > 89u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr297;
	} else
		goto tr297;
	goto tr16;
tr285:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st159;
tr366:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st159;
st159:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof159;
case 159:
	switch( (*( current_position)) ) {
		case 81u: goto tr297;
		case 194u: goto tr299;
		case 198u: goto tr299;
		case 208u: goto tr297;
	}
	if ( (*( current_position)) < 84u ) {
		if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr297;
	} else if ( (*( current_position)) > 89u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr297;
		} else if ( (*( current_position)) >= 92u )
			goto tr297;
	} else
		goto tr297;
	goto tr16;
tr286:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st160;
tr367:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st160;
st160:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof160;
case 160:
	if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
		goto tr309;
	goto tr16;
tr287:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st161;
tr368:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st161;
st161:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof161;
case 161:
	if ( (*( current_position)) == 208u )
		goto tr297;
	if ( (*( current_position)) > 17u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr297;
	} else if ( (*( current_position)) >= 16u )
		goto tr309;
	goto tr16;
tr288:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st162;
tr369:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st162;
st162:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof162;
case 162:
	switch( (*( current_position)) ) {
		case 18u: goto st152;
		case 19u: goto tr298;
		case 22u: goto st152;
		case 23u: goto tr298;
		case 43u: goto tr298;
		case 80u: goto tr309;
		case 119u: goto tr300;
		case 174u: goto st163;
		case 194u: goto tr299;
		case 198u: goto tr299;
	}
	if ( (*( current_position)) < 40u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 21u )
			goto tr297;
	} else if ( (*( current_position)) > 41u ) {
		if ( (*( current_position)) > 47u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr297;
		} else if ( (*( current_position)) >= 46u )
			goto tr297;
	} else
		goto tr297;
	goto tr16;
st163:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof163;
case 163:
	switch( (*( current_position)) ) {
		case 20u: goto tr301;
		case 21u: goto tr302;
		case 28u: goto tr301;
		case 29u: goto tr302;
		case 84u: goto tr304;
		case 92u: goto tr304;
		case 148u: goto tr305;
		case 156u: goto tr305;
	}
	if ( (*( current_position)) < 80u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 31u )
			goto tr300;
	} else if ( (*( current_position)) > 95u ) {
		if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto tr302;
	} else
		goto tr303;
	goto tr16;
tr289:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st164;
tr370:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st164;
st164:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof164;
case 164:
	switch( (*( current_position)) ) {
		case 43u: goto tr298;
		case 80u: goto tr309;
		case 81u: goto tr297;
		case 112u: goto tr299;
		case 115u: goto st155;
		case 127u: goto tr297;
		case 194u: goto tr299;
		case 196u: goto st165;
		case 197u: goto tr312;
		case 198u: goto tr299;
		case 215u: goto tr309;
		case 231u: goto tr298;
		case 247u: goto tr309;
	}
	if ( (*( current_position)) < 84u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) > 17u ) {
				if ( 18u <= (*( current_position)) && (*( current_position)) <= 19u )
					goto tr298;
			} else if ( (*( current_position)) >= 16u )
				goto tr297;
		} else if ( (*( current_position)) > 21u ) {
			if ( (*( current_position)) < 40u ) {
				if ( 22u <= (*( current_position)) && (*( current_position)) <= 23u )
					goto tr298;
			} else if ( (*( current_position)) > 41u ) {
				if ( 46u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr297;
			} else
				goto tr297;
		} else
			goto tr297;
	} else if ( (*( current_position)) > 111u ) {
		if ( (*( current_position)) < 124u ) {
			if ( (*( current_position)) > 114u ) {
				if ( 116u <= (*( current_position)) && (*( current_position)) <= 118u )
					goto tr297;
			} else if ( (*( current_position)) >= 113u )
				goto st154;
		} else if ( (*( current_position)) > 125u ) {
			if ( (*( current_position)) < 216u ) {
				if ( 208u <= (*( current_position)) && (*( current_position)) <= 213u )
					goto tr297;
			} else if ( (*( current_position)) > 239u ) {
				if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
					goto tr297;
			} else
				goto tr297;
		} else
			goto tr297;
	} else
		goto tr297;
	goto tr16;
st165:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof165;
case 165:
	switch( (*( current_position)) ) {
		case 4u: goto tr313;
		case 5u: goto tr314;
		case 12u: goto tr313;
		case 13u: goto tr314;
		case 20u: goto tr313;
		case 21u: goto tr314;
		case 28u: goto tr313;
		case 29u: goto tr314;
		case 36u: goto tr313;
		case 37u: goto tr314;
		case 44u: goto tr313;
		case 45u: goto tr314;
		case 52u: goto tr313;
		case 53u: goto tr314;
		case 60u: goto tr313;
		case 61u: goto tr314;
		case 68u: goto tr316;
		case 76u: goto tr316;
		case 84u: goto tr316;
		case 92u: goto tr316;
		case 100u: goto tr316;
		case 108u: goto tr316;
		case 116u: goto tr316;
		case 124u: goto tr316;
		case 132u: goto tr317;
		case 140u: goto tr317;
		case 148u: goto tr317;
		case 156u: goto tr317;
		case 164u: goto tr317;
		case 172u: goto tr317;
		case 180u: goto tr317;
		case 188u: goto tr317;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr314;
	} else if ( (*( current_position)) >= 64u )
		goto tr315;
	goto tr308;
tr290:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st166;
tr371:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st166;
st166:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof166;
case 166:
	switch( (*( current_position)) ) {
		case 18u: goto tr297;
		case 22u: goto tr297;
		case 42u: goto tr297;
		case 111u: goto tr297;
		case 112u: goto tr299;
		case 194u: goto tr299;
		case 230u: goto tr297;
	}
	if ( (*( current_position)) < 81u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto tr297;
		} else if ( (*( current_position)) >= 16u )
			goto tr298;
	} else if ( (*( current_position)) > 83u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr297;
		} else if ( (*( current_position)) >= 88u )
			goto tr297;
	} else
		goto tr297;
	goto tr16;
tr291:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st167;
tr372:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st167;
st167:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof167;
case 167:
	switch( (*( current_position)) ) {
		case 18u: goto tr297;
		case 42u: goto tr297;
		case 81u: goto tr297;
		case 112u: goto tr299;
		case 194u: goto tr299;
		case 208u: goto tr297;
		case 230u: goto tr297;
		case 240u: goto tr298;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto tr297;
		} else if ( (*( current_position)) >= 16u )
			goto tr298;
	} else if ( (*( current_position)) > 90u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr297;
		} else if ( (*( current_position)) >= 92u )
			goto tr297;
	} else
		goto tr297;
	goto tr16;
tr292:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st168;
tr373:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st168;
st168:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof168;
case 168:
	switch( (*( current_position)) ) {
		case 43u: goto tr298;
		case 80u: goto tr309;
		case 119u: goto tr300;
		case 194u: goto tr299;
		case 198u: goto tr299;
	}
	if ( (*( current_position)) < 20u ) {
		if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
			goto tr297;
	} else if ( (*( current_position)) > 21u ) {
		if ( (*( current_position)) > 41u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr297;
		} else if ( (*( current_position)) >= 40u )
			goto tr297;
	} else
		goto tr297;
	goto tr16;
tr293:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st169;
tr374:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st169;
st169:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof169;
case 169:
	switch( (*( current_position)) ) {
		case 43u: goto tr298;
		case 80u: goto tr309;
		case 81u: goto tr297;
		case 111u: goto tr297;
		case 194u: goto tr299;
		case 198u: goto tr299;
		case 208u: goto tr297;
		case 214u: goto tr297;
		case 230u: goto tr297;
		case 231u: goto tr298;
	}
	if ( (*( current_position)) < 40u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
				goto tr297;
		} else if ( (*( current_position)) >= 16u )
			goto tr297;
	} else if ( (*( current_position)) > 41u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr297;
		} else if ( (*( current_position)) >= 84u )
			goto tr297;
	} else
		goto tr297;
	goto tr16;
tr294:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st170;
tr375:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st170;
st170:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof170;
case 170:
	switch( (*( current_position)) ) {
		case 18u: goto tr297;
		case 22u: goto tr297;
		case 91u: goto tr297;
		case 111u: goto tr297;
		case 127u: goto tr297;
		case 230u: goto tr297;
	}
	if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
		goto tr309;
	goto tr16;
tr295:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st171;
tr376:
	{
    /*
     * VEX.R is not used ia32 mode and VEX.W is always unset.
     *
     * Look for AMD64 version below for details of encoding.
     */
    SET_VEX_PREFIX3((*current_position) & (~VEX_W));
  }
	goto st171;
st171:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof171;
case 171:
	switch( (*( current_position)) ) {
		case 18u: goto tr297;
		case 208u: goto tr297;
		case 230u: goto tr297;
		case 240u: goto tr298;
	}
	if ( (*( current_position)) > 17u ) {
		if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
			goto tr297;
	} else if ( (*( current_position)) >= 16u )
		goto tr309;
	goto tr16;
st172:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof172;
case 172:
	switch( (*( current_position)) ) {
		case 64u: goto tr318;
		case 65u: goto tr319;
		case 69u: goto tr320;
		case 72u: goto tr318;
		case 73u: goto tr319;
		case 77u: goto tr320;
		case 80u: goto tr318;
		case 81u: goto tr319;
		case 85u: goto tr320;
		case 88u: goto tr318;
		case 89u: goto tr319;
		case 93u: goto tr320;
		case 96u: goto tr318;
		case 97u: goto tr319;
		case 101u: goto tr320;
		case 104u: goto tr318;
		case 105u: goto tr319;
		case 109u: goto tr320;
		case 112u: goto tr318;
		case 113u: goto tr319;
		case 117u: goto tr320;
		case 120u: goto tr318;
		case 121u: goto tr321;
		case 125u: goto tr322;
		case 193u: goto tr323;
		case 197u: goto tr324;
		case 201u: goto tr323;
		case 205u: goto tr324;
		case 209u: goto tr323;
		case 213u: goto tr324;
		case 217u: goto tr323;
		case 221u: goto tr324;
		case 225u: goto tr323;
		case 229u: goto tr324;
		case 233u: goto tr323;
		case 237u: goto tr324;
		case 241u: goto tr323;
		case 245u: goto tr324;
		case 249u: goto tr323;
		case 253u: goto tr324;
	}
	goto tr16;
tr318:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st173;
st173:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof173;
case 173:
	switch( (*( current_position)) ) {
		case 242u: goto tr325;
		case 243u: goto st174;
		case 247u: goto tr325;
	}
	goto tr16;
st174:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof174;
case 174:
	switch( (*( current_position)) ) {
		case 12u: goto tr328;
		case 13u: goto tr329;
		case 20u: goto tr328;
		case 21u: goto tr329;
		case 28u: goto tr328;
		case 29u: goto tr329;
		case 76u: goto tr331;
		case 84u: goto tr331;
		case 92u: goto tr331;
		case 140u: goto tr332;
		case 148u: goto tr332;
		case 156u: goto tr332;
	}
	if ( (*( current_position)) < 72u ) {
		if ( 8u <= (*( current_position)) && (*( current_position)) <= 31u )
			goto tr327;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) > 159u ) {
			if ( 200u <= (*( current_position)) && (*( current_position)) <= 223u )
				goto tr327;
		} else if ( (*( current_position)) >= 136u )
			goto tr329;
	} else
		goto tr330;
	goto tr16;
tr319:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st175;
st175:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof175;
case 175:
	if ( (*( current_position)) == 43u )
		goto tr297;
	if ( (*( current_position)) < 55u ) {
		if ( (*( current_position)) < 40u ) {
			if ( (*( current_position)) <= 13u )
				goto tr297;
		} else if ( (*( current_position)) > 41u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr298;
		} else
			goto tr297;
	} else if ( (*( current_position)) > 64u ) {
		if ( (*( current_position)) < 166u ) {
			if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
				goto tr333;
		} else if ( (*( current_position)) > 175u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr334;
			} else if ( (*( current_position)) >= 182u )
				goto tr333;
		} else
			goto tr333;
	} else
		goto tr297;
	goto tr16;
tr320:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st176;
st176:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof176;
case 176:
	switch( (*( current_position)) ) {
		case 154u: goto tr333;
		case 156u: goto tr333;
		case 158u: goto tr333;
		case 170u: goto tr333;
		case 172u: goto tr333;
		case 174u: goto tr333;
		case 186u: goto tr333;
		case 188u: goto tr333;
		case 190u: goto tr333;
	}
	if ( (*( current_position)) < 150u ) {
		if ( (*( current_position)) > 13u ) {
			if ( 44u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr298;
		} else if ( (*( current_position)) >= 12u )
			goto tr297;
	} else if ( (*( current_position)) > 152u ) {
		if ( (*( current_position)) > 168u ) {
			if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
				goto tr333;
		} else if ( (*( current_position)) >= 166u )
			goto tr333;
	} else
		goto tr333;
	goto tr16;
tr321:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st177;
st177:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof177;
case 177:
	switch( (*( current_position)) ) {
		case 19u: goto tr335;
		case 23u: goto tr297;
		case 24u: goto tr298;
		case 42u: goto tr298;
	}
	if ( (*( current_position)) < 48u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) > 15u ) {
				if ( 28u <= (*( current_position)) && (*( current_position)) <= 30u )
					goto tr297;
			} else
				goto tr297;
		} else if ( (*( current_position)) > 37u ) {
			if ( (*( current_position)) > 43u ) {
				if ( 44u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr298;
			} else if ( (*( current_position)) >= 40u )
				goto tr297;
		} else
			goto tr297;
	} else if ( (*( current_position)) > 53u ) {
		if ( (*( current_position)) < 166u ) {
			if ( (*( current_position)) > 65u ) {
				if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
					goto tr333;
			} else if ( (*( current_position)) >= 55u )
				goto tr297;
		} else if ( (*( current_position)) > 175u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr334;
			} else if ( (*( current_position)) >= 182u )
				goto tr333;
		} else
			goto tr333;
	} else
		goto tr297;
	goto tr16;
tr322:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st178;
st178:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof178;
case 178:
	switch( (*( current_position)) ) {
		case 19u: goto tr335;
		case 23u: goto tr297;
		case 154u: goto tr333;
		case 156u: goto tr333;
		case 158u: goto tr333;
		case 170u: goto tr333;
		case 172u: goto tr333;
		case 174u: goto tr333;
		case 186u: goto tr333;
		case 188u: goto tr333;
		case 190u: goto tr333;
	}
	if ( (*( current_position)) < 44u ) {
		if ( (*( current_position)) > 15u ) {
			if ( 24u <= (*( current_position)) && (*( current_position)) <= 26u )
				goto tr298;
		} else if ( (*( current_position)) >= 12u )
			goto tr297;
	} else if ( (*( current_position)) > 47u ) {
		if ( (*( current_position)) < 166u ) {
			if ( 150u <= (*( current_position)) && (*( current_position)) <= 152u )
				goto tr333;
		} else if ( (*( current_position)) > 168u ) {
			if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
				goto tr333;
		} else
			goto tr333;
	} else
		goto tr298;
	goto tr16;
tr323:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st179;
st179:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof179;
case 179:
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto tr333;
	} else if ( (*( current_position)) > 175u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr333;
	} else
		goto tr333;
	goto tr16;
tr324:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st180;
st180:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof180;
case 180:
	switch( (*( current_position)) ) {
		case 154u: goto tr333;
		case 156u: goto tr333;
		case 158u: goto tr333;
		case 170u: goto tr333;
		case 172u: goto tr333;
		case 174u: goto tr333;
		case 186u: goto tr333;
		case 188u: goto tr333;
		case 190u: goto tr333;
	}
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 152u )
			goto tr333;
	} else if ( (*( current_position)) > 168u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
			goto tr333;
	} else
		goto tr333;
	goto tr16;
st181:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof181;
case 181:
	switch( (*( current_position)) ) {
		case 65u: goto tr336;
		case 69u: goto tr337;
		case 73u: goto tr336;
		case 77u: goto tr337;
		case 81u: goto tr336;
		case 85u: goto tr337;
		case 89u: goto tr336;
		case 93u: goto tr337;
		case 97u: goto tr336;
		case 101u: goto tr337;
		case 105u: goto tr336;
		case 109u: goto tr337;
		case 113u: goto tr336;
		case 117u: goto tr337;
		case 121u: goto tr338;
		case 125u: goto tr339;
		case 193u: goto tr340;
		case 197u: goto tr341;
		case 201u: goto tr340;
		case 205u: goto tr341;
		case 209u: goto tr340;
		case 213u: goto tr341;
		case 217u: goto tr340;
		case 221u: goto tr341;
		case 225u: goto tr340;
		case 229u: goto tr341;
		case 233u: goto tr340;
		case 237u: goto tr341;
		case 241u: goto tr340;
		case 245u: goto tr341;
		case 249u: goto tr340;
		case 253u: goto tr341;
	}
	goto tr16;
tr336:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st182;
st182:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof182;
case 182:
	switch( (*( current_position)) ) {
		case 33u: goto tr299;
		case 68u: goto tr342;
		case 223u: goto tr346;
	}
	if ( (*( current_position)) < 74u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
				goto tr299;
		} else if ( (*( current_position)) > 66u ) {
			if ( 72u <= (*( current_position)) && (*( current_position)) <= 73u )
				goto tr343;
		} else
			goto tr299;
	} else if ( (*( current_position)) > 76u ) {
		if ( (*( current_position)) < 104u ) {
			if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr345;
		} else if ( (*( current_position)) > 111u ) {
			if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr345;
		} else
			goto tr345;
	} else
		goto tr344;
	goto tr16;
tr343:
	{ SET_CPU_FEATURE(CPUFeature_XOP);       }
	goto st183;
st183:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof183;
case 183:
	switch( (*( current_position)) ) {
		case 4u: goto st185;
		case 5u: goto st186;
		case 12u: goto st185;
		case 13u: goto st186;
		case 20u: goto st185;
		case 21u: goto st186;
		case 28u: goto st185;
		case 29u: goto st186;
		case 36u: goto st185;
		case 37u: goto st186;
		case 44u: goto st185;
		case 45u: goto st186;
		case 52u: goto st185;
		case 53u: goto st186;
		case 60u: goto st185;
		case 61u: goto st186;
		case 68u: goto st191;
		case 76u: goto st191;
		case 84u: goto st191;
		case 92u: goto st191;
		case 100u: goto st191;
		case 108u: goto st191;
		case 116u: goto st191;
		case 124u: goto st191;
		case 132u: goto st192;
		case 140u: goto st192;
		case 148u: goto st192;
		case 156u: goto st192;
		case 164u: goto st192;
		case 172u: goto st192;
		case 180u: goto st192;
		case 188u: goto st192;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st186;
	} else if ( (*( current_position)) >= 64u )
		goto st190;
	goto st184;
tr357:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	goto st184;
tr358:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{}
	goto st184;
st184:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof184;
case 184:
	if ( (*( current_position)) < 48u ) {
		if ( (*( current_position)) < 16u ) {
			if ( (*( current_position)) <= 3u )
				goto tr353;
		} else if ( (*( current_position)) > 19u ) {
			if ( 32u <= (*( current_position)) && (*( current_position)) <= 35u )
				goto tr353;
		} else
			goto tr353;
	} else if ( (*( current_position)) > 51u ) {
		if ( (*( current_position)) < 80u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 67u )
				goto tr353;
		} else if ( (*( current_position)) > 83u ) {
			if ( (*( current_position)) > 99u ) {
				if ( 112u <= (*( current_position)) && (*( current_position)) <= 115u )
					goto tr353;
			} else if ( (*( current_position)) >= 96u )
				goto tr353;
		} else
			goto tr353;
	} else
		goto tr353;
	goto tr16;
st185:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof185;
case 185:
	switch( (*( current_position)) ) {
		case 5u: goto st186;
		case 13u: goto st186;
		case 21u: goto st186;
		case 29u: goto st186;
		case 37u: goto st186;
		case 45u: goto st186;
		case 53u: goto st186;
		case 61u: goto st186;
		case 69u: goto st186;
		case 77u: goto st186;
		case 85u: goto st186;
		case 93u: goto st186;
		case 101u: goto st186;
		case 109u: goto st186;
		case 117u: goto st186;
		case 125u: goto st186;
		case 133u: goto st186;
		case 141u: goto st186;
		case 149u: goto st186;
		case 157u: goto st186;
		case 165u: goto st186;
		case 173u: goto st186;
		case 181u: goto st186;
		case 189u: goto st186;
		case 197u: goto st186;
		case 205u: goto st186;
		case 213u: goto st186;
		case 221u: goto st186;
		case 229u: goto st186;
		case 237u: goto st186;
		case 245u: goto st186;
		case 253u: goto st186;
	}
	goto st184;
st186:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof186;
case 186:
	goto tr354;
tr354:
	{}
	goto st187;
st187:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof187;
case 187:
	goto tr355;
tr355:
	{}
	goto st188;
st188:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof188;
case 188:
	goto tr356;
tr356:
	{}
	goto st189;
st189:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof189;
case 189:
	goto tr357;
st190:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof190;
case 190:
	goto tr358;
st191:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof191;
case 191:
	goto st190;
st192:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof192;
case 192:
	goto st186;
tr337:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st193;
st193:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof193;
case 193:
	switch( (*( current_position)) ) {
		case 6u: goto tr299;
		case 64u: goto tr299;
	}
	if ( (*( current_position)) < 92u ) {
		if ( (*( current_position)) < 12u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 9u )
				goto tr299;
		} else if ( (*( current_position)) > 13u ) {
			if ( (*( current_position)) > 73u ) {
				if ( 74u <= (*( current_position)) && (*( current_position)) <= 75u )
					goto tr344;
			} else if ( (*( current_position)) >= 72u )
				goto tr343;
		} else
			goto tr299;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) < 108u ) {
			if ( 104u <= (*( current_position)) && (*( current_position)) <= 105u )
				goto tr345;
		} else if ( (*( current_position)) > 109u ) {
			if ( (*( current_position)) > 121u ) {
				if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
					goto tr345;
			} else if ( (*( current_position)) >= 120u )
				goto tr345;
		} else
			goto tr345;
	} else
		goto tr345;
	goto tr16;
tr338:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st194;
st194:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof194;
case 194:
	switch( (*( current_position)) ) {
		case 22u: goto tr299;
		case 23u: goto tr359;
		case 29u: goto tr360;
		case 32u: goto st165;
		case 68u: goto tr342;
		case 223u: goto tr346;
	}
	if ( (*( current_position)) < 72u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) > 5u ) {
				if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
					goto tr299;
			} else if ( (*( current_position)) >= 4u )
				goto tr299;
		} else if ( (*( current_position)) > 21u ) {
			if ( (*( current_position)) > 34u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 66u )
					goto tr299;
			} else if ( (*( current_position)) >= 33u )
				goto tr299;
		} else
			goto st165;
	} else if ( (*( current_position)) > 73u ) {
		if ( (*( current_position)) < 96u ) {
			if ( (*( current_position)) > 76u ) {
				if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
					goto tr345;
			} else if ( (*( current_position)) >= 74u )
				goto tr344;
		} else if ( (*( current_position)) > 99u ) {
			if ( (*( current_position)) > 111u ) {
				if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr345;
			} else if ( (*( current_position)) >= 104u )
				goto tr345;
		} else
			goto tr299;
	} else
		goto tr343;
	goto tr16;
tr359:
	{ SET_CPU_FEATURE(CPUFeature_AVX);       }
	goto st195;
st195:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof195;
case 195:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 12u: goto st35;
		case 13u: goto st36;
		case 20u: goto st35;
		case 21u: goto st36;
		case 28u: goto st35;
		case 29u: goto st36;
		case 36u: goto st35;
		case 37u: goto st36;
		case 44u: goto st35;
		case 45u: goto st36;
		case 52u: goto st35;
		case 53u: goto st36;
		case 60u: goto st35;
		case 61u: goto st36;
		case 68u: goto st41;
		case 76u: goto st41;
		case 84u: goto st41;
		case 92u: goto st41;
		case 100u: goto st41;
		case 108u: goto st41;
		case 116u: goto st41;
		case 124u: goto st41;
		case 132u: goto st42;
		case 140u: goto st42;
		case 148u: goto st42;
		case 156u: goto st42;
		case 164u: goto st42;
		case 172u: goto st42;
		case 180u: goto st42;
		case 188u: goto st42;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 63u )
			goto st10;
	} else if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st36;
	} else
		goto st40;
	goto tr16;
tr339:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st196;
st196:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof196;
case 196:
	switch( (*( current_position)) ) {
		case 29u: goto tr360;
		case 64u: goto tr299;
	}
	if ( (*( current_position)) < 74u ) {
		if ( (*( current_position)) < 12u ) {
			if ( (*( current_position)) > 6u ) {
				if ( 8u <= (*( current_position)) && (*( current_position)) <= 9u )
					goto tr299;
			} else if ( (*( current_position)) >= 4u )
				goto tr299;
		} else if ( (*( current_position)) > 13u ) {
			if ( (*( current_position)) > 25u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 73u )
					goto tr343;
			} else if ( (*( current_position)) >= 24u )
				goto tr299;
		} else
			goto tr299;
	} else if ( (*( current_position)) > 75u ) {
		if ( (*( current_position)) < 108u ) {
			if ( (*( current_position)) > 95u ) {
				if ( 104u <= (*( current_position)) && (*( current_position)) <= 105u )
					goto tr345;
			} else if ( (*( current_position)) >= 92u )
				goto tr345;
		} else if ( (*( current_position)) > 109u ) {
			if ( (*( current_position)) > 121u ) {
				if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
					goto tr345;
			} else if ( (*( current_position)) >= 120u )
				goto tr345;
		} else
			goto tr345;
	} else
		goto tr344;
	goto tr16;
tr340:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st197;
st197:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof197;
case 197:
	if ( (*( current_position)) < 92u ) {
		if ( 72u <= (*( current_position)) && (*( current_position)) <= 73u )
			goto tr343;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) > 111u ) {
			if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr345;
		} else if ( (*( current_position)) >= 104u )
			goto tr345;
	} else
		goto tr345;
	goto tr16;
tr341:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st198;
st198:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof198;
case 198:
	if ( (*( current_position)) < 104u ) {
		if ( (*( current_position)) > 73u ) {
			if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
				goto tr345;
		} else if ( (*( current_position)) >= 72u )
			goto tr343;
	} else if ( (*( current_position)) > 105u ) {
		if ( (*( current_position)) < 120u ) {
			if ( 108u <= (*( current_position)) && (*( current_position)) <= 109u )
				goto tr345;
		} else if ( (*( current_position)) > 121u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr345;
		} else
			goto tr345;
	} else
		goto tr345;
	goto tr16;
st199:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof199;
case 199:
	switch( (*( current_position)) ) {
		case 193u: goto tr362;
		case 194u: goto tr363;
		case 195u: goto tr364;
		case 196u: goto tr365;
		case 197u: goto tr366;
		case 198u: goto tr367;
		case 199u: goto tr368;
		case 201u: goto tr362;
		case 202u: goto tr363;
		case 203u: goto tr364;
		case 204u: goto tr365;
		case 205u: goto tr366;
		case 206u: goto tr367;
		case 207u: goto tr368;
		case 209u: goto tr362;
		case 210u: goto tr363;
		case 211u: goto tr364;
		case 212u: goto tr365;
		case 213u: goto tr366;
		case 214u: goto tr367;
		case 215u: goto tr368;
		case 217u: goto tr362;
		case 218u: goto tr363;
		case 219u: goto tr364;
		case 220u: goto tr365;
		case 221u: goto tr366;
		case 222u: goto tr367;
		case 223u: goto tr368;
		case 225u: goto tr362;
		case 226u: goto tr363;
		case 227u: goto tr364;
		case 228u: goto tr365;
		case 229u: goto tr366;
		case 230u: goto tr367;
		case 231u: goto tr368;
		case 233u: goto tr362;
		case 234u: goto tr363;
		case 235u: goto tr364;
		case 236u: goto tr365;
		case 237u: goto tr366;
		case 238u: goto tr367;
		case 239u: goto tr368;
		case 241u: goto tr362;
		case 242u: goto tr363;
		case 243u: goto tr364;
		case 244u: goto tr365;
		case 245u: goto tr366;
		case 246u: goto tr367;
		case 247u: goto tr368;
		case 248u: goto tr369;
		case 249u: goto tr370;
		case 250u: goto tr371;
		case 251u: goto tr372;
		case 252u: goto tr373;
		case 253u: goto tr374;
		case 254u: goto tr375;
		case 255u: goto tr376;
	}
	if ( 192u <= (*( current_position)) && (*( current_position)) <= 240u )
		goto tr361;
	goto tr16;
st200:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof200;
case 200:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 68u: goto st41;
		case 132u: goto st42;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st10;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st10;
		} else if ( (*( current_position)) >= 128u )
			goto st36;
	} else
		goto st40;
	goto tr16;
st201:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof201;
case 201:
	switch( (*( current_position)) ) {
		case 4u: goto st111;
		case 5u: goto st112;
		case 68u: goto st117;
		case 132u: goto st118;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st11;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st11;
		} else if ( (*( current_position)) >= 128u )
			goto st112;
	} else
		goto st116;
	goto tr16;
st202:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof202;
case 202:
	switch( (*( current_position)) ) {
		case 4u: goto tr378;
		case 5u: goto tr379;
		case 12u: goto tr378;
		case 13u: goto tr379;
		case 20u: goto tr378;
		case 21u: goto tr379;
		case 28u: goto tr378;
		case 29u: goto tr379;
		case 36u: goto tr378;
		case 37u: goto tr379;
		case 44u: goto tr378;
		case 45u: goto tr379;
		case 52u: goto tr378;
		case 53u: goto tr379;
		case 60u: goto tr378;
		case 61u: goto tr379;
		case 68u: goto tr381;
		case 76u: goto tr381;
		case 84u: goto tr381;
		case 92u: goto tr381;
		case 100u: goto tr381;
		case 108u: goto tr381;
		case 116u: goto tr381;
		case 124u: goto tr381;
		case 132u: goto tr382;
		case 140u: goto tr382;
		case 148u: goto tr382;
		case 156u: goto tr382;
		case 164u: goto tr382;
		case 172u: goto tr382;
		case 180u: goto tr382;
		case 188u: goto tr382;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr379;
	} else if ( (*( current_position)) >= 64u )
		goto tr380;
	goto tr377;
st203:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof203;
case 203:
	switch( (*( current_position)) ) {
		case 4u: goto tr378;
		case 5u: goto tr379;
		case 20u: goto tr378;
		case 21u: goto tr379;
		case 28u: goto tr378;
		case 29u: goto tr379;
		case 36u: goto tr378;
		case 37u: goto tr379;
		case 44u: goto tr378;
		case 45u: goto tr379;
		case 52u: goto tr378;
		case 53u: goto tr379;
		case 60u: goto tr378;
		case 61u: goto tr379;
		case 68u: goto tr381;
		case 84u: goto tr381;
		case 92u: goto tr381;
		case 100u: goto tr381;
		case 108u: goto tr381;
		case 116u: goto tr381;
		case 124u: goto tr381;
		case 132u: goto tr382;
		case 148u: goto tr382;
		case 156u: goto tr382;
		case 164u: goto tr382;
		case 172u: goto tr382;
		case 180u: goto tr382;
		case 188u: goto tr382;
		case 239u: goto tr16;
	}
	if ( (*( current_position)) < 128u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
				goto tr16;
		} else if ( (*( current_position)) > 71u ) {
			if ( (*( current_position)) > 79u ) {
				if ( 80u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr380;
			} else if ( (*( current_position)) >= 72u )
				goto tr16;
		} else
			goto tr380;
	} else if ( (*( current_position)) > 135u ) {
		if ( (*( current_position)) < 209u ) {
			if ( (*( current_position)) > 143u ) {
				if ( 144u <= (*( current_position)) && (*( current_position)) <= 191u )
					goto tr379;
			} else if ( (*( current_position)) >= 136u )
				goto tr16;
		} else if ( (*( current_position)) > 223u ) {
			if ( (*( current_position)) > 227u ) {
				if ( 230u <= (*( current_position)) && (*( current_position)) <= 231u )
					goto tr16;
			} else if ( (*( current_position)) >= 226u )
				goto tr16;
		} else
			goto tr16;
	} else
		goto tr379;
	goto tr377;
st204:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof204;
case 204:
	switch( (*( current_position)) ) {
		case 4u: goto tr378;
		case 12u: goto tr378;
		case 20u: goto tr378;
		case 28u: goto tr378;
		case 36u: goto tr378;
		case 44u: goto tr378;
		case 52u: goto tr378;
		case 60u: goto tr378;
		case 68u: goto tr381;
		case 76u: goto tr381;
		case 84u: goto tr381;
		case 92u: goto tr381;
		case 100u: goto tr381;
		case 108u: goto tr381;
		case 116u: goto tr381;
		case 124u: goto tr381;
		case 132u: goto tr382;
		case 140u: goto tr382;
		case 148u: goto tr382;
		case 156u: goto tr382;
		case 164u: goto tr382;
		case 172u: goto tr382;
		case 180u: goto tr382;
		case 188u: goto tr382;
		case 233u: goto tr377;
	}
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr377;
			} else
				goto tr377;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr377;
			} else if ( (*( current_position)) >= 22u )
				goto tr377;
		} else
			goto tr377;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr377;
			} else if ( (*( current_position)) >= 46u )
				goto tr377;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) < 192u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr380;
			} else if ( (*( current_position)) > 223u ) {
				if ( 224u <= (*( current_position)) )
					goto tr16;
			} else
				goto tr383;
		} else
			goto tr377;
	} else
		goto tr377;
	goto tr379;
st205:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof205;
case 205:
	switch( (*( current_position)) ) {
		case 4u: goto tr378;
		case 5u: goto tr379;
		case 12u: goto tr378;
		case 13u: goto tr379;
		case 20u: goto tr378;
		case 21u: goto tr379;
		case 28u: goto tr378;
		case 29u: goto tr379;
		case 44u: goto tr378;
		case 45u: goto tr379;
		case 60u: goto tr378;
		case 61u: goto tr379;
		case 68u: goto tr381;
		case 76u: goto tr381;
		case 84u: goto tr381;
		case 92u: goto tr381;
		case 108u: goto tr381;
		case 124u: goto tr381;
		case 132u: goto tr382;
		case 140u: goto tr382;
		case 148u: goto tr382;
		case 156u: goto tr382;
		case 172u: goto tr382;
		case 188u: goto tr382;
	}
	if ( (*( current_position)) < 120u ) {
		if ( (*( current_position)) < 56u ) {
			if ( (*( current_position)) > 31u ) {
				if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr377;
			} else
				goto tr377;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 95u ) {
				if ( 104u <= (*( current_position)) && (*( current_position)) <= 111u )
					goto tr380;
			} else if ( (*( current_position)) >= 64u )
				goto tr380;
		} else
			goto tr377;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 184u ) {
			if ( (*( current_position)) > 159u ) {
				if ( 168u <= (*( current_position)) && (*( current_position)) <= 175u )
					goto tr379;
			} else if ( (*( current_position)) >= 128u )
				goto tr379;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) < 226u ) {
				if ( 192u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr383;
			} else if ( (*( current_position)) > 227u ) {
				if ( 232u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr377;
			} else
				goto tr377;
		} else
			goto tr379;
	} else
		goto tr380;
	goto tr16;
st206:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof206;
case 206:
	switch( (*( current_position)) ) {
		case 4u: goto tr378;
		case 5u: goto tr379;
		case 12u: goto tr378;
		case 13u: goto tr379;
		case 20u: goto tr378;
		case 21u: goto tr379;
		case 28u: goto tr378;
		case 29u: goto tr379;
		case 36u: goto tr378;
		case 37u: goto tr379;
		case 44u: goto tr378;
		case 45u: goto tr379;
		case 52u: goto tr378;
		case 53u: goto tr379;
		case 60u: goto tr378;
		case 61u: goto tr379;
		case 68u: goto tr381;
		case 76u: goto tr381;
		case 84u: goto tr381;
		case 92u: goto tr381;
		case 100u: goto tr381;
		case 108u: goto tr381;
		case 116u: goto tr381;
		case 124u: goto tr381;
		case 132u: goto tr382;
		case 140u: goto tr382;
		case 148u: goto tr382;
		case 156u: goto tr382;
		case 164u: goto tr382;
		case 172u: goto tr382;
		case 180u: goto tr382;
		case 188u: goto tr382;
	}
	if ( (*( current_position)) < 128u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto tr380;
	} else if ( (*( current_position)) > 191u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 223u )
			goto tr16;
	} else
		goto tr379;
	goto tr377;
st207:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof207;
case 207:
	switch( (*( current_position)) ) {
		case 4u: goto tr378;
		case 5u: goto tr379;
		case 12u: goto tr378;
		case 13u: goto tr379;
		case 20u: goto tr378;
		case 21u: goto tr379;
		case 28u: goto tr378;
		case 29u: goto tr379;
		case 36u: goto tr378;
		case 37u: goto tr379;
		case 52u: goto tr378;
		case 53u: goto tr379;
		case 60u: goto tr378;
		case 61u: goto tr379;
		case 68u: goto tr381;
		case 76u: goto tr381;
		case 84u: goto tr381;
		case 92u: goto tr381;
		case 100u: goto tr381;
		case 116u: goto tr381;
		case 124u: goto tr381;
		case 132u: goto tr382;
		case 140u: goto tr382;
		case 148u: goto tr382;
		case 156u: goto tr382;
		case 164u: goto tr382;
		case 180u: goto tr382;
		case 188u: goto tr382;
	}
	if ( (*( current_position)) < 128u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr16;
		} else if ( (*( current_position)) > 103u ) {
			if ( (*( current_position)) > 111u ) {
				if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr380;
			} else if ( (*( current_position)) >= 104u )
				goto tr16;
		} else
			goto tr380;
	} else if ( (*( current_position)) > 167u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 168u <= (*( current_position)) && (*( current_position)) <= 175u )
				goto tr16;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) )
					goto tr16;
			} else if ( (*( current_position)) >= 200u )
				goto tr16;
		} else
			goto tr379;
	} else
		goto tr379;
	goto tr377;
st208:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof208;
case 208:
	switch( (*( current_position)) ) {
		case 4u: goto tr378;
		case 5u: goto tr379;
		case 12u: goto tr378;
		case 13u: goto tr379;
		case 20u: goto tr378;
		case 21u: goto tr379;
		case 28u: goto tr378;
		case 29u: goto tr379;
		case 36u: goto tr378;
		case 37u: goto tr379;
		case 44u: goto tr378;
		case 45u: goto tr379;
		case 52u: goto tr378;
		case 53u: goto tr379;
		case 60u: goto tr378;
		case 61u: goto tr379;
		case 68u: goto tr381;
		case 76u: goto tr381;
		case 84u: goto tr381;
		case 92u: goto tr381;
		case 100u: goto tr381;
		case 108u: goto tr381;
		case 116u: goto tr381;
		case 124u: goto tr381;
		case 132u: goto tr382;
		case 140u: goto tr382;
		case 148u: goto tr382;
		case 156u: goto tr382;
		case 164u: goto tr382;
		case 172u: goto tr382;
		case 180u: goto tr382;
		case 188u: goto tr382;
	}
	if ( (*( current_position)) < 128u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto tr380;
	} else if ( (*( current_position)) > 191u ) {
		if ( (*( current_position)) > 216u ) {
			if ( 218u <= (*( current_position)) && (*( current_position)) <= 223u )
				goto tr16;
		} else if ( (*( current_position)) >= 208u )
			goto tr16;
	} else
		goto tr379;
	goto tr377;
st209:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof209;
case 209:
	switch( (*( current_position)) ) {
		case 4u: goto tr378;
		case 5u: goto tr379;
		case 12u: goto tr378;
		case 13u: goto tr379;
		case 20u: goto tr378;
		case 21u: goto tr379;
		case 28u: goto tr378;
		case 29u: goto tr379;
		case 36u: goto tr378;
		case 37u: goto tr379;
		case 44u: goto tr378;
		case 45u: goto tr379;
		case 52u: goto tr378;
		case 53u: goto tr379;
		case 60u: goto tr378;
		case 61u: goto tr379;
		case 68u: goto tr381;
		case 76u: goto tr381;
		case 84u: goto tr381;
		case 92u: goto tr381;
		case 100u: goto tr381;
		case 108u: goto tr381;
		case 116u: goto tr381;
		case 124u: goto tr381;
		case 132u: goto tr382;
		case 140u: goto tr382;
		case 148u: goto tr382;
		case 156u: goto tr382;
		case 164u: goto tr382;
		case 172u: goto tr382;
		case 180u: goto tr382;
		case 188u: goto tr382;
	}
	if ( (*( current_position)) < 192u ) {
		if ( (*( current_position)) > 127u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
				goto tr379;
		} else if ( (*( current_position)) >= 64u )
			goto tr380;
	} else if ( (*( current_position)) > 223u ) {
		if ( (*( current_position)) > 231u ) {
			if ( 248u <= (*( current_position)) )
				goto tr16;
		} else if ( (*( current_position)) >= 225u )
			goto tr16;
	} else
		goto tr16;
	goto tr377;
st210:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof210;
case 210:
	goto tr384;
tr384:
	{}
	goto st211;
st211:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof211;
case 211:
	goto tr385;
tr385:
	{}
	goto st212;
st212:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof212;
case 212:
	goto tr386;
tr386:
	{}
	goto st213;
st213:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof213;
case 213:
	goto tr387;
st214:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof214;
case 214:
	switch( (*( current_position)) ) {
		case 15u: goto st215;
		case 102u: goto st99;
		case 128u: goto st101;
		case 129u: goto st216;
		case 131u: goto st101;
	}
	if ( (*( current_position)) < 32u ) {
		if ( (*( current_position)) < 8u ) {
			if ( (*( current_position)) <= 1u )
				goto st29;
		} else if ( (*( current_position)) > 9u ) {
			if ( (*( current_position)) > 17u ) {
				if ( 24u <= (*( current_position)) && (*( current_position)) <= 25u )
					goto st29;
			} else if ( (*( current_position)) >= 16u )
				goto st29;
		} else
			goto st29;
	} else if ( (*( current_position)) > 33u ) {
		if ( (*( current_position)) < 134u ) {
			if ( (*( current_position)) > 41u ) {
				if ( 48u <= (*( current_position)) && (*( current_position)) <= 49u )
					goto st29;
			} else if ( (*( current_position)) >= 40u )
				goto st29;
		} else if ( (*( current_position)) > 135u ) {
			if ( (*( current_position)) > 247u ) {
				if ( 254u <= (*( current_position)) )
					goto st103;
			} else if ( (*( current_position)) >= 246u )
				goto st102;
		} else
			goto st29;
	} else
		goto st29;
	goto tr16;
st215:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof215;
case 215:
	if ( (*( current_position)) == 199u )
		goto st53;
	if ( (*( current_position)) > 177u ) {
		if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
			goto st29;
	} else if ( (*( current_position)) >= 176u )
		goto st29;
	goto tr16;
st216:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof216;
case 216:
	switch( (*( current_position)) ) {
		case 4u: goto st111;
		case 5u: goto st112;
		case 12u: goto st111;
		case 13u: goto st112;
		case 20u: goto st111;
		case 21u: goto st112;
		case 28u: goto st111;
		case 29u: goto st112;
		case 36u: goto st111;
		case 37u: goto st112;
		case 44u: goto st111;
		case 45u: goto st112;
		case 52u: goto st111;
		case 53u: goto st112;
		case 68u: goto st117;
		case 76u: goto st117;
		case 84u: goto st117;
		case 92u: goto st117;
		case 100u: goto st117;
		case 108u: goto st117;
		case 116u: goto st117;
		case 132u: goto st118;
		case 140u: goto st118;
		case 148u: goto st118;
		case 156u: goto st118;
		case 164u: goto st118;
		case 172u: goto st118;
		case 180u: goto st118;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st11;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st112;
	} else
		goto st116;
	goto tr16;
st217:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof217;
case 217:
	switch( (*( current_position)) ) {
		case 15u: goto st218;
		case 102u: goto st104;
	}
	if ( (*( current_position)) > 167u ) {
		if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
			goto tr0;
	} else if ( (*( current_position)) >= 166u )
		goto tr0;
	goto tr16;
st218:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof218;
case 218:
	switch( (*( current_position)) ) {
		case 18u: goto tr392;
		case 43u: goto tr393;
		case 56u: goto st219;
		case 81u: goto tr391;
		case 112u: goto tr395;
		case 120u: goto tr396;
		case 121u: goto tr397;
		case 194u: goto tr395;
		case 208u: goto tr398;
		case 214u: goto tr399;
		case 230u: goto tr391;
		case 240u: goto tr400;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 42u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto tr391;
		} else if ( (*( current_position)) >= 16u )
			goto tr391;
	} else if ( (*( current_position)) > 90u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto tr392;
		} else if ( (*( current_position)) >= 92u )
			goto tr391;
	} else
		goto tr391;
	goto tr16;
st219:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof219;
case 219:
	if ( 240u <= (*( current_position)) && (*( current_position)) <= 241u )
		goto tr210;
	goto tr16;
tr396:
	{
    SET_REPNZ_PREFIX(FALSE);
  }
	{ SET_CPU_FEATURE(CPUFeature_SSE4A);     }
	goto st220;
st220:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof220;
case 220:
	if ( 192u <= (*( current_position)) )
		goto st221;
	goto tr16;
st221:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof221;
case 221:
	goto tr402;
st222:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof222;
case 222:
	switch( (*( current_position)) ) {
		case 15u: goto st223;
		case 102u: goto st107;
		case 144u: goto tr404;
	}
	if ( (*( current_position)) < 170u ) {
		if ( 164u <= (*( current_position)) && (*( current_position)) <= 167u )
			goto tr0;
	} else if ( (*( current_position)) > 171u ) {
		if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
			goto tr0;
	} else
		goto tr0;
	goto tr16;
st223:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof223;
case 223:
	switch( (*( current_position)) ) {
		case 18u: goto tr406;
		case 22u: goto tr406;
		case 43u: goto tr407;
		case 111u: goto tr408;
		case 112u: goto tr409;
		case 184u: goto tr410;
		case 188u: goto tr411;
		case 189u: goto tr412;
		case 194u: goto tr413;
		case 214u: goto tr414;
		case 230u: goto tr408;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) < 42u ) {
			if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
				goto tr405;
		} else if ( (*( current_position)) > 45u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 83u )
				goto tr405;
		} else
			goto tr405;
	} else if ( (*( current_position)) > 89u ) {
		if ( (*( current_position)) < 92u ) {
			if ( 90u <= (*( current_position)) && (*( current_position)) <= 91u )
				goto tr408;
		} else if ( (*( current_position)) > 95u ) {
			if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto tr408;
		} else
			goto tr405;
	} else
		goto tr405;
	goto tr16;
st224:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof224;
case 224:
	switch( (*( current_position)) ) {
		case 4u: goto st35;
		case 5u: goto st36;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st41;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st42;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*( current_position)) < 80u ) {
		if ( (*( current_position)) < 8u ) {
			if ( (*( current_position)) <= 7u )
				goto st10;
		} else if ( (*( current_position)) > 15u ) {
			if ( (*( current_position)) > 71u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto st40;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr16;
			} else if ( (*( current_position)) >= 128u )
				goto st36;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 199u ) {
				if ( 200u <= (*( current_position)) && (*( current_position)) <= 207u )
					goto tr16;
			} else if ( (*( current_position)) >= 192u )
				goto st10;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
st225:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof225;
case 225:
	switch( (*( current_position)) ) {
		case 4u: goto st111;
		case 5u: goto st112;
		case 20u: goto st2;
		case 21u: goto st3;
		case 28u: goto st2;
		case 29u: goto st3;
		case 36u: goto st2;
		case 37u: goto st3;
		case 44u: goto st2;
		case 45u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 60u: goto st2;
		case 61u: goto st3;
		case 68u: goto st117;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st118;
		case 148u: goto st9;
		case 156u: goto st9;
		case 164u: goto st9;
		case 172u: goto st9;
		case 180u: goto st9;
		case 188u: goto st9;
	}
	if ( (*( current_position)) < 80u ) {
		if ( (*( current_position)) < 8u ) {
			if ( (*( current_position)) <= 7u )
				goto st11;
		} else if ( (*( current_position)) > 15u ) {
			if ( (*( current_position)) > 71u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto st116;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr16;
			} else if ( (*( current_position)) >= 128u )
				goto st112;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 199u ) {
				if ( 200u <= (*( current_position)) && (*( current_position)) <= 207u )
					goto tr16;
			} else if ( (*( current_position)) >= 192u )
				goto st11;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
st226:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof226;
case 226:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr0;
	} else if ( (*( current_position)) > 79u ) {
		if ( (*( current_position)) > 143u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 207u )
				goto tr0;
		} else if ( (*( current_position)) >= 128u )
			goto st3;
	} else
		goto st7;
	goto tr16;
st227:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof227;
case 227:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 208u: goto tr415;
		case 224u: goto tr416;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st228:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof228;
case 228:
	if ( (*( current_position)) == 224u )
		goto tr417;
	goto tr11;
tr417:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st244;
st244:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof244;
case 244:
	switch( (*( current_position)) ) {
		case 4u: goto st10;
		case 5u: goto st11;
		case 12u: goto st10;
		case 13u: goto st11;
		case 15u: goto st15;
		case 20u: goto st10;
		case 21u: goto st11;
		case 28u: goto st10;
		case 29u: goto st11;
		case 36u: goto st10;
		case 37u: goto st11;
		case 44u: goto st10;
		case 45u: goto st11;
		case 46u: goto st54;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st54;
		case 101u: goto st57;
		case 102u: goto st63;
		case 104u: goto st11;
		case 105u: goto st110;
		case 106u: goto st10;
		case 107u: goto st34;
		case 128u: goto st34;
		case 129u: goto st110;
		case 131u: goto st119;
		case 141u: goto st29;
		case 143u: goto st121;
		case 155u: goto tr377;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st149;
		case 197u: goto st199;
		case 198u: goto st200;
		case 199u: goto st201;
		case 201u: goto tr0;
		case 216u: goto st202;
		case 217u: goto st203;
		case 218u: goto st204;
		case 219u: goto st205;
		case 220u: goto st206;
		case 221u: goto st207;
		case 222u: goto st208;
		case 223u: goto st209;
		case 232u: goto st210;
		case 233u: goto st46;
		case 235u: goto st56;
		case 240u: goto st214;
		case 242u: goto st217;
		case 243u: goto st222;
		case 246u: goto st224;
		case 247u: goto st225;
		case 254u: goto st226;
		case 255u: goto st229;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) <= 3u )
					goto st1;
			} else if ( (*( current_position)) > 11u ) {
				if ( (*( current_position)) > 19u ) {
					if ( 24u <= (*( current_position)) && (*( current_position)) <= 27u )
						goto st1;
				} else if ( (*( current_position)) >= 16u )
					goto st1;
			} else
				goto st1;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 56u ) {
				if ( (*( current_position)) > 43u ) {
					if ( 48u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto st1;
				} else if ( (*( current_position)) >= 40u )
					goto st1;
			} else if ( (*( current_position)) > 59u ) {
				if ( (*( current_position)) > 95u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto st56;
				} else if ( (*( current_position)) >= 64u )
					goto tr0;
			} else
				goto st1;
		} else
			goto st1;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 176u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 153u ) {
					if ( 158u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto tr0;
				} else if ( (*( current_position)) >= 144u )
					goto tr0;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) > 171u ) {
					if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
						goto tr0;
				} else if ( (*( current_position)) >= 164u )
					goto tr0;
			} else
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto st96;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st98;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
st229:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof229;
case 229:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 209u: goto tr415;
		case 225u: goto tr416;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st230:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof230;
case 230:
	if ( (*( current_position)) == 224u )
		goto tr418;
	goto tr11;
tr418:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st245;
st245:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof245;
case 245:
	switch( (*( current_position)) ) {
		case 4u: goto st10;
		case 5u: goto st11;
		case 12u: goto st10;
		case 13u: goto st11;
		case 15u: goto st15;
		case 20u: goto st10;
		case 21u: goto st11;
		case 28u: goto st10;
		case 29u: goto st11;
		case 36u: goto st10;
		case 37u: goto st11;
		case 44u: goto st10;
		case 45u: goto st11;
		case 46u: goto st54;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st54;
		case 101u: goto st57;
		case 102u: goto st63;
		case 104u: goto st11;
		case 105u: goto st110;
		case 106u: goto st10;
		case 107u: goto st34;
		case 128u: goto st34;
		case 129u: goto st110;
		case 131u: goto st119;
		case 141u: goto st29;
		case 143u: goto st121;
		case 155u: goto tr377;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st149;
		case 197u: goto st199;
		case 198u: goto st200;
		case 199u: goto st201;
		case 201u: goto tr0;
		case 216u: goto st202;
		case 217u: goto st203;
		case 218u: goto st204;
		case 219u: goto st205;
		case 220u: goto st206;
		case 221u: goto st207;
		case 222u: goto st208;
		case 223u: goto st209;
		case 232u: goto st210;
		case 233u: goto st46;
		case 235u: goto st56;
		case 240u: goto st214;
		case 242u: goto st217;
		case 243u: goto st222;
		case 246u: goto st224;
		case 247u: goto st225;
		case 254u: goto st226;
		case 255u: goto st231;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) <= 3u )
					goto st1;
			} else if ( (*( current_position)) > 11u ) {
				if ( (*( current_position)) > 19u ) {
					if ( 24u <= (*( current_position)) && (*( current_position)) <= 27u )
						goto st1;
				} else if ( (*( current_position)) >= 16u )
					goto st1;
			} else
				goto st1;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 56u ) {
				if ( (*( current_position)) > 43u ) {
					if ( 48u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto st1;
				} else if ( (*( current_position)) >= 40u )
					goto st1;
			} else if ( (*( current_position)) > 59u ) {
				if ( (*( current_position)) > 95u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto st56;
				} else if ( (*( current_position)) >= 64u )
					goto tr0;
			} else
				goto st1;
		} else
			goto st1;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 176u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 153u ) {
					if ( 158u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto tr0;
				} else if ( (*( current_position)) >= 144u )
					goto tr0;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) > 171u ) {
					if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
						goto tr0;
				} else if ( (*( current_position)) >= 164u )
					goto tr0;
			} else
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto st96;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st98;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
st231:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof231;
case 231:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 210u: goto tr415;
		case 226u: goto tr416;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st232:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof232;
case 232:
	if ( (*( current_position)) == 224u )
		goto tr419;
	goto tr11;
tr419:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st246;
st246:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof246;
case 246:
	switch( (*( current_position)) ) {
		case 4u: goto st10;
		case 5u: goto st11;
		case 12u: goto st10;
		case 13u: goto st11;
		case 15u: goto st15;
		case 20u: goto st10;
		case 21u: goto st11;
		case 28u: goto st10;
		case 29u: goto st11;
		case 36u: goto st10;
		case 37u: goto st11;
		case 44u: goto st10;
		case 45u: goto st11;
		case 46u: goto st54;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st54;
		case 101u: goto st57;
		case 102u: goto st63;
		case 104u: goto st11;
		case 105u: goto st110;
		case 106u: goto st10;
		case 107u: goto st34;
		case 128u: goto st34;
		case 129u: goto st110;
		case 131u: goto st119;
		case 141u: goto st29;
		case 143u: goto st121;
		case 155u: goto tr377;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st149;
		case 197u: goto st199;
		case 198u: goto st200;
		case 199u: goto st201;
		case 201u: goto tr0;
		case 216u: goto st202;
		case 217u: goto st203;
		case 218u: goto st204;
		case 219u: goto st205;
		case 220u: goto st206;
		case 221u: goto st207;
		case 222u: goto st208;
		case 223u: goto st209;
		case 232u: goto st210;
		case 233u: goto st46;
		case 235u: goto st56;
		case 240u: goto st214;
		case 242u: goto st217;
		case 243u: goto st222;
		case 246u: goto st224;
		case 247u: goto st225;
		case 254u: goto st226;
		case 255u: goto st233;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) <= 3u )
					goto st1;
			} else if ( (*( current_position)) > 11u ) {
				if ( (*( current_position)) > 19u ) {
					if ( 24u <= (*( current_position)) && (*( current_position)) <= 27u )
						goto st1;
				} else if ( (*( current_position)) >= 16u )
					goto st1;
			} else
				goto st1;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 56u ) {
				if ( (*( current_position)) > 43u ) {
					if ( 48u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto st1;
				} else if ( (*( current_position)) >= 40u )
					goto st1;
			} else if ( (*( current_position)) > 59u ) {
				if ( (*( current_position)) > 95u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto st56;
				} else if ( (*( current_position)) >= 64u )
					goto tr0;
			} else
				goto st1;
		} else
			goto st1;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 176u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 153u ) {
					if ( 158u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto tr0;
				} else if ( (*( current_position)) >= 144u )
					goto tr0;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) > 171u ) {
					if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
						goto tr0;
				} else if ( (*( current_position)) >= 164u )
					goto tr0;
			} else
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto st96;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st98;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
st233:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof233;
case 233:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 211u: goto tr415;
		case 227u: goto tr416;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st234:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof234;
case 234:
	if ( (*( current_position)) == 224u )
		goto tr420;
	goto tr11;
tr420:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st247;
st247:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof247;
case 247:
	switch( (*( current_position)) ) {
		case 4u: goto st10;
		case 5u: goto st11;
		case 12u: goto st10;
		case 13u: goto st11;
		case 15u: goto st15;
		case 20u: goto st10;
		case 21u: goto st11;
		case 28u: goto st10;
		case 29u: goto st11;
		case 36u: goto st10;
		case 37u: goto st11;
		case 44u: goto st10;
		case 45u: goto st11;
		case 46u: goto st54;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st54;
		case 101u: goto st57;
		case 102u: goto st63;
		case 104u: goto st11;
		case 105u: goto st110;
		case 106u: goto st10;
		case 107u: goto st34;
		case 128u: goto st34;
		case 129u: goto st110;
		case 131u: goto st119;
		case 141u: goto st29;
		case 143u: goto st121;
		case 155u: goto tr377;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st149;
		case 197u: goto st199;
		case 198u: goto st200;
		case 199u: goto st201;
		case 201u: goto tr0;
		case 216u: goto st202;
		case 217u: goto st203;
		case 218u: goto st204;
		case 219u: goto st205;
		case 220u: goto st206;
		case 221u: goto st207;
		case 222u: goto st208;
		case 223u: goto st209;
		case 232u: goto st210;
		case 233u: goto st46;
		case 235u: goto st56;
		case 240u: goto st214;
		case 242u: goto st217;
		case 243u: goto st222;
		case 246u: goto st224;
		case 247u: goto st225;
		case 254u: goto st226;
		case 255u: goto st235;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) <= 3u )
					goto st1;
			} else if ( (*( current_position)) > 11u ) {
				if ( (*( current_position)) > 19u ) {
					if ( 24u <= (*( current_position)) && (*( current_position)) <= 27u )
						goto st1;
				} else if ( (*( current_position)) >= 16u )
					goto st1;
			} else
				goto st1;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 56u ) {
				if ( (*( current_position)) > 43u ) {
					if ( 48u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto st1;
				} else if ( (*( current_position)) >= 40u )
					goto st1;
			} else if ( (*( current_position)) > 59u ) {
				if ( (*( current_position)) > 95u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto st56;
				} else if ( (*( current_position)) >= 64u )
					goto tr0;
			} else
				goto st1;
		} else
			goto st1;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 176u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 153u ) {
					if ( 158u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto tr0;
				} else if ( (*( current_position)) >= 144u )
					goto tr0;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) > 171u ) {
					if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
						goto tr0;
				} else if ( (*( current_position)) >= 164u )
					goto tr0;
			} else
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto st96;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st98;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
st235:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof235;
case 235:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 212u: goto tr415;
		case 228u: goto tr416;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st236:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof236;
case 236:
	if ( (*( current_position)) == 224u )
		goto tr421;
	goto tr11;
tr421:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st248;
st248:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof248;
case 248:
	switch( (*( current_position)) ) {
		case 4u: goto st10;
		case 5u: goto st11;
		case 12u: goto st10;
		case 13u: goto st11;
		case 15u: goto st15;
		case 20u: goto st10;
		case 21u: goto st11;
		case 28u: goto st10;
		case 29u: goto st11;
		case 36u: goto st10;
		case 37u: goto st11;
		case 44u: goto st10;
		case 45u: goto st11;
		case 46u: goto st54;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st54;
		case 101u: goto st57;
		case 102u: goto st63;
		case 104u: goto st11;
		case 105u: goto st110;
		case 106u: goto st10;
		case 107u: goto st34;
		case 128u: goto st34;
		case 129u: goto st110;
		case 131u: goto st119;
		case 141u: goto st29;
		case 143u: goto st121;
		case 155u: goto tr377;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st149;
		case 197u: goto st199;
		case 198u: goto st200;
		case 199u: goto st201;
		case 201u: goto tr0;
		case 216u: goto st202;
		case 217u: goto st203;
		case 218u: goto st204;
		case 219u: goto st205;
		case 220u: goto st206;
		case 221u: goto st207;
		case 222u: goto st208;
		case 223u: goto st209;
		case 232u: goto st210;
		case 233u: goto st46;
		case 235u: goto st56;
		case 240u: goto st214;
		case 242u: goto st217;
		case 243u: goto st222;
		case 246u: goto st224;
		case 247u: goto st225;
		case 254u: goto st226;
		case 255u: goto st237;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) <= 3u )
					goto st1;
			} else if ( (*( current_position)) > 11u ) {
				if ( (*( current_position)) > 19u ) {
					if ( 24u <= (*( current_position)) && (*( current_position)) <= 27u )
						goto st1;
				} else if ( (*( current_position)) >= 16u )
					goto st1;
			} else
				goto st1;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 56u ) {
				if ( (*( current_position)) > 43u ) {
					if ( 48u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto st1;
				} else if ( (*( current_position)) >= 40u )
					goto st1;
			} else if ( (*( current_position)) > 59u ) {
				if ( (*( current_position)) > 95u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto st56;
				} else if ( (*( current_position)) >= 64u )
					goto tr0;
			} else
				goto st1;
		} else
			goto st1;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 176u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 153u ) {
					if ( 158u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto tr0;
				} else if ( (*( current_position)) >= 144u )
					goto tr0;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) > 171u ) {
					if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
						goto tr0;
				} else if ( (*( current_position)) >= 164u )
					goto tr0;
			} else
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto st96;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st98;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
st237:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof237;
case 237:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 213u: goto tr415;
		case 229u: goto tr416;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st238:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof238;
case 238:
	if ( (*( current_position)) == 224u )
		goto tr422;
	goto tr11;
tr422:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st249;
st249:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof249;
case 249:
	switch( (*( current_position)) ) {
		case 4u: goto st10;
		case 5u: goto st11;
		case 12u: goto st10;
		case 13u: goto st11;
		case 15u: goto st15;
		case 20u: goto st10;
		case 21u: goto st11;
		case 28u: goto st10;
		case 29u: goto st11;
		case 36u: goto st10;
		case 37u: goto st11;
		case 44u: goto st10;
		case 45u: goto st11;
		case 46u: goto st54;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st54;
		case 101u: goto st57;
		case 102u: goto st63;
		case 104u: goto st11;
		case 105u: goto st110;
		case 106u: goto st10;
		case 107u: goto st34;
		case 128u: goto st34;
		case 129u: goto st110;
		case 131u: goto st119;
		case 141u: goto st29;
		case 143u: goto st121;
		case 155u: goto tr377;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st149;
		case 197u: goto st199;
		case 198u: goto st200;
		case 199u: goto st201;
		case 201u: goto tr0;
		case 216u: goto st202;
		case 217u: goto st203;
		case 218u: goto st204;
		case 219u: goto st205;
		case 220u: goto st206;
		case 221u: goto st207;
		case 222u: goto st208;
		case 223u: goto st209;
		case 232u: goto st210;
		case 233u: goto st46;
		case 235u: goto st56;
		case 240u: goto st214;
		case 242u: goto st217;
		case 243u: goto st222;
		case 246u: goto st224;
		case 247u: goto st225;
		case 254u: goto st226;
		case 255u: goto st239;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) <= 3u )
					goto st1;
			} else if ( (*( current_position)) > 11u ) {
				if ( (*( current_position)) > 19u ) {
					if ( 24u <= (*( current_position)) && (*( current_position)) <= 27u )
						goto st1;
				} else if ( (*( current_position)) >= 16u )
					goto st1;
			} else
				goto st1;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 56u ) {
				if ( (*( current_position)) > 43u ) {
					if ( 48u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto st1;
				} else if ( (*( current_position)) >= 40u )
					goto st1;
			} else if ( (*( current_position)) > 59u ) {
				if ( (*( current_position)) > 95u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto st56;
				} else if ( (*( current_position)) >= 64u )
					goto tr0;
			} else
				goto st1;
		} else
			goto st1;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 176u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 153u ) {
					if ( 158u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto tr0;
				} else if ( (*( current_position)) >= 144u )
					goto tr0;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) > 171u ) {
					if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
						goto tr0;
				} else if ( (*( current_position)) >= 164u )
					goto tr0;
			} else
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto st96;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st98;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
st239:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof239;
case 239:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 214u: goto tr415;
		case 230u: goto tr416;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
st240:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof240;
case 240:
	if ( (*( current_position)) == 224u )
		goto tr423;
	goto tr11;
tr423:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_start, current_position,
                                 instruction_info_collected, callback_data);
    }
    /* On successful match the instruction start must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_start = current_position + 1;
    /* Mark this position as a valid target for jump.  */
    MarkValidJumpTarget(current_position + 1 - data, valid_targets);
    instruction_info_collected = 0;
  }
	goto st250;
st250:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof250;
case 250:
	switch( (*( current_position)) ) {
		case 4u: goto st10;
		case 5u: goto st11;
		case 12u: goto st10;
		case 13u: goto st11;
		case 15u: goto st15;
		case 20u: goto st10;
		case 21u: goto st11;
		case 28u: goto st10;
		case 29u: goto st11;
		case 36u: goto st10;
		case 37u: goto st11;
		case 44u: goto st10;
		case 45u: goto st11;
		case 46u: goto st54;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st54;
		case 101u: goto st57;
		case 102u: goto st63;
		case 104u: goto st11;
		case 105u: goto st110;
		case 106u: goto st10;
		case 107u: goto st34;
		case 128u: goto st34;
		case 129u: goto st110;
		case 131u: goto st119;
		case 141u: goto st29;
		case 143u: goto st121;
		case 155u: goto tr377;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st149;
		case 197u: goto st199;
		case 198u: goto st200;
		case 199u: goto st201;
		case 201u: goto tr0;
		case 216u: goto st202;
		case 217u: goto st203;
		case 218u: goto st204;
		case 219u: goto st205;
		case 220u: goto st206;
		case 221u: goto st207;
		case 222u: goto st208;
		case 223u: goto st209;
		case 232u: goto st210;
		case 233u: goto st46;
		case 235u: goto st56;
		case 240u: goto st214;
		case 242u: goto st217;
		case 243u: goto st222;
		case 246u: goto st224;
		case 247u: goto st225;
		case 254u: goto st226;
		case 255u: goto st241;
	}
	if ( (*( current_position)) < 132u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) < 8u ) {
				if ( (*( current_position)) <= 3u )
					goto st1;
			} else if ( (*( current_position)) > 11u ) {
				if ( (*( current_position)) > 19u ) {
					if ( 24u <= (*( current_position)) && (*( current_position)) <= 27u )
						goto st1;
				} else if ( (*( current_position)) >= 16u )
					goto st1;
			} else
				goto st1;
		} else if ( (*( current_position)) > 35u ) {
			if ( (*( current_position)) < 56u ) {
				if ( (*( current_position)) > 43u ) {
					if ( 48u <= (*( current_position)) && (*( current_position)) <= 51u )
						goto st1;
				} else if ( (*( current_position)) >= 40u )
					goto st1;
			} else if ( (*( current_position)) > 59u ) {
				if ( (*( current_position)) > 95u ) {
					if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
						goto st56;
				} else if ( (*( current_position)) >= 64u )
					goto tr0;
			} else
				goto st1;
		} else
			goto st1;
	} else if ( (*( current_position)) > 139u ) {
		if ( (*( current_position)) < 176u ) {
			if ( (*( current_position)) < 160u ) {
				if ( (*( current_position)) > 153u ) {
					if ( 158u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto tr0;
				} else if ( (*( current_position)) >= 144u )
					goto tr0;
			} else if ( (*( current_position)) > 163u ) {
				if ( (*( current_position)) > 171u ) {
					if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
						goto tr0;
				} else if ( (*( current_position)) >= 164u )
					goto tr0;
			} else
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( (*( current_position)) > 191u ) {
					if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
						goto st96;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st98;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
st241:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof241;
case 241:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 12u: goto st2;
		case 13u: goto st3;
		case 52u: goto st2;
		case 53u: goto st3;
		case 68u: goto st8;
		case 76u: goto st8;
		case 116u: goto st8;
		case 132u: goto st9;
		case 140u: goto st9;
		case 180u: goto st9;
		case 215u: goto tr415;
		case 231u: goto tr416;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( (*( current_position)) <= 15u )
				goto tr0;
		} else if ( (*( current_position)) > 55u ) {
			if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto st7;
		} else
			goto tr0;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto st3;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr0;
			} else if ( (*( current_position)) >= 192u )
				goto tr0;
		} else
			goto st3;
	} else
		goto st7;
	goto tr16;
	}
	_test_eof242: ( current_state) = 242; goto _test_eof; 
	_test_eof1: ( current_state) = 1; goto _test_eof; 
	_test_eof2: ( current_state) = 2; goto _test_eof; 
	_test_eof3: ( current_state) = 3; goto _test_eof; 
	_test_eof4: ( current_state) = 4; goto _test_eof; 
	_test_eof5: ( current_state) = 5; goto _test_eof; 
	_test_eof6: ( current_state) = 6; goto _test_eof; 
	_test_eof7: ( current_state) = 7; goto _test_eof; 
	_test_eof8: ( current_state) = 8; goto _test_eof; 
	_test_eof9: ( current_state) = 9; goto _test_eof; 
	_test_eof10: ( current_state) = 10; goto _test_eof; 
	_test_eof11: ( current_state) = 11; goto _test_eof; 
	_test_eof12: ( current_state) = 12; goto _test_eof; 
	_test_eof13: ( current_state) = 13; goto _test_eof; 
	_test_eof14: ( current_state) = 14; goto _test_eof; 
	_test_eof15: ( current_state) = 15; goto _test_eof; 
	_test_eof16: ( current_state) = 16; goto _test_eof; 
	_test_eof17: ( current_state) = 17; goto _test_eof; 
	_test_eof18: ( current_state) = 18; goto _test_eof; 
	_test_eof19: ( current_state) = 19; goto _test_eof; 
	_test_eof20: ( current_state) = 20; goto _test_eof; 
	_test_eof21: ( current_state) = 21; goto _test_eof; 
	_test_eof22: ( current_state) = 22; goto _test_eof; 
	_test_eof23: ( current_state) = 23; goto _test_eof; 
	_test_eof24: ( current_state) = 24; goto _test_eof; 
	_test_eof25: ( current_state) = 25; goto _test_eof; 
	_test_eof26: ( current_state) = 26; goto _test_eof; 
	_test_eof27: ( current_state) = 27; goto _test_eof; 
	_test_eof28: ( current_state) = 28; goto _test_eof; 
	_test_eof29: ( current_state) = 29; goto _test_eof; 
	_test_eof30: ( current_state) = 30; goto _test_eof; 
	_test_eof31: ( current_state) = 31; goto _test_eof; 
	_test_eof32: ( current_state) = 32; goto _test_eof; 
	_test_eof33: ( current_state) = 33; goto _test_eof; 
	_test_eof34: ( current_state) = 34; goto _test_eof; 
	_test_eof35: ( current_state) = 35; goto _test_eof; 
	_test_eof36: ( current_state) = 36; goto _test_eof; 
	_test_eof37: ( current_state) = 37; goto _test_eof; 
	_test_eof38: ( current_state) = 38; goto _test_eof; 
	_test_eof39: ( current_state) = 39; goto _test_eof; 
	_test_eof40: ( current_state) = 40; goto _test_eof; 
	_test_eof41: ( current_state) = 41; goto _test_eof; 
	_test_eof42: ( current_state) = 42; goto _test_eof; 
	_test_eof43: ( current_state) = 43; goto _test_eof; 
	_test_eof44: ( current_state) = 44; goto _test_eof; 
	_test_eof45: ( current_state) = 45; goto _test_eof; 
	_test_eof46: ( current_state) = 46; goto _test_eof; 
	_test_eof47: ( current_state) = 47; goto _test_eof; 
	_test_eof48: ( current_state) = 48; goto _test_eof; 
	_test_eof49: ( current_state) = 49; goto _test_eof; 
	_test_eof50: ( current_state) = 50; goto _test_eof; 
	_test_eof51: ( current_state) = 51; goto _test_eof; 
	_test_eof52: ( current_state) = 52; goto _test_eof; 
	_test_eof53: ( current_state) = 53; goto _test_eof; 
	_test_eof54: ( current_state) = 54; goto _test_eof; 
	_test_eof55: ( current_state) = 55; goto _test_eof; 
	_test_eof56: ( current_state) = 56; goto _test_eof; 
	_test_eof57: ( current_state) = 57; goto _test_eof; 
	_test_eof58: ( current_state) = 58; goto _test_eof; 
	_test_eof59: ( current_state) = 59; goto _test_eof; 
	_test_eof60: ( current_state) = 60; goto _test_eof; 
	_test_eof61: ( current_state) = 61; goto _test_eof; 
	_test_eof62: ( current_state) = 62; goto _test_eof; 
	_test_eof63: ( current_state) = 63; goto _test_eof; 
	_test_eof64: ( current_state) = 64; goto _test_eof; 
	_test_eof65: ( current_state) = 65; goto _test_eof; 
	_test_eof66: ( current_state) = 66; goto _test_eof; 
	_test_eof67: ( current_state) = 67; goto _test_eof; 
	_test_eof68: ( current_state) = 68; goto _test_eof; 
	_test_eof69: ( current_state) = 69; goto _test_eof; 
	_test_eof70: ( current_state) = 70; goto _test_eof; 
	_test_eof71: ( current_state) = 71; goto _test_eof; 
	_test_eof72: ( current_state) = 72; goto _test_eof; 
	_test_eof73: ( current_state) = 73; goto _test_eof; 
	_test_eof74: ( current_state) = 74; goto _test_eof; 
	_test_eof75: ( current_state) = 75; goto _test_eof; 
	_test_eof76: ( current_state) = 76; goto _test_eof; 
	_test_eof77: ( current_state) = 77; goto _test_eof; 
	_test_eof78: ( current_state) = 78; goto _test_eof; 
	_test_eof79: ( current_state) = 79; goto _test_eof; 
	_test_eof80: ( current_state) = 80; goto _test_eof; 
	_test_eof81: ( current_state) = 81; goto _test_eof; 
	_test_eof82: ( current_state) = 82; goto _test_eof; 
	_test_eof83: ( current_state) = 83; goto _test_eof; 
	_test_eof84: ( current_state) = 84; goto _test_eof; 
	_test_eof85: ( current_state) = 85; goto _test_eof; 
	_test_eof86: ( current_state) = 86; goto _test_eof; 
	_test_eof87: ( current_state) = 87; goto _test_eof; 
	_test_eof88: ( current_state) = 88; goto _test_eof; 
	_test_eof89: ( current_state) = 89; goto _test_eof; 
	_test_eof90: ( current_state) = 90; goto _test_eof; 
	_test_eof91: ( current_state) = 91; goto _test_eof; 
	_test_eof92: ( current_state) = 92; goto _test_eof; 
	_test_eof93: ( current_state) = 93; goto _test_eof; 
	_test_eof94: ( current_state) = 94; goto _test_eof; 
	_test_eof95: ( current_state) = 95; goto _test_eof; 
	_test_eof96: ( current_state) = 96; goto _test_eof; 
	_test_eof97: ( current_state) = 97; goto _test_eof; 
	_test_eof98: ( current_state) = 98; goto _test_eof; 
	_test_eof99: ( current_state) = 99; goto _test_eof; 
	_test_eof100: ( current_state) = 100; goto _test_eof; 
	_test_eof101: ( current_state) = 101; goto _test_eof; 
	_test_eof102: ( current_state) = 102; goto _test_eof; 
	_test_eof103: ( current_state) = 103; goto _test_eof; 
	_test_eof104: ( current_state) = 104; goto _test_eof; 
	_test_eof105: ( current_state) = 105; goto _test_eof; 
	_test_eof106: ( current_state) = 106; goto _test_eof; 
	_test_eof107: ( current_state) = 107; goto _test_eof; 
	_test_eof108: ( current_state) = 108; goto _test_eof; 
	_test_eof109: ( current_state) = 109; goto _test_eof; 
	_test_eof110: ( current_state) = 110; goto _test_eof; 
	_test_eof111: ( current_state) = 111; goto _test_eof; 
	_test_eof112: ( current_state) = 112; goto _test_eof; 
	_test_eof113: ( current_state) = 113; goto _test_eof; 
	_test_eof114: ( current_state) = 114; goto _test_eof; 
	_test_eof115: ( current_state) = 115; goto _test_eof; 
	_test_eof116: ( current_state) = 116; goto _test_eof; 
	_test_eof117: ( current_state) = 117; goto _test_eof; 
	_test_eof118: ( current_state) = 118; goto _test_eof; 
	_test_eof119: ( current_state) = 119; goto _test_eof; 
	_test_eof120: ( current_state) = 120; goto _test_eof; 
	_test_eof243: ( current_state) = 243; goto _test_eof; 
	_test_eof121: ( current_state) = 121; goto _test_eof; 
	_test_eof122: ( current_state) = 122; goto _test_eof; 
	_test_eof123: ( current_state) = 123; goto _test_eof; 
	_test_eof124: ( current_state) = 124; goto _test_eof; 
	_test_eof125: ( current_state) = 125; goto _test_eof; 
	_test_eof126: ( current_state) = 126; goto _test_eof; 
	_test_eof127: ( current_state) = 127; goto _test_eof; 
	_test_eof128: ( current_state) = 128; goto _test_eof; 
	_test_eof129: ( current_state) = 129; goto _test_eof; 
	_test_eof130: ( current_state) = 130; goto _test_eof; 
	_test_eof131: ( current_state) = 131; goto _test_eof; 
	_test_eof132: ( current_state) = 132; goto _test_eof; 
	_test_eof133: ( current_state) = 133; goto _test_eof; 
	_test_eof134: ( current_state) = 134; goto _test_eof; 
	_test_eof135: ( current_state) = 135; goto _test_eof; 
	_test_eof136: ( current_state) = 136; goto _test_eof; 
	_test_eof137: ( current_state) = 137; goto _test_eof; 
	_test_eof138: ( current_state) = 138; goto _test_eof; 
	_test_eof139: ( current_state) = 139; goto _test_eof; 
	_test_eof140: ( current_state) = 140; goto _test_eof; 
	_test_eof141: ( current_state) = 141; goto _test_eof; 
	_test_eof142: ( current_state) = 142; goto _test_eof; 
	_test_eof143: ( current_state) = 143; goto _test_eof; 
	_test_eof144: ( current_state) = 144; goto _test_eof; 
	_test_eof145: ( current_state) = 145; goto _test_eof; 
	_test_eof146: ( current_state) = 146; goto _test_eof; 
	_test_eof147: ( current_state) = 147; goto _test_eof; 
	_test_eof148: ( current_state) = 148; goto _test_eof; 
	_test_eof149: ( current_state) = 149; goto _test_eof; 
	_test_eof150: ( current_state) = 150; goto _test_eof; 
	_test_eof151: ( current_state) = 151; goto _test_eof; 
	_test_eof152: ( current_state) = 152; goto _test_eof; 
	_test_eof153: ( current_state) = 153; goto _test_eof; 
	_test_eof154: ( current_state) = 154; goto _test_eof; 
	_test_eof155: ( current_state) = 155; goto _test_eof; 
	_test_eof156: ( current_state) = 156; goto _test_eof; 
	_test_eof157: ( current_state) = 157; goto _test_eof; 
	_test_eof158: ( current_state) = 158; goto _test_eof; 
	_test_eof159: ( current_state) = 159; goto _test_eof; 
	_test_eof160: ( current_state) = 160; goto _test_eof; 
	_test_eof161: ( current_state) = 161; goto _test_eof; 
	_test_eof162: ( current_state) = 162; goto _test_eof; 
	_test_eof163: ( current_state) = 163; goto _test_eof; 
	_test_eof164: ( current_state) = 164; goto _test_eof; 
	_test_eof165: ( current_state) = 165; goto _test_eof; 
	_test_eof166: ( current_state) = 166; goto _test_eof; 
	_test_eof167: ( current_state) = 167; goto _test_eof; 
	_test_eof168: ( current_state) = 168; goto _test_eof; 
	_test_eof169: ( current_state) = 169; goto _test_eof; 
	_test_eof170: ( current_state) = 170; goto _test_eof; 
	_test_eof171: ( current_state) = 171; goto _test_eof; 
	_test_eof172: ( current_state) = 172; goto _test_eof; 
	_test_eof173: ( current_state) = 173; goto _test_eof; 
	_test_eof174: ( current_state) = 174; goto _test_eof; 
	_test_eof175: ( current_state) = 175; goto _test_eof; 
	_test_eof176: ( current_state) = 176; goto _test_eof; 
	_test_eof177: ( current_state) = 177; goto _test_eof; 
	_test_eof178: ( current_state) = 178; goto _test_eof; 
	_test_eof179: ( current_state) = 179; goto _test_eof; 
	_test_eof180: ( current_state) = 180; goto _test_eof; 
	_test_eof181: ( current_state) = 181; goto _test_eof; 
	_test_eof182: ( current_state) = 182; goto _test_eof; 
	_test_eof183: ( current_state) = 183; goto _test_eof; 
	_test_eof184: ( current_state) = 184; goto _test_eof; 
	_test_eof185: ( current_state) = 185; goto _test_eof; 
	_test_eof186: ( current_state) = 186; goto _test_eof; 
	_test_eof187: ( current_state) = 187; goto _test_eof; 
	_test_eof188: ( current_state) = 188; goto _test_eof; 
	_test_eof189: ( current_state) = 189; goto _test_eof; 
	_test_eof190: ( current_state) = 190; goto _test_eof; 
	_test_eof191: ( current_state) = 191; goto _test_eof; 
	_test_eof192: ( current_state) = 192; goto _test_eof; 
	_test_eof193: ( current_state) = 193; goto _test_eof; 
	_test_eof194: ( current_state) = 194; goto _test_eof; 
	_test_eof195: ( current_state) = 195; goto _test_eof; 
	_test_eof196: ( current_state) = 196; goto _test_eof; 
	_test_eof197: ( current_state) = 197; goto _test_eof; 
	_test_eof198: ( current_state) = 198; goto _test_eof; 
	_test_eof199: ( current_state) = 199; goto _test_eof; 
	_test_eof200: ( current_state) = 200; goto _test_eof; 
	_test_eof201: ( current_state) = 201; goto _test_eof; 
	_test_eof202: ( current_state) = 202; goto _test_eof; 
	_test_eof203: ( current_state) = 203; goto _test_eof; 
	_test_eof204: ( current_state) = 204; goto _test_eof; 
	_test_eof205: ( current_state) = 205; goto _test_eof; 
	_test_eof206: ( current_state) = 206; goto _test_eof; 
	_test_eof207: ( current_state) = 207; goto _test_eof; 
	_test_eof208: ( current_state) = 208; goto _test_eof; 
	_test_eof209: ( current_state) = 209; goto _test_eof; 
	_test_eof210: ( current_state) = 210; goto _test_eof; 
	_test_eof211: ( current_state) = 211; goto _test_eof; 
	_test_eof212: ( current_state) = 212; goto _test_eof; 
	_test_eof213: ( current_state) = 213; goto _test_eof; 
	_test_eof214: ( current_state) = 214; goto _test_eof; 
	_test_eof215: ( current_state) = 215; goto _test_eof; 
	_test_eof216: ( current_state) = 216; goto _test_eof; 
	_test_eof217: ( current_state) = 217; goto _test_eof; 
	_test_eof218: ( current_state) = 218; goto _test_eof; 
	_test_eof219: ( current_state) = 219; goto _test_eof; 
	_test_eof220: ( current_state) = 220; goto _test_eof; 
	_test_eof221: ( current_state) = 221; goto _test_eof; 
	_test_eof222: ( current_state) = 222; goto _test_eof; 
	_test_eof223: ( current_state) = 223; goto _test_eof; 
	_test_eof224: ( current_state) = 224; goto _test_eof; 
	_test_eof225: ( current_state) = 225; goto _test_eof; 
	_test_eof226: ( current_state) = 226; goto _test_eof; 
	_test_eof227: ( current_state) = 227; goto _test_eof; 
	_test_eof228: ( current_state) = 228; goto _test_eof; 
	_test_eof244: ( current_state) = 244; goto _test_eof; 
	_test_eof229: ( current_state) = 229; goto _test_eof; 
	_test_eof230: ( current_state) = 230; goto _test_eof; 
	_test_eof245: ( current_state) = 245; goto _test_eof; 
	_test_eof231: ( current_state) = 231; goto _test_eof; 
	_test_eof232: ( current_state) = 232; goto _test_eof; 
	_test_eof246: ( current_state) = 246; goto _test_eof; 
	_test_eof233: ( current_state) = 233; goto _test_eof; 
	_test_eof234: ( current_state) = 234; goto _test_eof; 
	_test_eof247: ( current_state) = 247; goto _test_eof; 
	_test_eof235: ( current_state) = 235; goto _test_eof; 
	_test_eof236: ( current_state) = 236; goto _test_eof; 
	_test_eof248: ( current_state) = 248; goto _test_eof; 
	_test_eof237: ( current_state) = 237; goto _test_eof; 
	_test_eof238: ( current_state) = 238; goto _test_eof; 
	_test_eof249: ( current_state) = 249; goto _test_eof; 
	_test_eof239: ( current_state) = 239; goto _test_eof; 
	_test_eof240: ( current_state) = 240; goto _test_eof; 
	_test_eof250: ( current_state) = 250; goto _test_eof; 
	_test_eof241: ( current_state) = 241; goto _test_eof; 

	_test_eof: {}
	if ( ( current_position) == ( end_of_bundle) )
	{
	switch ( ( current_state) ) {
	case 1: 
	case 2: 
	case 3: 
	case 4: 
	case 5: 
	case 6: 
	case 7: 
	case 8: 
	case 9: 
	case 10: 
	case 11: 
	case 12: 
	case 13: 
	case 14: 
	case 15: 
	case 16: 
	case 17: 
	case 18: 
	case 19: 
	case 20: 
	case 21: 
	case 22: 
	case 23: 
	case 24: 
	case 25: 
	case 26: 
	case 27: 
	case 28: 
	case 29: 
	case 30: 
	case 31: 
	case 32: 
	case 33: 
	case 34: 
	case 35: 
	case 36: 
	case 37: 
	case 38: 
	case 39: 
	case 40: 
	case 41: 
	case 42: 
	case 43: 
	case 44: 
	case 45: 
	case 46: 
	case 47: 
	case 48: 
	case 49: 
	case 50: 
	case 51: 
	case 52: 
	case 53: 
	case 54: 
	case 55: 
	case 56: 
	case 57: 
	case 58: 
	case 59: 
	case 60: 
	case 61: 
	case 62: 
	case 63: 
	case 64: 
	case 65: 
	case 66: 
	case 67: 
	case 68: 
	case 69: 
	case 70: 
	case 71: 
	case 72: 
	case 73: 
	case 74: 
	case 75: 
	case 76: 
	case 77: 
	case 78: 
	case 79: 
	case 80: 
	case 81: 
	case 82: 
	case 83: 
	case 84: 
	case 85: 
	case 86: 
	case 87: 
	case 88: 
	case 89: 
	case 90: 
	case 91: 
	case 92: 
	case 93: 
	case 94: 
	case 95: 
	case 96: 
	case 97: 
	case 98: 
	case 99: 
	case 100: 
	case 101: 
	case 102: 
	case 103: 
	case 104: 
	case 105: 
	case 106: 
	case 107: 
	case 108: 
	case 109: 
	case 110: 
	case 111: 
	case 112: 
	case 113: 
	case 114: 
	case 115: 
	case 116: 
	case 117: 
	case 118: 
	case 119: 
	case 120: 
	case 121: 
	case 122: 
	case 123: 
	case 124: 
	case 125: 
	case 126: 
	case 127: 
	case 128: 
	case 129: 
	case 130: 
	case 131: 
	case 132: 
	case 133: 
	case 134: 
	case 135: 
	case 136: 
	case 137: 
	case 138: 
	case 139: 
	case 140: 
	case 141: 
	case 142: 
	case 143: 
	case 144: 
	case 145: 
	case 146: 
	case 147: 
	case 148: 
	case 149: 
	case 150: 
	case 151: 
	case 152: 
	case 153: 
	case 154: 
	case 155: 
	case 156: 
	case 157: 
	case 158: 
	case 159: 
	case 160: 
	case 161: 
	case 162: 
	case 163: 
	case 164: 
	case 165: 
	case 166: 
	case 167: 
	case 168: 
	case 169: 
	case 170: 
	case 171: 
	case 172: 
	case 173: 
	case 174: 
	case 175: 
	case 176: 
	case 177: 
	case 178: 
	case 179: 
	case 180: 
	case 181: 
	case 182: 
	case 183: 
	case 184: 
	case 185: 
	case 186: 
	case 187: 
	case 188: 
	case 189: 
	case 190: 
	case 191: 
	case 192: 
	case 193: 
	case 194: 
	case 195: 
	case 196: 
	case 197: 
	case 198: 
	case 199: 
	case 200: 
	case 201: 
	case 202: 
	case 203: 
	case 204: 
	case 205: 
	case 206: 
	case 207: 
	case 208: 
	case 209: 
	case 210: 
	case 211: 
	case 212: 
	case 213: 
	case 214: 
	case 215: 
	case 216: 
	case 217: 
	case 218: 
	case 219: 
	case 220: 
	case 221: 
	case 222: 
	case 223: 
	case 224: 
	case 225: 
	case 226: 
	case 227: 
	case 228: 
	case 229: 
	case 230: 
	case 231: 
	case 232: 
	case 233: 
	case 234: 
	case 235: 
	case 236: 
	case 237: 
	case 238: 
	case 239: 
	case 240: 
	case 241: 
	{
    result &= user_callback(instruction_start, current_position,
                            UNRECOGNIZED_INSTRUCTION, callback_data);
    /*
     * Process the next bundle: “continue” here is for the “for” cycle in
     * the ValidateChunkIA32 function.
     *
     * It does not affect the case which we really care about (when code
     * is validatable), but makes it possible to detect more errors in one
     * run in tools like ncval.
     */
    continue;
  }
	break;
	}
	}

	_out: {}
	}

  }

  /*
   * Check the direct jumps.  All the targets from jump_dests must be in
   * valid_targets.
   */
  result &= ProcessInvalidJumpTargets(data, size, valid_targets, jump_dests,
                                      user_callback, callback_data);

  /* We only use malloc for a large code sequences  */
  if (jump_dests != jump_dests_small) free(jump_dests);
  if (valid_targets != valid_targets_small) free(valid_targets);
  if (!result) errno = EINVAL;
  return result;
}
