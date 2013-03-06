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





static const int x86_32_validator_start = 234;
static const int x86_32_validator_first_final = 234;
static const int x86_32_validator_error = 0;

static const int x86_32_validator_en_main = 234;




Bool ValidateChunkIA32(const uint8_t *data, size_t size,
                       uint32_t options,
                       const NaClCPUFeaturesX86 *cpu_features,
                       ValidationCallbackFunc user_callback,
                       void *callback_data) {
  bitmap_word valid_targets_small;
  bitmap_word jump_dests_small;
  bitmap_word *valid_targets;
  bitmap_word *jump_dests;
  const uint8_t *current_position;
  const uint8_t *end_of_bundle;
  int result = TRUE;

  CHECK(sizeof valid_targets_small == sizeof jump_dests_small);
  CHECK(size % kBundleSize == 0);

  /* For a very small sequences (one bundle) malloc is too expensive.  */
  if (size <= (sizeof valid_targets_small * 8)) {
    valid_targets_small = 0;
    valid_targets = &valid_targets_small;
    jump_dests_small = 0;
    jump_dests = &jump_dests_small;
  } else {
    valid_targets = BitmapAllocate(size);
    jump_dests = BitmapAllocate(size);
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
    const uint8_t *instruction_begin = current_position;
    /* Only used locally in the end_of_instruction_cleanup action.  */
    const uint8_t *instruction_end;
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
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr9:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr10:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr11:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr15:
	{
    SET_IMM_TYPE(IMM32);
    SET_IMM_PTR(current_position - 3);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr19:
	{ SET_CPU_FEATURE(CPUFeature_3DNOW);     }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr26:
	{ SET_CPU_FEATURE(CPUFeature_TSC);       }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr35:
	{ SET_CPU_FEATURE(CPUFeature_MMX);       }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr48:
	{ SET_CPU_FEATURE(CPUFeature_MON);       }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr49:
	{ SET_CPU_FEATURE(CPUFeature_FXSR);      }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr50:
	{ SET_CPU_FEATURE(CPUFeature_3DPRFTCH);  }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr62:
	{
    instruction_info_collected |= LAST_BYTE_IS_NOT_IMMEDIATE;
  }
	{ SET_CPU_FEATURE(CPUFeature_E3DNOW);    }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr63:
	{
    instruction_info_collected |= LAST_BYTE_IS_NOT_IMMEDIATE;
  }
	{ SET_CPU_FEATURE(CPUFeature_3DNOW);     }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr69:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr75:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr83:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr94:
	{ SET_CPU_FEATURE(CPUFeature_MOVBE);     }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr115:
	{ SET_CPU_FEATURE(CPUFeature_CMOV);      }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr131:
	{
    Rel32Operand(current_position + 1, data, jump_dests, size,
                 &instruction_info_collected);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr137:
	{ SET_CPU_FEATURE(CPUFeature_CLFLUSH);   }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr146:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr153:
	{ SET_CPU_FEATURE(CPUFeature_CX8);       }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr164:
	{ SET_CPU_FEATURE(CPUFeature_EMMX);      }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr167:
	{
    Rel8Operand(current_position + 1, data, jump_dests, size,
                &instruction_info_collected);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr188:
	{
    SET_IMM_TYPE(IMM16);
    SET_IMM_PTR(current_position - 1);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr205:
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr211:
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr217:
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr257:
	{ SET_CPU_FEATURE(CPUFeature_SSE4A);     }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr258:
	{ SET_CPU_FEATURE(CPUFeature_SSE3);      }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr312:
	{ SET_CPU_FEATURE(CPUFeature_TBM);       }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr319:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr346:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr354:
	{ SET_CPU_FEATURE(CPUFeature_FMA);       }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr360:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr367:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr393:
	{
    instruction_info_collected |= LAST_BYTE_IS_NOT_IMMEDIATE;
  }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr415:
	{ SET_CPU_FEATURE(CPUFeature_x87);       }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr421:
	{ SET_CPU_FEATURE(CPUFeature_CMOVx87);   }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr425:
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
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr440:
	{
    SET_IMM2_TYPE(IMM8);
    SET_IMM2_PTR(current_position);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr445:
	{ SET_CPU_FEATURE(CPUFeature_POPCNT);    }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr451:
	{ SET_CPU_FEATURE(CPUFeature_TZCNT);     }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr457:
	{ SET_CPU_FEATURE(CPUFeature_LZCNT);     }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr463:
	{
      UnmarkValidJumpTarget((current_position - data) - 1, valid_targets);
      instruction_begin -= 3;
      instruction_info_collected |= SPECIAL_INSTRUCTION;
    }
	{
      if (((current_position - data) & kBundleMask) != kBundleMask)
        instruction_info_collected |= BAD_CALL_ALIGNMENT;
    }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
tr464:
	{
      UnmarkValidJumpTarget((current_position - data) - 1, valid_targets);
      instruction_begin -= 3;
      instruction_info_collected |= SPECIAL_INSTRUCTION;
    }
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st234;
st234:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof234;
case 234:
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
		case 46u: goto st66;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st66;
		case 101u: goto st69;
		case 102u: goto st75;
		case 104u: goto st11;
		case 105u: goto st131;
		case 106u: goto st10;
		case 107u: goto st56;
		case 128u: goto st56;
		case 129u: goto st131;
		case 131u: goto st140;
		case 141u: goto st116;
		case 143u: goto st142;
		case 155u: goto tr415;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st154;
		case 198u: goto st186;
		case 199u: goto st187;
		case 201u: goto tr0;
		case 216u: goto st188;
		case 217u: goto st189;
		case 218u: goto st190;
		case 219u: goto st191;
		case 220u: goto st192;
		case 221u: goto st193;
		case 222u: goto st194;
		case 223u: goto st195;
		case 232u: goto st196;
		case 233u: goto st52;
		case 235u: goto st68;
		case 240u: goto st200;
		case 242u: goto st203;
		case 243u: goto st211;
		case 246u: goto st216;
		case 247u: goto st217;
		case 254u: goto st218;
		case 255u: goto st130;
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
						goto st68;
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
						goto st117;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st119;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
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
tr51:
	{ SET_CPU_FEATURE(CPUFeature_3DPRFTCH);  }
	goto st2;
tr70:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st2;
tr76:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st2;
tr84:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st2;
tr89:
	{ SET_CPU_FEATURE(CPUFeature_MMX);       }
	goto st2;
tr95:
	{ SET_CPU_FEATURE(CPUFeature_MOVBE);     }
	goto st2;
tr116:
	{ SET_CPU_FEATURE(CPUFeature_CMOV);      }
	goto st2;
tr159:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st2;
tr135:
	{ SET_CPU_FEATURE(CPUFeature_FXSR);      }
	goto st2;
tr138:
	{ SET_CPU_FEATURE(CPUFeature_CLFLUSH);   }
	goto st2;
tr154:
	{ SET_CPU_FEATURE(CPUFeature_CX8);       }
	goto st2;
tr206:
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st2;
tr212:
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st2;
tr218:
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st2;
tr259:
	{ SET_CPU_FEATURE(CPUFeature_SSE3);      }
	goto st2;
tr313:
	{ SET_CPU_FEATURE(CPUFeature_TBM);       }
	goto st2;
tr347:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st2;
tr355:
	{ SET_CPU_FEATURE(CPUFeature_FMA);       }
	goto st2;
tr361:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st2;
tr368:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st2;
tr416:
	{ SET_CPU_FEATURE(CPUFeature_x87);       }
	goto st2;
tr433:
	{ SET_CPU_FEATURE(CPUFeature_SSE4A);     }
	goto st2;
tr446:
	{ SET_CPU_FEATURE(CPUFeature_POPCNT);    }
	goto st2;
tr452:
	{ SET_CPU_FEATURE(CPUFeature_TZCNT);     }
	goto st2;
tr458:
	{ SET_CPU_FEATURE(CPUFeature_LZCNT);     }
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
tr52:
	{ SET_CPU_FEATURE(CPUFeature_3DPRFTCH);  }
	goto st3;
tr71:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st3;
tr77:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st3;
tr85:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st3;
tr90:
	{ SET_CPU_FEATURE(CPUFeature_MMX);       }
	goto st3;
tr96:
	{ SET_CPU_FEATURE(CPUFeature_MOVBE);     }
	goto st3;
tr117:
	{ SET_CPU_FEATURE(CPUFeature_CMOV);      }
	goto st3;
tr160:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st3;
tr136:
	{ SET_CPU_FEATURE(CPUFeature_FXSR);      }
	goto st3;
tr139:
	{ SET_CPU_FEATURE(CPUFeature_CLFLUSH);   }
	goto st3;
tr155:
	{ SET_CPU_FEATURE(CPUFeature_CX8);       }
	goto st3;
tr207:
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st3;
tr213:
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st3;
tr219:
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st3;
tr260:
	{ SET_CPU_FEATURE(CPUFeature_SSE3);      }
	goto st3;
tr314:
	{ SET_CPU_FEATURE(CPUFeature_TBM);       }
	goto st3;
tr348:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st3;
tr356:
	{ SET_CPU_FEATURE(CPUFeature_FMA);       }
	goto st3;
tr362:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st3;
tr369:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st3;
tr417:
	{ SET_CPU_FEATURE(CPUFeature_x87);       }
	goto st3;
tr434:
	{ SET_CPU_FEATURE(CPUFeature_SSE4A);     }
	goto st3;
tr447:
	{ SET_CPU_FEATURE(CPUFeature_POPCNT);    }
	goto st3;
tr453:
	{ SET_CPU_FEATURE(CPUFeature_TZCNT);     }
	goto st3;
tr459:
	{ SET_CPU_FEATURE(CPUFeature_LZCNT);     }
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
tr53:
	{ SET_CPU_FEATURE(CPUFeature_3DPRFTCH);  }
	goto st7;
tr72:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st7;
tr78:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st7;
tr86:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st7;
tr91:
	{ SET_CPU_FEATURE(CPUFeature_MMX);       }
	goto st7;
tr97:
	{ SET_CPU_FEATURE(CPUFeature_MOVBE);     }
	goto st7;
tr118:
	{ SET_CPU_FEATURE(CPUFeature_CMOV);      }
	goto st7;
tr161:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st7;
tr140:
	{ SET_CPU_FEATURE(CPUFeature_FXSR);      }
	goto st7;
tr142:
	{ SET_CPU_FEATURE(CPUFeature_CLFLUSH);   }
	goto st7;
tr156:
	{ SET_CPU_FEATURE(CPUFeature_CX8);       }
	goto st7;
tr208:
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st7;
tr214:
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st7;
tr220:
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st7;
tr261:
	{ SET_CPU_FEATURE(CPUFeature_SSE3);      }
	goto st7;
tr315:
	{ SET_CPU_FEATURE(CPUFeature_TBM);       }
	goto st7;
tr349:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st7;
tr357:
	{ SET_CPU_FEATURE(CPUFeature_FMA);       }
	goto st7;
tr363:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st7;
tr370:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st7;
tr418:
	{ SET_CPU_FEATURE(CPUFeature_x87);       }
	goto st7;
tr435:
	{ SET_CPU_FEATURE(CPUFeature_SSE4A);     }
	goto st7;
tr448:
	{ SET_CPU_FEATURE(CPUFeature_POPCNT);    }
	goto st7;
tr454:
	{ SET_CPU_FEATURE(CPUFeature_TZCNT);     }
	goto st7;
tr460:
	{ SET_CPU_FEATURE(CPUFeature_LZCNT);     }
	goto st7;
st7:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof7;
case 7:
	goto tr10;
tr54:
	{ SET_CPU_FEATURE(CPUFeature_3DPRFTCH);  }
	goto st8;
tr73:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st8;
tr79:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st8;
tr87:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st8;
tr92:
	{ SET_CPU_FEATURE(CPUFeature_MMX);       }
	goto st8;
tr98:
	{ SET_CPU_FEATURE(CPUFeature_MOVBE);     }
	goto st8;
tr119:
	{ SET_CPU_FEATURE(CPUFeature_CMOV);      }
	goto st8;
tr162:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st8;
tr141:
	{ SET_CPU_FEATURE(CPUFeature_FXSR);      }
	goto st8;
tr143:
	{ SET_CPU_FEATURE(CPUFeature_CLFLUSH);   }
	goto st8;
tr157:
	{ SET_CPU_FEATURE(CPUFeature_CX8);       }
	goto st8;
tr209:
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st8;
tr215:
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st8;
tr221:
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st8;
tr262:
	{ SET_CPU_FEATURE(CPUFeature_SSE3);      }
	goto st8;
tr316:
	{ SET_CPU_FEATURE(CPUFeature_TBM);       }
	goto st8;
tr350:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st8;
tr358:
	{ SET_CPU_FEATURE(CPUFeature_FMA);       }
	goto st8;
tr364:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st8;
tr371:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st8;
tr419:
	{ SET_CPU_FEATURE(CPUFeature_x87);       }
	goto st8;
tr436:
	{ SET_CPU_FEATURE(CPUFeature_SSE4A);     }
	goto st8;
tr449:
	{ SET_CPU_FEATURE(CPUFeature_POPCNT);    }
	goto st8;
tr455:
	{ SET_CPU_FEATURE(CPUFeature_TZCNT);     }
	goto st8;
tr461:
	{ SET_CPU_FEATURE(CPUFeature_LZCNT);     }
	goto st8;
st8:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof8;
case 8:
	goto st7;
tr55:
	{ SET_CPU_FEATURE(CPUFeature_3DPRFTCH);  }
	goto st9;
tr74:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st9;
tr80:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st9;
tr88:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st9;
tr93:
	{ SET_CPU_FEATURE(CPUFeature_MMX);       }
	goto st9;
tr99:
	{ SET_CPU_FEATURE(CPUFeature_MOVBE);     }
	goto st9;
tr120:
	{ SET_CPU_FEATURE(CPUFeature_CMOV);      }
	goto st9;
tr163:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st9;
tr144:
	{ SET_CPU_FEATURE(CPUFeature_FXSR);      }
	goto st9;
tr145:
	{ SET_CPU_FEATURE(CPUFeature_CLFLUSH);   }
	goto st9;
tr158:
	{ SET_CPU_FEATURE(CPUFeature_CX8);       }
	goto st9;
tr210:
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st9;
tr216:
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st9;
tr222:
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st9;
tr263:
	{ SET_CPU_FEATURE(CPUFeature_SSE3);      }
	goto st9;
tr317:
	{ SET_CPU_FEATURE(CPUFeature_TBM);       }
	goto st9;
tr351:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st9;
tr359:
	{ SET_CPU_FEATURE(CPUFeature_FMA);       }
	goto st9;
tr365:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st9;
tr372:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st9;
tr420:
	{ SET_CPU_FEATURE(CPUFeature_x87);       }
	goto st9;
tr437:
	{ SET_CPU_FEATURE(CPUFeature_SSE4A);     }
	goto st9;
tr450:
	{ SET_CPU_FEATURE(CPUFeature_POPCNT);    }
	goto st9;
tr456:
	{ SET_CPU_FEATURE(CPUFeature_TZCNT);     }
	goto st9;
tr462:
	{ SET_CPU_FEATURE(CPUFeature_LZCNT);     }
	goto st9;
st9:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof9;
case 9:
	goto st3;
tr112:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	goto st10;
tr113:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{}
	goto st10;
tr147:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st10;
tr251:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st10;
tr101:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st10;
tr127:
	{ SET_CPU_FEATURE(CPUFeature_MMX);       }
	goto st10;
tr121:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st10;
tr227:
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st10;
tr239:
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st10;
tr245:
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st10;
tr233:
	{ SET_CPU_FEATURE(CPUFeature_CLMUL);     }
	goto st10;
tr402:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st10;
tr409:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st10;
tr381:
	{ SET_CPU_FEATURE(CPUFeature_CLMULAVX);  }
	goto st10;
st10:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof10;
case 10:
	goto tr11;
tr295:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	goto st11;
tr296:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{}
	goto st11;
tr323:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	goto st11;
tr330:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
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
    result &= user_callback(instruction_begin, current_position,
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
		case 19u: goto st29;
		case 23u: goto st29;
		case 24u: goto st30;
		case 31u: goto st31;
		case 43u: goto st29;
		case 49u: goto tr26;
		case 56u: goto st33;
		case 58u: goto st37;
		case 80u: goto st48;
		case 112u: goto st49;
		case 115u: goto st51;
		case 119u: goto tr35;
		case 162u: goto tr0;
		case 164u: goto st56;
		case 165u: goto st1;
		case 172u: goto st56;
		case 174u: goto st57;
		case 195u: goto st59;
		case 196u: goto st49;
		case 197u: goto st60;
		case 199u: goto st61;
		case 212u: goto st32;
		case 215u: goto st62;
		case 218u: goto st63;
		case 222u: goto st63;
		case 224u: goto st63;
		case 229u: goto st35;
		case 231u: goto st64;
		case 234u: goto st63;
		case 238u: goto st63;
		case 244u: goto st32;
		case 246u: goto st63;
		case 247u: goto st65;
		case 251u: goto st32;
	}
	if ( (*( current_position)) < 126u ) {
		if ( (*( current_position)) < 81u ) {
			if ( (*( current_position)) < 42u ) {
				if ( (*( current_position)) > 22u ) {
					if ( 40u <= (*( current_position)) && (*( current_position)) <= 41u )
						goto st28;
				} else if ( (*( current_position)) >= 16u )
					goto st28;
			} else if ( (*( current_position)) > 45u ) {
				if ( (*( current_position)) > 47u ) {
					if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
						goto st47;
				} else if ( (*( current_position)) >= 46u )
					goto st28;
			} else
				goto st32;
		} else if ( (*( current_position)) > 89u ) {
			if ( (*( current_position)) < 96u ) {
				if ( (*( current_position)) > 91u ) {
					if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
						goto st28;
				} else if ( (*( current_position)) >= 90u )
					goto st32;
			} else if ( (*( current_position)) > 107u ) {
				if ( (*( current_position)) < 113u ) {
					if ( 110u <= (*( current_position)) && (*( current_position)) <= 111u )
						goto st35;
				} else if ( (*( current_position)) > 114u ) {
					if ( 116u <= (*( current_position)) && (*( current_position)) <= 118u )
						goto st35;
				} else
					goto st50;
			} else
				goto st35;
		} else
			goto st28;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 194u ) {
			if ( (*( current_position)) < 173u ) {
				if ( (*( current_position)) > 143u ) {
					if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
						goto st47;
				} else if ( (*( current_position)) >= 128u )
					goto st52;
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
						goto st35;
				} else if ( (*( current_position)) >= 200u )
					goto tr0;
			} else if ( (*( current_position)) > 226u ) {
				if ( (*( current_position)) < 232u ) {
					if ( 227u <= (*( current_position)) && (*( current_position)) <= 228u )
						goto st63;
				} else if ( (*( current_position)) > 239u ) {
					if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
						goto st35;
				} else
					goto st35;
			} else
				goto st35;
		} else
			goto st58;
	} else
		goto st35;
	goto tr16;
st16:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof16;
case 16:
	if ( (*( current_position)) == 208u )
		goto tr49;
	if ( 200u <= (*( current_position)) && (*( current_position)) <= 201u )
		goto tr48;
	goto tr16;
st17:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof17;
case 17:
	switch( (*( current_position)) ) {
		case 4u: goto tr51;
		case 5u: goto tr52;
		case 12u: goto tr51;
		case 13u: goto tr52;
		case 68u: goto tr54;
		case 76u: goto tr54;
		case 132u: goto tr55;
		case 140u: goto tr55;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr50;
	} else if ( (*( current_position)) > 79u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
			goto tr52;
	} else
		goto tr53;
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
tr67:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	goto st19;
tr68:
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
		case 12u: goto tr62;
		case 13u: goto tr63;
		case 28u: goto tr62;
		case 29u: goto tr63;
		case 138u: goto tr62;
		case 142u: goto tr62;
		case 144u: goto tr63;
		case 148u: goto tr63;
		case 154u: goto tr63;
		case 158u: goto tr63;
		case 160u: goto tr63;
		case 164u: goto tr63;
		case 170u: goto tr63;
		case 174u: goto tr63;
		case 176u: goto tr63;
		case 180u: goto tr63;
		case 187u: goto tr62;
		case 191u: goto tr63;
	}
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 151u )
			goto tr63;
	} else if ( (*( current_position)) > 167u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto tr63;
	} else
		goto tr63;
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
	goto tr64;
tr64:
	{}
	goto st22;
st22:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof22;
case 22:
	goto tr65;
tr65:
	{}
	goto st23;
st23:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof23;
case 23:
	goto tr66;
tr66:
	{}
	goto st24;
st24:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof24;
case 24:
	goto tr67;
st25:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof25;
case 25:
	goto tr68;
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
		case 4u: goto tr70;
		case 5u: goto tr71;
		case 12u: goto tr70;
		case 13u: goto tr71;
		case 20u: goto tr70;
		case 21u: goto tr71;
		case 28u: goto tr70;
		case 29u: goto tr71;
		case 36u: goto tr70;
		case 37u: goto tr71;
		case 44u: goto tr70;
		case 45u: goto tr71;
		case 52u: goto tr70;
		case 53u: goto tr71;
		case 60u: goto tr70;
		case 61u: goto tr71;
		case 68u: goto tr73;
		case 76u: goto tr73;
		case 84u: goto tr73;
		case 92u: goto tr73;
		case 100u: goto tr73;
		case 108u: goto tr73;
		case 116u: goto tr73;
		case 124u: goto tr73;
		case 132u: goto tr74;
		case 140u: goto tr74;
		case 148u: goto tr74;
		case 156u: goto tr74;
		case 164u: goto tr74;
		case 172u: goto tr74;
		case 180u: goto tr74;
		case 188u: goto tr74;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr71;
	} else if ( (*( current_position)) >= 64u )
		goto tr72;
	goto tr69;
st29:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof29;
case 29:
	switch( (*( current_position)) ) {
		case 4u: goto tr70;
		case 12u: goto tr70;
		case 20u: goto tr70;
		case 28u: goto tr70;
		case 36u: goto tr70;
		case 44u: goto tr70;
		case 52u: goto tr70;
		case 60u: goto tr70;
		case 68u: goto tr73;
		case 76u: goto tr73;
		case 84u: goto tr73;
		case 92u: goto tr73;
		case 100u: goto tr73;
		case 108u: goto tr73;
		case 116u: goto tr73;
		case 124u: goto tr73;
		case 132u: goto tr74;
		case 140u: goto tr74;
		case 148u: goto tr74;
		case 156u: goto tr74;
		case 164u: goto tr74;
		case 172u: goto tr74;
		case 180u: goto tr74;
		case 188u: goto tr74;
	}
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr69;
			} else
				goto tr69;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr69;
			} else if ( (*( current_position)) >= 22u )
				goto tr69;
		} else
			goto tr69;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr69;
			} else if ( (*( current_position)) >= 46u )
				goto tr69;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 127u ) {
				if ( 192u <= (*( current_position)) )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto tr72;
		} else
			goto tr69;
	} else
		goto tr69;
	goto tr71;
st30:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof30;
case 30:
	switch( (*( current_position)) ) {
		case 4u: goto tr70;
		case 5u: goto tr71;
		case 12u: goto tr70;
		case 13u: goto tr71;
		case 20u: goto tr70;
		case 21u: goto tr71;
		case 28u: goto tr70;
		case 29u: goto tr71;
		case 68u: goto tr73;
		case 76u: goto tr73;
		case 84u: goto tr73;
		case 92u: goto tr73;
		case 132u: goto tr74;
		case 140u: goto tr74;
		case 148u: goto tr74;
		case 156u: goto tr74;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 31u )
			goto tr69;
	} else if ( (*( current_position)) > 95u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto tr71;
	} else
		goto tr72;
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
	switch( (*( current_position)) ) {
		case 4u: goto tr76;
		case 5u: goto tr77;
		case 12u: goto tr76;
		case 13u: goto tr77;
		case 20u: goto tr76;
		case 21u: goto tr77;
		case 28u: goto tr76;
		case 29u: goto tr77;
		case 36u: goto tr76;
		case 37u: goto tr77;
		case 44u: goto tr76;
		case 45u: goto tr77;
		case 52u: goto tr76;
		case 53u: goto tr77;
		case 60u: goto tr76;
		case 61u: goto tr77;
		case 68u: goto tr79;
		case 76u: goto tr79;
		case 84u: goto tr79;
		case 92u: goto tr79;
		case 100u: goto tr79;
		case 108u: goto tr79;
		case 116u: goto tr79;
		case 124u: goto tr79;
		case 132u: goto tr80;
		case 140u: goto tr80;
		case 148u: goto tr80;
		case 156u: goto tr80;
		case 164u: goto tr80;
		case 172u: goto tr80;
		case 180u: goto tr80;
		case 188u: goto tr80;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr77;
	} else if ( (*( current_position)) >= 64u )
		goto tr78;
	goto tr75;
st33:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof33;
case 33:
	if ( (*( current_position)) == 4u )
		goto st35;
	if ( (*( current_position)) < 28u ) {
		if ( (*( current_position)) <= 11u )
			goto st34;
	} else if ( (*( current_position)) > 30u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 241u )
			goto st36;
	} else
		goto st34;
	goto tr16;
st34:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof34;
case 34:
	switch( (*( current_position)) ) {
		case 4u: goto tr84;
		case 5u: goto tr85;
		case 12u: goto tr84;
		case 13u: goto tr85;
		case 20u: goto tr84;
		case 21u: goto tr85;
		case 28u: goto tr84;
		case 29u: goto tr85;
		case 36u: goto tr84;
		case 37u: goto tr85;
		case 44u: goto tr84;
		case 45u: goto tr85;
		case 52u: goto tr84;
		case 53u: goto tr85;
		case 60u: goto tr84;
		case 61u: goto tr85;
		case 68u: goto tr87;
		case 76u: goto tr87;
		case 84u: goto tr87;
		case 92u: goto tr87;
		case 100u: goto tr87;
		case 108u: goto tr87;
		case 116u: goto tr87;
		case 124u: goto tr87;
		case 132u: goto tr88;
		case 140u: goto tr88;
		case 148u: goto tr88;
		case 156u: goto tr88;
		case 164u: goto tr88;
		case 172u: goto tr88;
		case 180u: goto tr88;
		case 188u: goto tr88;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr85;
	} else if ( (*( current_position)) >= 64u )
		goto tr86;
	goto tr83;
st35:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof35;
case 35:
	switch( (*( current_position)) ) {
		case 4u: goto tr89;
		case 5u: goto tr90;
		case 12u: goto tr89;
		case 13u: goto tr90;
		case 20u: goto tr89;
		case 21u: goto tr90;
		case 28u: goto tr89;
		case 29u: goto tr90;
		case 36u: goto tr89;
		case 37u: goto tr90;
		case 44u: goto tr89;
		case 45u: goto tr90;
		case 52u: goto tr89;
		case 53u: goto tr90;
		case 60u: goto tr89;
		case 61u: goto tr90;
		case 68u: goto tr92;
		case 76u: goto tr92;
		case 84u: goto tr92;
		case 92u: goto tr92;
		case 100u: goto tr92;
		case 108u: goto tr92;
		case 116u: goto tr92;
		case 124u: goto tr92;
		case 132u: goto tr93;
		case 140u: goto tr93;
		case 148u: goto tr93;
		case 156u: goto tr93;
		case 164u: goto tr93;
		case 172u: goto tr93;
		case 180u: goto tr93;
		case 188u: goto tr93;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr90;
	} else if ( (*( current_position)) >= 64u )
		goto tr91;
	goto tr35;
st36:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof36;
case 36:
	switch( (*( current_position)) ) {
		case 4u: goto tr95;
		case 5u: goto tr96;
		case 12u: goto tr95;
		case 13u: goto tr96;
		case 20u: goto tr95;
		case 21u: goto tr96;
		case 28u: goto tr95;
		case 29u: goto tr96;
		case 36u: goto tr95;
		case 37u: goto tr96;
		case 44u: goto tr95;
		case 45u: goto tr96;
		case 52u: goto tr95;
		case 53u: goto tr96;
		case 60u: goto tr95;
		case 61u: goto tr96;
		case 68u: goto tr98;
		case 76u: goto tr98;
		case 84u: goto tr98;
		case 92u: goto tr98;
		case 100u: goto tr98;
		case 108u: goto tr98;
		case 116u: goto tr98;
		case 124u: goto tr98;
		case 132u: goto tr99;
		case 140u: goto tr99;
		case 148u: goto tr99;
		case 156u: goto tr99;
		case 164u: goto tr99;
		case 172u: goto tr99;
		case 180u: goto tr99;
		case 188u: goto tr99;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 63u )
			goto tr94;
	} else if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr96;
	} else
		goto tr97;
	goto tr16;
st37:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof37;
case 37:
	if ( (*( current_position)) == 15u )
		goto st38;
	goto tr16;
st38:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof38;
case 38:
	switch( (*( current_position)) ) {
		case 4u: goto tr102;
		case 5u: goto tr103;
		case 12u: goto tr102;
		case 13u: goto tr103;
		case 20u: goto tr102;
		case 21u: goto tr103;
		case 28u: goto tr102;
		case 29u: goto tr103;
		case 36u: goto tr102;
		case 37u: goto tr103;
		case 44u: goto tr102;
		case 45u: goto tr103;
		case 52u: goto tr102;
		case 53u: goto tr103;
		case 60u: goto tr102;
		case 61u: goto tr103;
		case 68u: goto tr105;
		case 76u: goto tr105;
		case 84u: goto tr105;
		case 92u: goto tr105;
		case 100u: goto tr105;
		case 108u: goto tr105;
		case 116u: goto tr105;
		case 124u: goto tr105;
		case 132u: goto tr106;
		case 140u: goto tr106;
		case 148u: goto tr106;
		case 156u: goto tr106;
		case 164u: goto tr106;
		case 172u: goto tr106;
		case 180u: goto tr106;
		case 188u: goto tr106;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr103;
	} else if ( (*( current_position)) >= 64u )
		goto tr104;
	goto tr101;
tr148:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st39;
tr252:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st39;
tr102:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st39;
tr122:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st39;
tr228:
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st39;
tr240:
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st39;
tr246:
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st39;
tr234:
	{ SET_CPU_FEATURE(CPUFeature_CLMUL);     }
	goto st39;
tr403:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st39;
tr410:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st39;
tr382:
	{ SET_CPU_FEATURE(CPUFeature_CLMULAVX);  }
	goto st39;
st39:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof39;
case 39:
	switch( (*( current_position)) ) {
		case 5u: goto st40;
		case 13u: goto st40;
		case 21u: goto st40;
		case 29u: goto st40;
		case 37u: goto st40;
		case 45u: goto st40;
		case 53u: goto st40;
		case 61u: goto st40;
		case 69u: goto st40;
		case 77u: goto st40;
		case 85u: goto st40;
		case 93u: goto st40;
		case 101u: goto st40;
		case 109u: goto st40;
		case 117u: goto st40;
		case 125u: goto st40;
		case 133u: goto st40;
		case 141u: goto st40;
		case 149u: goto st40;
		case 157u: goto st40;
		case 165u: goto st40;
		case 173u: goto st40;
		case 181u: goto st40;
		case 189u: goto st40;
		case 197u: goto st40;
		case 205u: goto st40;
		case 213u: goto st40;
		case 221u: goto st40;
		case 229u: goto st40;
		case 237u: goto st40;
		case 245u: goto st40;
		case 253u: goto st40;
	}
	goto st10;
tr149:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st40;
tr253:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st40;
tr103:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st40;
tr123:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st40;
tr229:
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st40;
tr241:
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st40;
tr247:
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st40;
tr235:
	{ SET_CPU_FEATURE(CPUFeature_CLMUL);     }
	goto st40;
tr404:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st40;
tr411:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st40;
tr383:
	{ SET_CPU_FEATURE(CPUFeature_CLMULAVX);  }
	goto st40;
st40:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof40;
case 40:
	goto tr109;
tr109:
	{}
	goto st41;
st41:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof41;
case 41:
	goto tr110;
tr110:
	{}
	goto st42;
st42:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof42;
case 42:
	goto tr111;
tr111:
	{}
	goto st43;
st43:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof43;
case 43:
	goto tr112;
tr150:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st44;
tr254:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st44;
tr104:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st44;
tr124:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st44;
tr230:
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st44;
tr242:
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st44;
tr248:
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st44;
tr236:
	{ SET_CPU_FEATURE(CPUFeature_CLMUL);     }
	goto st44;
tr405:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st44;
tr412:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st44;
tr384:
	{ SET_CPU_FEATURE(CPUFeature_CLMULAVX);  }
	goto st44;
st44:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof44;
case 44:
	goto tr113;
tr151:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st45;
tr255:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st45;
tr105:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st45;
tr125:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st45;
tr231:
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st45;
tr243:
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st45;
tr249:
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st45;
tr237:
	{ SET_CPU_FEATURE(CPUFeature_CLMUL);     }
	goto st45;
tr406:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st45;
tr413:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st45;
tr385:
	{ SET_CPU_FEATURE(CPUFeature_CLMULAVX);  }
	goto st45;
st45:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof45;
case 45:
	goto st44;
tr152:
	{ SET_CPU_FEATURE(CPUFeature_SSE);       }
	goto st46;
tr256:
	{ SET_CPU_FEATURE(CPUFeature_SSE2);      }
	goto st46;
tr106:
	{ SET_CPU_FEATURE(CPUFeature_SSSE3);     }
	goto st46;
tr126:
	{ SET_CPU_FEATURE(CPUFeature_EMMXSSE);   }
	goto st46;
tr232:
	{ SET_CPU_FEATURE(CPUFeature_SSE41);     }
	goto st46;
tr244:
	{ SET_CPU_FEATURE(CPUFeature_SSE42);     }
	goto st46;
tr250:
	{ SET_CPU_FEATURE(CPUFeature_AES);       }
	goto st46;
tr238:
	{ SET_CPU_FEATURE(CPUFeature_CLMUL);     }
	goto st46;
tr407:
	{ SET_CPU_FEATURE(CPUFeature_AESAVX);    }
	goto st46;
tr414:
	{ SET_CPU_FEATURE(CPUFeature_F16C);      }
	goto st46;
tr386:
	{ SET_CPU_FEATURE(CPUFeature_CLMULAVX);  }
	goto st46;
st46:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof46;
case 46:
	goto st40;
st47:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof47;
case 47:
	switch( (*( current_position)) ) {
		case 4u: goto tr116;
		case 5u: goto tr117;
		case 12u: goto tr116;
		case 13u: goto tr117;
		case 20u: goto tr116;
		case 21u: goto tr117;
		case 28u: goto tr116;
		case 29u: goto tr117;
		case 36u: goto tr116;
		case 37u: goto tr117;
		case 44u: goto tr116;
		case 45u: goto tr117;
		case 52u: goto tr116;
		case 53u: goto tr117;
		case 60u: goto tr116;
		case 61u: goto tr117;
		case 68u: goto tr119;
		case 76u: goto tr119;
		case 84u: goto tr119;
		case 92u: goto tr119;
		case 100u: goto tr119;
		case 108u: goto tr119;
		case 116u: goto tr119;
		case 124u: goto tr119;
		case 132u: goto tr120;
		case 140u: goto tr120;
		case 148u: goto tr120;
		case 156u: goto tr120;
		case 164u: goto tr120;
		case 172u: goto tr120;
		case 180u: goto tr120;
		case 188u: goto tr120;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr117;
	} else if ( (*( current_position)) >= 64u )
		goto tr118;
	goto tr115;
st48:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof48;
case 48:
	if ( 192u <= (*( current_position)) )
		goto tr69;
	goto tr16;
st49:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof49;
case 49:
	switch( (*( current_position)) ) {
		case 4u: goto tr122;
		case 5u: goto tr123;
		case 12u: goto tr122;
		case 13u: goto tr123;
		case 20u: goto tr122;
		case 21u: goto tr123;
		case 28u: goto tr122;
		case 29u: goto tr123;
		case 36u: goto tr122;
		case 37u: goto tr123;
		case 44u: goto tr122;
		case 45u: goto tr123;
		case 52u: goto tr122;
		case 53u: goto tr123;
		case 60u: goto tr122;
		case 61u: goto tr123;
		case 68u: goto tr125;
		case 76u: goto tr125;
		case 84u: goto tr125;
		case 92u: goto tr125;
		case 100u: goto tr125;
		case 108u: goto tr125;
		case 116u: goto tr125;
		case 124u: goto tr125;
		case 132u: goto tr126;
		case 140u: goto tr126;
		case 148u: goto tr126;
		case 156u: goto tr126;
		case 164u: goto tr126;
		case 172u: goto tr126;
		case 180u: goto tr126;
		case 188u: goto tr126;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr123;
	} else if ( (*( current_position)) >= 64u )
		goto tr124;
	goto tr121;
st50:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof50;
case 50:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr127;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr127;
	} else
		goto tr127;
	goto tr16;
st51:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof51;
case 51:
	if ( (*( current_position)) > 215u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr127;
	} else if ( (*( current_position)) >= 208u )
		goto tr127;
	goto tr16;
st52:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof52;
case 52:
	goto tr128;
tr128:
	{}
	goto st53;
st53:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof53;
case 53:
	goto tr129;
tr129:
	{}
	goto st54;
st54:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof54;
case 54:
	goto tr130;
tr130:
	{}
	goto st55;
st55:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof55;
case 55:
	goto tr131;
st56:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof56;
case 56:
	switch( (*( current_position)) ) {
		case 4u: goto st39;
		case 5u: goto st40;
		case 12u: goto st39;
		case 13u: goto st40;
		case 20u: goto st39;
		case 21u: goto st40;
		case 28u: goto st39;
		case 29u: goto st40;
		case 36u: goto st39;
		case 37u: goto st40;
		case 44u: goto st39;
		case 45u: goto st40;
		case 52u: goto st39;
		case 53u: goto st40;
		case 60u: goto st39;
		case 61u: goto st40;
		case 68u: goto st45;
		case 76u: goto st45;
		case 84u: goto st45;
		case 92u: goto st45;
		case 100u: goto st45;
		case 108u: goto st45;
		case 116u: goto st45;
		case 124u: goto st45;
		case 132u: goto st46;
		case 140u: goto st46;
		case 148u: goto st46;
		case 156u: goto st46;
		case 164u: goto st46;
		case 172u: goto st46;
		case 180u: goto st46;
		case 188u: goto st46;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st40;
	} else if ( (*( current_position)) >= 64u )
		goto st44;
	goto st10;
st57:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof57;
case 57:
	switch( (*( current_position)) ) {
		case 4u: goto tr135;
		case 5u: goto tr136;
		case 12u: goto tr135;
		case 13u: goto tr136;
		case 20u: goto tr70;
		case 21u: goto tr71;
		case 28u: goto tr70;
		case 29u: goto tr71;
		case 36u: goto tr135;
		case 37u: goto tr136;
		case 44u: goto tr135;
		case 45u: goto tr136;
		case 52u: goto tr135;
		case 53u: goto tr136;
		case 60u: goto tr138;
		case 61u: goto tr139;
		case 68u: goto tr141;
		case 76u: goto tr141;
		case 84u: goto tr73;
		case 92u: goto tr73;
		case 100u: goto tr141;
		case 108u: goto tr141;
		case 116u: goto tr141;
		case 124u: goto tr143;
		case 132u: goto tr144;
		case 140u: goto tr144;
		case 148u: goto tr74;
		case 156u: goto tr74;
		case 164u: goto tr144;
		case 172u: goto tr144;
		case 180u: goto tr144;
		case 188u: goto tr145;
		case 232u: goto tr75;
		case 240u: goto tr75;
		case 248u: goto tr146;
	}
	if ( (*( current_position)) < 80u ) {
		if ( (*( current_position)) < 32u ) {
			if ( (*( current_position)) > 15u ) {
				if ( 16u <= (*( current_position)) && (*( current_position)) <= 31u )
					goto tr69;
			} else
				goto tr49;
		} else if ( (*( current_position)) > 55u ) {
			if ( (*( current_position)) > 63u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr140;
			} else if ( (*( current_position)) >= 56u )
				goto tr137;
		} else
			goto tr49;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) < 128u ) {
			if ( (*( current_position)) > 119u ) {
				if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr142;
			} else if ( (*( current_position)) >= 96u )
				goto tr140;
		} else if ( (*( current_position)) > 143u ) {
			if ( (*( current_position)) < 160u ) {
				if ( 144u <= (*( current_position)) && (*( current_position)) <= 159u )
					goto tr71;
			} else if ( (*( current_position)) > 183u ) {
				if ( 184u <= (*( current_position)) && (*( current_position)) <= 191u )
					goto tr139;
			} else
				goto tr136;
		} else
			goto tr136;
	} else
		goto tr72;
	goto tr16;
st58:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof58;
case 58:
	switch( (*( current_position)) ) {
		case 4u: goto tr148;
		case 5u: goto tr149;
		case 12u: goto tr148;
		case 13u: goto tr149;
		case 20u: goto tr148;
		case 21u: goto tr149;
		case 28u: goto tr148;
		case 29u: goto tr149;
		case 36u: goto tr148;
		case 37u: goto tr149;
		case 44u: goto tr148;
		case 45u: goto tr149;
		case 52u: goto tr148;
		case 53u: goto tr149;
		case 60u: goto tr148;
		case 61u: goto tr149;
		case 68u: goto tr151;
		case 76u: goto tr151;
		case 84u: goto tr151;
		case 92u: goto tr151;
		case 100u: goto tr151;
		case 108u: goto tr151;
		case 116u: goto tr151;
		case 124u: goto tr151;
		case 132u: goto tr152;
		case 140u: goto tr152;
		case 148u: goto tr152;
		case 156u: goto tr152;
		case 164u: goto tr152;
		case 172u: goto tr152;
		case 180u: goto tr152;
		case 188u: goto tr152;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr149;
	} else if ( (*( current_position)) >= 64u )
		goto tr150;
	goto tr147;
st59:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof59;
case 59:
	switch( (*( current_position)) ) {
		case 4u: goto tr76;
		case 12u: goto tr76;
		case 20u: goto tr76;
		case 28u: goto tr76;
		case 36u: goto tr76;
		case 44u: goto tr76;
		case 52u: goto tr76;
		case 60u: goto tr76;
		case 68u: goto tr79;
		case 76u: goto tr79;
		case 84u: goto tr79;
		case 92u: goto tr79;
		case 100u: goto tr79;
		case 108u: goto tr79;
		case 116u: goto tr79;
		case 124u: goto tr79;
		case 132u: goto tr80;
		case 140u: goto tr80;
		case 148u: goto tr80;
		case 156u: goto tr80;
		case 164u: goto tr80;
		case 172u: goto tr80;
		case 180u: goto tr80;
		case 188u: goto tr80;
	}
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr75;
			} else
				goto tr75;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr75;
			} else if ( (*( current_position)) >= 22u )
				goto tr75;
		} else
			goto tr75;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr75;
			} else if ( (*( current_position)) >= 46u )
				goto tr75;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 127u ) {
				if ( 192u <= (*( current_position)) )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto tr78;
		} else
			goto tr75;
	} else
		goto tr75;
	goto tr77;
st60:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof60;
case 60:
	if ( 192u <= (*( current_position)) )
		goto tr121;
	goto tr16;
st61:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof61;
case 61:
	switch( (*( current_position)) ) {
		case 12u: goto tr154;
		case 13u: goto tr155;
		case 76u: goto tr157;
		case 140u: goto tr158;
	}
	if ( (*( current_position)) < 72u ) {
		if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
			goto tr153;
	} else if ( (*( current_position)) > 79u ) {
		if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
			goto tr155;
	} else
		goto tr156;
	goto tr16;
st62:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof62;
case 62:
	if ( 192u <= (*( current_position)) )
		goto tr146;
	goto tr16;
st63:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof63;
case 63:
	switch( (*( current_position)) ) {
		case 4u: goto tr159;
		case 5u: goto tr160;
		case 12u: goto tr159;
		case 13u: goto tr160;
		case 20u: goto tr159;
		case 21u: goto tr160;
		case 28u: goto tr159;
		case 29u: goto tr160;
		case 36u: goto tr159;
		case 37u: goto tr160;
		case 44u: goto tr159;
		case 45u: goto tr160;
		case 52u: goto tr159;
		case 53u: goto tr160;
		case 60u: goto tr159;
		case 61u: goto tr160;
		case 68u: goto tr162;
		case 76u: goto tr162;
		case 84u: goto tr162;
		case 92u: goto tr162;
		case 100u: goto tr162;
		case 108u: goto tr162;
		case 116u: goto tr162;
		case 124u: goto tr162;
		case 132u: goto tr163;
		case 140u: goto tr163;
		case 148u: goto tr163;
		case 156u: goto tr163;
		case 164u: goto tr163;
		case 172u: goto tr163;
		case 180u: goto tr163;
		case 188u: goto tr163;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr160;
	} else if ( (*( current_position)) >= 64u )
		goto tr161;
	goto tr146;
st64:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof64;
case 64:
	switch( (*( current_position)) ) {
		case 4u: goto tr159;
		case 12u: goto tr159;
		case 20u: goto tr159;
		case 28u: goto tr159;
		case 36u: goto tr159;
		case 44u: goto tr159;
		case 52u: goto tr159;
		case 60u: goto tr159;
		case 68u: goto tr162;
		case 76u: goto tr162;
		case 84u: goto tr162;
		case 92u: goto tr162;
		case 100u: goto tr162;
		case 108u: goto tr162;
		case 116u: goto tr162;
		case 124u: goto tr162;
		case 132u: goto tr163;
		case 140u: goto tr163;
		case 148u: goto tr163;
		case 156u: goto tr163;
		case 164u: goto tr163;
		case 172u: goto tr163;
		case 180u: goto tr163;
		case 188u: goto tr163;
	}
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr146;
			} else
				goto tr146;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr146;
			} else if ( (*( current_position)) >= 22u )
				goto tr146;
		} else
			goto tr146;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr146;
			} else if ( (*( current_position)) >= 46u )
				goto tr146;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 127u ) {
				if ( 192u <= (*( current_position)) )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto tr161;
		} else
			goto tr146;
	} else
		goto tr146;
	goto tr160;
st65:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof65;
case 65:
	if ( 192u <= (*( current_position)) )
		goto tr164;
	goto tr16;
st66:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof66;
case 66:
	if ( (*( current_position)) == 15u )
		goto st67;
	if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
		goto st68;
	goto tr16;
st67:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof67;
case 67:
	if ( 128u <= (*( current_position)) && (*( current_position)) <= 143u )
		goto st52;
	goto tr16;
st68:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof68;
case 68:
	goto tr167;
st69:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof69;
case 69:
	switch( (*( current_position)) ) {
		case 139u: goto st70;
		case 161u: goto st71;
	}
	goto tr16;
st70:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof70;
case 70:
	switch( (*( current_position)) ) {
		case 5u: goto st71;
		case 13u: goto st71;
		case 21u: goto st71;
		case 29u: goto st71;
		case 37u: goto st71;
		case 45u: goto st71;
		case 53u: goto st71;
		case 61u: goto st71;
	}
	goto tr16;
st71:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof71;
case 71:
	switch( (*( current_position)) ) {
		case 0u: goto st72;
		case 4u: goto st72;
	}
	goto tr16;
st72:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof72;
case 72:
	if ( (*( current_position)) == 0u )
		goto st73;
	goto tr16;
st73:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof73;
case 73:
	if ( (*( current_position)) == 0u )
		goto st74;
	goto tr16;
st74:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof74;
case 74:
	if ( (*( current_position)) == 0u )
		goto tr0;
	goto tr16;
st75:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof75;
case 75:
	switch( (*( current_position)) ) {
		case 1u: goto st1;
		case 3u: goto st1;
		case 5u: goto st76;
		case 9u: goto st1;
		case 11u: goto st1;
		case 13u: goto st76;
		case 15u: goto st78;
		case 17u: goto st1;
		case 19u: goto st1;
		case 21u: goto st76;
		case 25u: goto st1;
		case 27u: goto st1;
		case 29u: goto st76;
		case 33u: goto st1;
		case 35u: goto st1;
		case 37u: goto st76;
		case 41u: goto st1;
		case 43u: goto st1;
		case 45u: goto st76;
		case 46u: goto st99;
		case 49u: goto st1;
		case 51u: goto st1;
		case 53u: goto st76;
		case 57u: goto st1;
		case 59u: goto st1;
		case 61u: goto st76;
		case 102u: goto st102;
		case 104u: goto st76;
		case 105u: goto st107;
		case 107u: goto st56;
		case 129u: goto st107;
		case 131u: goto st56;
		case 133u: goto st1;
		case 135u: goto st1;
		case 137u: goto st1;
		case 139u: goto st1;
		case 141u: goto st116;
		case 143u: goto st31;
		case 161u: goto st3;
		case 163u: goto st3;
		case 165u: goto tr0;
		case 167u: goto tr0;
		case 169u: goto st76;
		case 171u: goto tr0;
		case 175u: goto tr0;
		case 193u: goto st117;
		case 199u: goto st118;
		case 209u: goto st119;
		case 211u: goto st119;
		case 240u: goto st120;
		case 242u: goto st125;
		case 243u: goto st128;
		case 247u: goto st129;
		case 255u: goto st130;
	}
	if ( (*( current_position)) < 144u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto tr0;
	} else if ( (*( current_position)) > 153u ) {
		if ( 184u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st76;
	} else
		goto tr0;
	goto tr16;
tr278:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	goto st76;
tr279:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{}
	goto st76;
st76:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof76;
case 76:
	goto tr187;
tr187:
	{}
	goto st77;
st77:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof77;
case 77:
	goto tr188;
st78:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof78;
case 78:
	switch( (*( current_position)) ) {
		case 31u: goto st79;
		case 43u: goto st59;
		case 56u: goto st82;
		case 58u: goto st87;
		case 80u: goto st92;
		case 81u: goto st32;
		case 112u: goto st93;
		case 115u: goto st95;
		case 121u: goto st96;
		case 175u: goto st1;
		case 194u: goto st93;
		case 196u: goto st58;
		case 197u: goto st98;
		case 198u: goto st93;
		case 215u: goto st92;
		case 231u: goto st59;
		case 247u: goto st92;
	}
	if ( (*( current_position)) < 113u ) {
		if ( (*( current_position)) < 22u ) {
			if ( (*( current_position)) < 18u ) {
				if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
					goto st32;
			} else if ( (*( current_position)) > 19u ) {
				if ( 20u <= (*( current_position)) && (*( current_position)) <= 21u )
					goto st32;
			} else
				goto st59;
		} else if ( (*( current_position)) > 23u ) {
			if ( (*( current_position)) < 64u ) {
				if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto st32;
			} else if ( (*( current_position)) > 79u ) {
				if ( 84u <= (*( current_position)) && (*( current_position)) <= 111u )
					goto st32;
			} else
				goto st47;
		} else
			goto st59;
	} else if ( (*( current_position)) > 114u ) {
		if ( (*( current_position)) < 182u ) {
			if ( (*( current_position)) < 124u ) {
				if ( 116u <= (*( current_position)) && (*( current_position)) <= 118u )
					goto st32;
			} else if ( (*( current_position)) > 125u ) {
				if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto st32;
			} else
				goto st97;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) < 208u ) {
				if ( 190u <= (*( current_position)) && (*( current_position)) <= 191u )
					goto st1;
			} else if ( (*( current_position)) > 239u ) {
				if ( 241u <= (*( current_position)) && (*( current_position)) <= 254u )
					goto st32;
			} else
				goto st32;
		} else
			goto st1;
	} else
		goto st94;
	goto tr16;
st79:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof79;
case 79:
	switch( (*( current_position)) ) {
		case 68u: goto st73;
		case 132u: goto st80;
	}
	goto tr16;
st80:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof80;
case 80:
	if ( (*( current_position)) == 0u )
		goto st81;
	goto tr16;
st81:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof81;
case 81:
	if ( (*( current_position)) == 0u )
		goto st72;
	goto tr16;
st82:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof82;
case 82:
	switch( (*( current_position)) ) {
		case 16u: goto st83;
		case 23u: goto st83;
		case 42u: goto st84;
		case 55u: goto st85;
	}
	if ( (*( current_position)) < 32u ) {
		if ( (*( current_position)) < 20u ) {
			if ( (*( current_position)) <= 11u )
				goto st34;
		} else if ( (*( current_position)) > 21u ) {
			if ( 28u <= (*( current_position)) && (*( current_position)) <= 30u )
				goto st34;
		} else
			goto st83;
	} else if ( (*( current_position)) > 37u ) {
		if ( (*( current_position)) < 48u ) {
			if ( 40u <= (*( current_position)) && (*( current_position)) <= 43u )
				goto st83;
		} else if ( (*( current_position)) > 53u ) {
			if ( (*( current_position)) > 65u ) {
				if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto st86;
			} else if ( (*( current_position)) >= 56u )
				goto st83;
		} else
			goto st83;
	} else
		goto st83;
	goto tr16;
st83:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof83;
case 83:
	switch( (*( current_position)) ) {
		case 4u: goto tr206;
		case 5u: goto tr207;
		case 12u: goto tr206;
		case 13u: goto tr207;
		case 20u: goto tr206;
		case 21u: goto tr207;
		case 28u: goto tr206;
		case 29u: goto tr207;
		case 36u: goto tr206;
		case 37u: goto tr207;
		case 44u: goto tr206;
		case 45u: goto tr207;
		case 52u: goto tr206;
		case 53u: goto tr207;
		case 60u: goto tr206;
		case 61u: goto tr207;
		case 68u: goto tr209;
		case 76u: goto tr209;
		case 84u: goto tr209;
		case 92u: goto tr209;
		case 100u: goto tr209;
		case 108u: goto tr209;
		case 116u: goto tr209;
		case 124u: goto tr209;
		case 132u: goto tr210;
		case 140u: goto tr210;
		case 148u: goto tr210;
		case 156u: goto tr210;
		case 164u: goto tr210;
		case 172u: goto tr210;
		case 180u: goto tr210;
		case 188u: goto tr210;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr207;
	} else if ( (*( current_position)) >= 64u )
		goto tr208;
	goto tr205;
st84:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof84;
case 84:
	switch( (*( current_position)) ) {
		case 4u: goto tr206;
		case 12u: goto tr206;
		case 20u: goto tr206;
		case 28u: goto tr206;
		case 36u: goto tr206;
		case 44u: goto tr206;
		case 52u: goto tr206;
		case 60u: goto tr206;
		case 68u: goto tr209;
		case 76u: goto tr209;
		case 84u: goto tr209;
		case 92u: goto tr209;
		case 100u: goto tr209;
		case 108u: goto tr209;
		case 116u: goto tr209;
		case 124u: goto tr209;
		case 132u: goto tr210;
		case 140u: goto tr210;
		case 148u: goto tr210;
		case 156u: goto tr210;
		case 164u: goto tr210;
		case 172u: goto tr210;
		case 180u: goto tr210;
		case 188u: goto tr210;
	}
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr205;
			} else
				goto tr205;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr205;
			} else if ( (*( current_position)) >= 22u )
				goto tr205;
		} else
			goto tr205;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr205;
			} else if ( (*( current_position)) >= 46u )
				goto tr205;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 127u ) {
				if ( 192u <= (*( current_position)) )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto tr208;
		} else
			goto tr205;
	} else
		goto tr205;
	goto tr207;
st85:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof85;
case 85:
	switch( (*( current_position)) ) {
		case 4u: goto tr212;
		case 5u: goto tr213;
		case 12u: goto tr212;
		case 13u: goto tr213;
		case 20u: goto tr212;
		case 21u: goto tr213;
		case 28u: goto tr212;
		case 29u: goto tr213;
		case 36u: goto tr212;
		case 37u: goto tr213;
		case 44u: goto tr212;
		case 45u: goto tr213;
		case 52u: goto tr212;
		case 53u: goto tr213;
		case 60u: goto tr212;
		case 61u: goto tr213;
		case 68u: goto tr215;
		case 76u: goto tr215;
		case 84u: goto tr215;
		case 92u: goto tr215;
		case 100u: goto tr215;
		case 108u: goto tr215;
		case 116u: goto tr215;
		case 124u: goto tr215;
		case 132u: goto tr216;
		case 140u: goto tr216;
		case 148u: goto tr216;
		case 156u: goto tr216;
		case 164u: goto tr216;
		case 172u: goto tr216;
		case 180u: goto tr216;
		case 188u: goto tr216;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr213;
	} else if ( (*( current_position)) >= 64u )
		goto tr214;
	goto tr211;
st86:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof86;
case 86:
	switch( (*( current_position)) ) {
		case 4u: goto tr218;
		case 5u: goto tr219;
		case 12u: goto tr218;
		case 13u: goto tr219;
		case 20u: goto tr218;
		case 21u: goto tr219;
		case 28u: goto tr218;
		case 29u: goto tr219;
		case 36u: goto tr218;
		case 37u: goto tr219;
		case 44u: goto tr218;
		case 45u: goto tr219;
		case 52u: goto tr218;
		case 53u: goto tr219;
		case 60u: goto tr218;
		case 61u: goto tr219;
		case 68u: goto tr221;
		case 76u: goto tr221;
		case 84u: goto tr221;
		case 92u: goto tr221;
		case 100u: goto tr221;
		case 108u: goto tr221;
		case 116u: goto tr221;
		case 124u: goto tr221;
		case 132u: goto tr222;
		case 140u: goto tr222;
		case 148u: goto tr222;
		case 156u: goto tr222;
		case 164u: goto tr222;
		case 172u: goto tr222;
		case 180u: goto tr222;
		case 188u: goto tr222;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr219;
	} else if ( (*( current_position)) >= 64u )
		goto tr220;
	goto tr217;
st87:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof87;
case 87:
	switch( (*( current_position)) ) {
		case 15u: goto st38;
		case 68u: goto st89;
		case 223u: goto st91;
	}
	if ( (*( current_position)) < 32u ) {
		if ( (*( current_position)) > 14u ) {
			if ( 20u <= (*( current_position)) && (*( current_position)) <= 23u )
				goto st88;
		} else if ( (*( current_position)) >= 8u )
			goto st88;
	} else if ( (*( current_position)) > 34u ) {
		if ( (*( current_position)) > 66u ) {
			if ( 96u <= (*( current_position)) && (*( current_position)) <= 99u )
				goto st90;
		} else if ( (*( current_position)) >= 64u )
			goto st88;
	} else
		goto st88;
	goto tr16;
st88:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof88;
case 88:
	switch( (*( current_position)) ) {
		case 4u: goto tr228;
		case 5u: goto tr229;
		case 12u: goto tr228;
		case 13u: goto tr229;
		case 20u: goto tr228;
		case 21u: goto tr229;
		case 28u: goto tr228;
		case 29u: goto tr229;
		case 36u: goto tr228;
		case 37u: goto tr229;
		case 44u: goto tr228;
		case 45u: goto tr229;
		case 52u: goto tr228;
		case 53u: goto tr229;
		case 60u: goto tr228;
		case 61u: goto tr229;
		case 68u: goto tr231;
		case 76u: goto tr231;
		case 84u: goto tr231;
		case 92u: goto tr231;
		case 100u: goto tr231;
		case 108u: goto tr231;
		case 116u: goto tr231;
		case 124u: goto tr231;
		case 132u: goto tr232;
		case 140u: goto tr232;
		case 148u: goto tr232;
		case 156u: goto tr232;
		case 164u: goto tr232;
		case 172u: goto tr232;
		case 180u: goto tr232;
		case 188u: goto tr232;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr229;
	} else if ( (*( current_position)) >= 64u )
		goto tr230;
	goto tr227;
st89:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof89;
case 89:
	switch( (*( current_position)) ) {
		case 4u: goto tr234;
		case 5u: goto tr235;
		case 12u: goto tr234;
		case 13u: goto tr235;
		case 20u: goto tr234;
		case 21u: goto tr235;
		case 28u: goto tr234;
		case 29u: goto tr235;
		case 36u: goto tr234;
		case 37u: goto tr235;
		case 44u: goto tr234;
		case 45u: goto tr235;
		case 52u: goto tr234;
		case 53u: goto tr235;
		case 60u: goto tr234;
		case 61u: goto tr235;
		case 68u: goto tr237;
		case 76u: goto tr237;
		case 84u: goto tr237;
		case 92u: goto tr237;
		case 100u: goto tr237;
		case 108u: goto tr237;
		case 116u: goto tr237;
		case 124u: goto tr237;
		case 132u: goto tr238;
		case 140u: goto tr238;
		case 148u: goto tr238;
		case 156u: goto tr238;
		case 164u: goto tr238;
		case 172u: goto tr238;
		case 180u: goto tr238;
		case 188u: goto tr238;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr235;
	} else if ( (*( current_position)) >= 64u )
		goto tr236;
	goto tr233;
st90:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof90;
case 90:
	switch( (*( current_position)) ) {
		case 4u: goto tr240;
		case 5u: goto tr241;
		case 12u: goto tr240;
		case 13u: goto tr241;
		case 20u: goto tr240;
		case 21u: goto tr241;
		case 28u: goto tr240;
		case 29u: goto tr241;
		case 36u: goto tr240;
		case 37u: goto tr241;
		case 44u: goto tr240;
		case 45u: goto tr241;
		case 52u: goto tr240;
		case 53u: goto tr241;
		case 60u: goto tr240;
		case 61u: goto tr241;
		case 68u: goto tr243;
		case 76u: goto tr243;
		case 84u: goto tr243;
		case 92u: goto tr243;
		case 100u: goto tr243;
		case 108u: goto tr243;
		case 116u: goto tr243;
		case 124u: goto tr243;
		case 132u: goto tr244;
		case 140u: goto tr244;
		case 148u: goto tr244;
		case 156u: goto tr244;
		case 164u: goto tr244;
		case 172u: goto tr244;
		case 180u: goto tr244;
		case 188u: goto tr244;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr241;
	} else if ( (*( current_position)) >= 64u )
		goto tr242;
	goto tr239;
st91:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof91;
case 91:
	switch( (*( current_position)) ) {
		case 4u: goto tr246;
		case 5u: goto tr247;
		case 12u: goto tr246;
		case 13u: goto tr247;
		case 20u: goto tr246;
		case 21u: goto tr247;
		case 28u: goto tr246;
		case 29u: goto tr247;
		case 36u: goto tr246;
		case 37u: goto tr247;
		case 44u: goto tr246;
		case 45u: goto tr247;
		case 52u: goto tr246;
		case 53u: goto tr247;
		case 60u: goto tr246;
		case 61u: goto tr247;
		case 68u: goto tr249;
		case 76u: goto tr249;
		case 84u: goto tr249;
		case 92u: goto tr249;
		case 100u: goto tr249;
		case 108u: goto tr249;
		case 116u: goto tr249;
		case 124u: goto tr249;
		case 132u: goto tr250;
		case 140u: goto tr250;
		case 148u: goto tr250;
		case 156u: goto tr250;
		case 164u: goto tr250;
		case 172u: goto tr250;
		case 180u: goto tr250;
		case 188u: goto tr250;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr247;
	} else if ( (*( current_position)) >= 64u )
		goto tr248;
	goto tr245;
st92:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof92;
case 92:
	if ( 192u <= (*( current_position)) )
		goto tr75;
	goto tr16;
st93:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof93;
case 93:
	switch( (*( current_position)) ) {
		case 4u: goto tr252;
		case 5u: goto tr253;
		case 12u: goto tr252;
		case 13u: goto tr253;
		case 20u: goto tr252;
		case 21u: goto tr253;
		case 28u: goto tr252;
		case 29u: goto tr253;
		case 36u: goto tr252;
		case 37u: goto tr253;
		case 44u: goto tr252;
		case 45u: goto tr253;
		case 52u: goto tr252;
		case 53u: goto tr253;
		case 60u: goto tr252;
		case 61u: goto tr253;
		case 68u: goto tr255;
		case 76u: goto tr255;
		case 84u: goto tr255;
		case 92u: goto tr255;
		case 100u: goto tr255;
		case 108u: goto tr255;
		case 116u: goto tr255;
		case 124u: goto tr255;
		case 132u: goto tr256;
		case 140u: goto tr256;
		case 148u: goto tr256;
		case 156u: goto tr256;
		case 164u: goto tr256;
		case 172u: goto tr256;
		case 180u: goto tr256;
		case 188u: goto tr256;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr253;
	} else if ( (*( current_position)) >= 64u )
		goto tr254;
	goto tr251;
st94:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof94;
case 94:
	if ( (*( current_position)) < 224u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 215u )
			goto tr251;
	} else if ( (*( current_position)) > 231u ) {
		if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
			goto tr251;
	} else
		goto tr251;
	goto tr16;
st95:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof95;
case 95:
	if ( (*( current_position)) > 223u ) {
		if ( 240u <= (*( current_position)) )
			goto tr251;
	} else if ( (*( current_position)) >= 208u )
		goto tr251;
	goto tr16;
st96:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof96;
case 96:
	if ( 192u <= (*( current_position)) )
		goto tr257;
	goto tr16;
st97:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof97;
case 97:
	switch( (*( current_position)) ) {
		case 4u: goto tr259;
		case 5u: goto tr260;
		case 12u: goto tr259;
		case 13u: goto tr260;
		case 20u: goto tr259;
		case 21u: goto tr260;
		case 28u: goto tr259;
		case 29u: goto tr260;
		case 36u: goto tr259;
		case 37u: goto tr260;
		case 44u: goto tr259;
		case 45u: goto tr260;
		case 52u: goto tr259;
		case 53u: goto tr260;
		case 60u: goto tr259;
		case 61u: goto tr260;
		case 68u: goto tr262;
		case 76u: goto tr262;
		case 84u: goto tr262;
		case 92u: goto tr262;
		case 100u: goto tr262;
		case 108u: goto tr262;
		case 116u: goto tr262;
		case 124u: goto tr262;
		case 132u: goto tr263;
		case 140u: goto tr263;
		case 148u: goto tr263;
		case 156u: goto tr263;
		case 164u: goto tr263;
		case 172u: goto tr263;
		case 180u: goto tr263;
		case 188u: goto tr263;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr260;
	} else if ( (*( current_position)) >= 64u )
		goto tr261;
	goto tr258;
st98:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof98;
case 98:
	if ( 192u <= (*( current_position)) )
		goto tr251;
	goto tr16;
st99:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof99;
case 99:
	if ( (*( current_position)) == 15u )
		goto st100;
	goto tr16;
st100:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof100;
case 100:
	if ( (*( current_position)) == 31u )
		goto st101;
	goto tr16;
st101:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof101;
case 101:
	if ( (*( current_position)) == 132u )
		goto st80;
	goto tr16;
st102:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof102;
case 102:
	switch( (*( current_position)) ) {
		case 46u: goto st99;
		case 102u: goto st103;
	}
	goto tr16;
st103:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof103;
case 103:
	switch( (*( current_position)) ) {
		case 46u: goto st99;
		case 102u: goto st104;
	}
	goto tr16;
st104:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof104;
case 104:
	switch( (*( current_position)) ) {
		case 46u: goto st99;
		case 102u: goto st105;
	}
	goto tr16;
st105:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof105;
case 105:
	switch( (*( current_position)) ) {
		case 46u: goto st99;
		case 102u: goto st106;
	}
	goto tr16;
st106:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof106;
case 106:
	if ( (*( current_position)) == 46u )
		goto st99;
	goto tr16;
st107:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof107;
case 107:
	switch( (*( current_position)) ) {
		case 4u: goto st108;
		case 5u: goto st109;
		case 12u: goto st108;
		case 13u: goto st109;
		case 20u: goto st108;
		case 21u: goto st109;
		case 28u: goto st108;
		case 29u: goto st109;
		case 36u: goto st108;
		case 37u: goto st109;
		case 44u: goto st108;
		case 45u: goto st109;
		case 52u: goto st108;
		case 53u: goto st109;
		case 60u: goto st108;
		case 61u: goto st109;
		case 68u: goto st114;
		case 76u: goto st114;
		case 84u: goto st114;
		case 92u: goto st114;
		case 100u: goto st114;
		case 108u: goto st114;
		case 116u: goto st114;
		case 124u: goto st114;
		case 132u: goto st115;
		case 140u: goto st115;
		case 148u: goto st115;
		case 156u: goto st115;
		case 164u: goto st115;
		case 172u: goto st115;
		case 180u: goto st115;
		case 188u: goto st115;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st109;
	} else if ( (*( current_position)) >= 64u )
		goto st113;
	goto st76;
st108:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof108;
case 108:
	switch( (*( current_position)) ) {
		case 5u: goto st109;
		case 13u: goto st109;
		case 21u: goto st109;
		case 29u: goto st109;
		case 37u: goto st109;
		case 45u: goto st109;
		case 53u: goto st109;
		case 61u: goto st109;
		case 69u: goto st109;
		case 77u: goto st109;
		case 85u: goto st109;
		case 93u: goto st109;
		case 101u: goto st109;
		case 109u: goto st109;
		case 117u: goto st109;
		case 125u: goto st109;
		case 133u: goto st109;
		case 141u: goto st109;
		case 149u: goto st109;
		case 157u: goto st109;
		case 165u: goto st109;
		case 173u: goto st109;
		case 181u: goto st109;
		case 189u: goto st109;
		case 197u: goto st109;
		case 205u: goto st109;
		case 213u: goto st109;
		case 221u: goto st109;
		case 229u: goto st109;
		case 237u: goto st109;
		case 245u: goto st109;
		case 253u: goto st109;
	}
	goto st76;
st109:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof109;
case 109:
	goto tr275;
tr275:
	{}
	goto st110;
st110:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof110;
case 110:
	goto tr276;
tr276:
	{}
	goto st111;
st111:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof111;
case 111:
	goto tr277;
tr277:
	{}
	goto st112;
st112:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof112;
case 112:
	goto tr278;
st113:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof113;
case 113:
	goto tr279;
st114:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof114;
case 114:
	goto st113;
st115:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof115;
case 115:
	goto st109;
st116:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof116;
case 116:
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
st117:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof117;
case 117:
	switch( (*( current_position)) ) {
		case 4u: goto st39;
		case 5u: goto st40;
		case 12u: goto st39;
		case 13u: goto st40;
		case 20u: goto st39;
		case 21u: goto st40;
		case 28u: goto st39;
		case 29u: goto st40;
		case 36u: goto st39;
		case 37u: goto st40;
		case 44u: goto st39;
		case 45u: goto st40;
		case 60u: goto st39;
		case 61u: goto st40;
		case 68u: goto st45;
		case 76u: goto st45;
		case 84u: goto st45;
		case 92u: goto st45;
		case 100u: goto st45;
		case 108u: goto st45;
		case 124u: goto st45;
		case 132u: goto st46;
		case 140u: goto st46;
		case 148u: goto st46;
		case 156u: goto st46;
		case 164u: goto st46;
		case 172u: goto st46;
		case 188u: goto st46;
	}
	if ( (*( current_position)) < 120u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 48u <= (*( current_position)) && (*( current_position)) <= 55u )
				goto tr16;
		} else if ( (*( current_position)) > 111u ) {
			if ( 112u <= (*( current_position)) && (*( current_position)) <= 119u )
				goto tr16;
		} else
			goto st44;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 175u )
				goto st40;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 191u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr16;
			} else if ( (*( current_position)) >= 184u )
				goto st40;
		} else
			goto tr16;
	} else
		goto st44;
	goto st10;
st118:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof118;
case 118:
	switch( (*( current_position)) ) {
		case 4u: goto st108;
		case 5u: goto st109;
		case 68u: goto st114;
		case 132u: goto st115;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st76;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st76;
		} else if ( (*( current_position)) >= 128u )
			goto st109;
	} else
		goto st113;
	goto tr16;
st119:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof119;
case 119:
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
st120:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof120;
case 120:
	switch( (*( current_position)) ) {
		case 1u: goto st116;
		case 9u: goto st116;
		case 17u: goto st116;
		case 25u: goto st116;
		case 33u: goto st116;
		case 41u: goto st116;
		case 49u: goto st116;
		case 129u: goto st121;
		case 131u: goto st122;
		case 135u: goto st116;
		case 247u: goto st123;
		case 255u: goto st124;
	}
	goto tr16;
st121:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof121;
case 121:
	switch( (*( current_position)) ) {
		case 4u: goto st108;
		case 5u: goto st109;
		case 12u: goto st108;
		case 13u: goto st109;
		case 20u: goto st108;
		case 21u: goto st109;
		case 28u: goto st108;
		case 29u: goto st109;
		case 36u: goto st108;
		case 37u: goto st109;
		case 44u: goto st108;
		case 45u: goto st109;
		case 52u: goto st108;
		case 53u: goto st109;
		case 68u: goto st114;
		case 76u: goto st114;
		case 84u: goto st114;
		case 92u: goto st114;
		case 100u: goto st114;
		case 108u: goto st114;
		case 116u: goto st114;
		case 132u: goto st115;
		case 140u: goto st115;
		case 148u: goto st115;
		case 156u: goto st115;
		case 164u: goto st115;
		case 172u: goto st115;
		case 180u: goto st115;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st76;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st109;
	} else
		goto st113;
	goto tr16;
st122:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof122;
case 122:
	switch( (*( current_position)) ) {
		case 4u: goto st39;
		case 5u: goto st40;
		case 12u: goto st39;
		case 13u: goto st40;
		case 20u: goto st39;
		case 21u: goto st40;
		case 28u: goto st39;
		case 29u: goto st40;
		case 36u: goto st39;
		case 37u: goto st40;
		case 44u: goto st39;
		case 45u: goto st40;
		case 52u: goto st39;
		case 53u: goto st40;
		case 68u: goto st45;
		case 76u: goto st45;
		case 84u: goto st45;
		case 92u: goto st45;
		case 100u: goto st45;
		case 108u: goto st45;
		case 116u: goto st45;
		case 132u: goto st46;
		case 140u: goto st46;
		case 148u: goto st46;
		case 156u: goto st46;
		case 164u: goto st46;
		case 172u: goto st46;
		case 180u: goto st46;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st10;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st40;
	} else
		goto st44;
	goto tr16;
st123:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof123;
case 123:
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
st124:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof124;
case 124:
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
st125:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof125;
case 125:
	switch( (*( current_position)) ) {
		case 15u: goto st126;
		case 167u: goto tr0;
		case 175u: goto tr0;
	}
	goto tr16;
st126:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof126;
case 126:
	if ( (*( current_position)) == 56u )
		goto st127;
	goto tr16;
st127:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof127;
case 127:
	if ( (*( current_position)) == 241u )
		goto st85;
	goto tr16;
st128:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof128;
case 128:
	switch( (*( current_position)) ) {
		case 165u: goto tr0;
		case 167u: goto tr0;
		case 171u: goto tr0;
		case 175u: goto tr0;
	}
	goto tr16;
st129:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof129;
case 129:
	switch( (*( current_position)) ) {
		case 4u: goto st108;
		case 5u: goto st109;
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
		case 68u: goto st114;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st115;
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
				goto st76;
		} else if ( (*( current_position)) > 15u ) {
			if ( (*( current_position)) > 71u ) {
				if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto st113;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr16;
			} else if ( (*( current_position)) >= 128u )
				goto st109;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) > 199u ) {
				if ( 200u <= (*( current_position)) && (*( current_position)) <= 207u )
					goto tr16;
			} else if ( (*( current_position)) >= 192u )
				goto st76;
		} else
			goto st3;
	} else
		goto st7;
	goto tr0;
st130:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof130;
case 130:
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
st131:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof131;
case 131:
	switch( (*( current_position)) ) {
		case 4u: goto st132;
		case 5u: goto st133;
		case 12u: goto st132;
		case 13u: goto st133;
		case 20u: goto st132;
		case 21u: goto st133;
		case 28u: goto st132;
		case 29u: goto st133;
		case 36u: goto st132;
		case 37u: goto st133;
		case 44u: goto st132;
		case 45u: goto st133;
		case 52u: goto st132;
		case 53u: goto st133;
		case 60u: goto st132;
		case 61u: goto st133;
		case 68u: goto st138;
		case 76u: goto st138;
		case 84u: goto st138;
		case 92u: goto st138;
		case 100u: goto st138;
		case 108u: goto st138;
		case 116u: goto st138;
		case 124u: goto st138;
		case 132u: goto st139;
		case 140u: goto st139;
		case 148u: goto st139;
		case 156u: goto st139;
		case 164u: goto st139;
		case 172u: goto st139;
		case 180u: goto st139;
		case 188u: goto st139;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st133;
	} else if ( (*( current_position)) >= 64u )
		goto st137;
	goto st11;
tr324:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	goto st132;
tr331:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st132;
st132:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof132;
case 132:
	switch( (*( current_position)) ) {
		case 5u: goto st133;
		case 13u: goto st133;
		case 21u: goto st133;
		case 29u: goto st133;
		case 37u: goto st133;
		case 45u: goto st133;
		case 53u: goto st133;
		case 61u: goto st133;
		case 69u: goto st133;
		case 77u: goto st133;
		case 85u: goto st133;
		case 93u: goto st133;
		case 101u: goto st133;
		case 109u: goto st133;
		case 117u: goto st133;
		case 125u: goto st133;
		case 133u: goto st133;
		case 141u: goto st133;
		case 149u: goto st133;
		case 157u: goto st133;
		case 165u: goto st133;
		case 173u: goto st133;
		case 181u: goto st133;
		case 189u: goto st133;
		case 197u: goto st133;
		case 205u: goto st133;
		case 213u: goto st133;
		case 221u: goto st133;
		case 229u: goto st133;
		case 237u: goto st133;
		case 245u: goto st133;
		case 253u: goto st133;
	}
	goto st11;
tr325:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	goto st133;
tr332:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st133;
st133:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof133;
case 133:
	goto tr292;
tr292:
	{}
	goto st134;
st134:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof134;
case 134:
	goto tr293;
tr293:
	{}
	goto st135;
st135:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof135;
case 135:
	goto tr294;
tr294:
	{}
	goto st136;
st136:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof136;
case 136:
	goto tr295;
tr326:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	goto st137;
tr333:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st137;
st137:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof137;
case 137:
	goto tr296;
tr327:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	goto st138;
tr334:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st138;
st138:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof138;
case 138:
	goto st137;
tr328:
	{ SET_CPU_FEATURE(CPUFeature_LWP);       }
	goto st139;
tr335:
	{ SET_CPU_FEATURE(CPUFeature_BMI1);      }
	goto st139;
st139:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof139;
case 139:
	goto st133;
st140:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof140;
case 140:
	switch( (*( current_position)) ) {
		case 4u: goto st39;
		case 5u: goto st40;
		case 12u: goto st39;
		case 13u: goto st40;
		case 20u: goto st39;
		case 21u: goto st40;
		case 28u: goto st39;
		case 29u: goto st40;
		case 36u: goto st39;
		case 37u: goto st40;
		case 44u: goto st39;
		case 45u: goto st40;
		case 52u: goto st39;
		case 53u: goto st40;
		case 60u: goto st39;
		case 61u: goto st40;
		case 68u: goto st45;
		case 76u: goto st45;
		case 84u: goto st45;
		case 92u: goto st45;
		case 100u: goto st45;
		case 108u: goto st45;
		case 116u: goto st45;
		case 124u: goto st45;
		case 132u: goto st46;
		case 140u: goto st46;
		case 148u: goto st46;
		case 156u: goto st46;
		case 164u: goto st46;
		case 172u: goto st46;
		case 180u: goto st46;
		case 188u: goto st46;
		case 224u: goto st141;
		case 225u: goto st220;
		case 226u: goto st222;
		case 227u: goto st224;
		case 228u: goto st226;
		case 229u: goto st228;
		case 230u: goto st230;
		case 231u: goto st232;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st40;
	} else if ( (*( current_position)) >= 64u )
		goto st44;
	goto st10;
st141:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof141;
case 141:
	if ( (*( current_position)) == 224u )
		goto tr305;
	goto tr11;
tr305:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st235;
st235:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof235;
case 235:
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
		case 46u: goto st66;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st66;
		case 101u: goto st69;
		case 102u: goto st75;
		case 104u: goto st11;
		case 105u: goto st131;
		case 106u: goto st10;
		case 107u: goto st56;
		case 128u: goto st56;
		case 129u: goto st131;
		case 131u: goto st140;
		case 141u: goto st116;
		case 143u: goto st142;
		case 155u: goto tr415;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st154;
		case 198u: goto st186;
		case 199u: goto st187;
		case 201u: goto tr0;
		case 216u: goto st188;
		case 217u: goto st189;
		case 218u: goto st190;
		case 219u: goto st191;
		case 220u: goto st192;
		case 221u: goto st193;
		case 222u: goto st194;
		case 223u: goto st195;
		case 232u: goto st196;
		case 233u: goto st52;
		case 235u: goto st68;
		case 240u: goto st200;
		case 242u: goto st203;
		case 243u: goto st211;
		case 246u: goto st216;
		case 247u: goto st217;
		case 254u: goto st218;
		case 255u: goto st219;
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
						goto st68;
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
						goto st117;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st119;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
st142:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof142;
case 142:
	switch( (*( current_position)) ) {
		case 4u: goto st2;
		case 5u: goto st3;
		case 68u: goto st8;
		case 132u: goto st9;
		case 233u: goto st143;
		case 234u: goto st149;
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
st143:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof143;
case 143:
	switch( (*( current_position)) ) {
		case 64u: goto tr308;
		case 72u: goto tr308;
		case 80u: goto tr308;
		case 88u: goto tr308;
		case 96u: goto tr308;
		case 104u: goto tr308;
		case 112u: goto tr308;
		case 120u: goto tr309;
	}
	goto tr16;
tr308:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st144;
st144:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof144;
case 144:
	switch( (*( current_position)) ) {
		case 1u: goto st145;
		case 2u: goto st146;
	}
	goto tr16;
st145:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof145;
case 145:
	switch( (*( current_position)) ) {
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
		case 76u: goto tr316;
		case 84u: goto tr316;
		case 92u: goto tr316;
		case 100u: goto tr316;
		case 108u: goto tr316;
		case 116u: goto tr316;
		case 124u: goto tr316;
		case 140u: goto tr317;
		case 148u: goto tr317;
		case 156u: goto tr317;
		case 164u: goto tr317;
		case 172u: goto tr317;
		case 180u: goto tr317;
		case 188u: goto tr317;
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
			goto tr314;
	} else
		goto tr315;
	goto tr312;
st146:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof146;
case 146:
	switch( (*( current_position)) ) {
		case 12u: goto tr313;
		case 13u: goto tr314;
		case 52u: goto tr313;
		case 53u: goto tr314;
		case 76u: goto tr316;
		case 116u: goto tr316;
		case 140u: goto tr317;
		case 180u: goto tr317;
	}
	if ( (*( current_position)) < 112u ) {
		if ( (*( current_position)) < 48u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
				goto tr312;
		} else if ( (*( current_position)) > 55u ) {
			if ( 72u <= (*( current_position)) && (*( current_position)) <= 79u )
				goto tr315;
		} else
			goto tr312;
	} else if ( (*( current_position)) > 119u ) {
		if ( (*( current_position)) < 176u ) {
			if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
				goto tr314;
		} else if ( (*( current_position)) > 183u ) {
			if ( (*( current_position)) > 207u ) {
				if ( 240u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr312;
			} else if ( (*( current_position)) >= 200u )
				goto tr312;
		} else
			goto tr314;
	} else
		goto tr315;
	goto tr16;
tr309:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st147;
st147:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof147;
case 147:
	switch( (*( current_position)) ) {
		case 1u: goto st145;
		case 2u: goto st146;
		case 18u: goto st148;
	}
	goto tr16;
st148:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof148;
case 148:
	if ( 192u <= (*( current_position)) && (*( current_position)) <= 207u )
		goto tr319;
	goto tr16;
st149:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof149;
case 149:
	switch( (*( current_position)) ) {
		case 64u: goto tr320;
		case 72u: goto tr320;
		case 80u: goto tr320;
		case 88u: goto tr320;
		case 96u: goto tr320;
		case 104u: goto tr320;
		case 112u: goto tr320;
		case 120u: goto tr321;
	}
	goto tr16;
tr320:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st150;
st150:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof150;
case 150:
	if ( (*( current_position)) == 18u )
		goto st151;
	goto tr16;
st151:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof151;
case 151:
	switch( (*( current_position)) ) {
		case 4u: goto tr324;
		case 5u: goto tr325;
		case 12u: goto tr324;
		case 13u: goto tr325;
		case 68u: goto tr327;
		case 76u: goto tr327;
		case 132u: goto tr328;
		case 140u: goto tr328;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 15u )
			goto tr323;
	} else if ( (*( current_position)) > 79u ) {
		if ( (*( current_position)) > 143u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 207u )
				goto tr323;
		} else if ( (*( current_position)) >= 128u )
			goto tr325;
	} else
		goto tr326;
	goto tr16;
tr321:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st152;
st152:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof152;
case 152:
	switch( (*( current_position)) ) {
		case 16u: goto st153;
		case 18u: goto st151;
	}
	goto tr16;
st153:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof153;
case 153:
	switch( (*( current_position)) ) {
		case 4u: goto tr331;
		case 5u: goto tr332;
		case 12u: goto tr331;
		case 13u: goto tr332;
		case 20u: goto tr331;
		case 21u: goto tr332;
		case 28u: goto tr331;
		case 29u: goto tr332;
		case 36u: goto tr331;
		case 37u: goto tr332;
		case 44u: goto tr331;
		case 45u: goto tr332;
		case 52u: goto tr331;
		case 53u: goto tr332;
		case 60u: goto tr331;
		case 61u: goto tr332;
		case 68u: goto tr334;
		case 76u: goto tr334;
		case 84u: goto tr334;
		case 92u: goto tr334;
		case 100u: goto tr334;
		case 108u: goto tr334;
		case 116u: goto tr334;
		case 124u: goto tr334;
		case 132u: goto tr335;
		case 140u: goto tr335;
		case 148u: goto tr335;
		case 156u: goto tr335;
		case 164u: goto tr335;
		case 172u: goto tr335;
		case 180u: goto tr335;
		case 188u: goto tr335;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr332;
	} else if ( (*( current_position)) >= 64u )
		goto tr333;
	goto tr330;
st154:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof154;
case 154:
	switch( (*( current_position)) ) {
		case 226u: goto st155;
		case 227u: goto st167;
	}
	goto tr16;
st155:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof155;
case 155:
	switch( (*( current_position)) ) {
		case 64u: goto tr338;
		case 65u: goto tr339;
		case 69u: goto tr340;
		case 72u: goto tr338;
		case 73u: goto tr339;
		case 77u: goto tr340;
		case 80u: goto tr338;
		case 81u: goto tr339;
		case 85u: goto tr340;
		case 88u: goto tr338;
		case 89u: goto tr339;
		case 93u: goto tr340;
		case 96u: goto tr338;
		case 97u: goto tr339;
		case 101u: goto tr340;
		case 104u: goto tr338;
		case 105u: goto tr339;
		case 109u: goto tr340;
		case 112u: goto tr338;
		case 113u: goto tr339;
		case 117u: goto tr340;
		case 120u: goto tr338;
		case 121u: goto tr341;
		case 125u: goto tr342;
		case 193u: goto tr343;
		case 197u: goto tr340;
		case 201u: goto tr343;
		case 205u: goto tr340;
		case 209u: goto tr343;
		case 213u: goto tr340;
		case 217u: goto tr343;
		case 221u: goto tr340;
		case 225u: goto tr343;
		case 229u: goto tr340;
		case 233u: goto tr343;
		case 237u: goto tr340;
		case 241u: goto tr343;
		case 245u: goto tr340;
		case 249u: goto tr343;
		case 253u: goto tr340;
	}
	goto tr16;
tr338:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st156;
st156:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof156;
case 156:
	switch( (*( current_position)) ) {
		case 242u: goto st157;
		case 243u: goto st158;
		case 247u: goto st157;
	}
	goto tr16;
st157:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof157;
case 157:
	switch( (*( current_position)) ) {
		case 4u: goto tr347;
		case 5u: goto tr348;
		case 12u: goto tr347;
		case 13u: goto tr348;
		case 20u: goto tr347;
		case 21u: goto tr348;
		case 28u: goto tr347;
		case 29u: goto tr348;
		case 36u: goto tr347;
		case 37u: goto tr348;
		case 44u: goto tr347;
		case 45u: goto tr348;
		case 52u: goto tr347;
		case 53u: goto tr348;
		case 60u: goto tr347;
		case 61u: goto tr348;
		case 68u: goto tr350;
		case 76u: goto tr350;
		case 84u: goto tr350;
		case 92u: goto tr350;
		case 100u: goto tr350;
		case 108u: goto tr350;
		case 116u: goto tr350;
		case 124u: goto tr350;
		case 132u: goto tr351;
		case 140u: goto tr351;
		case 148u: goto tr351;
		case 156u: goto tr351;
		case 164u: goto tr351;
		case 172u: goto tr351;
		case 180u: goto tr351;
		case 188u: goto tr351;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr348;
	} else if ( (*( current_position)) >= 64u )
		goto tr349;
	goto tr346;
st158:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof158;
case 158:
	switch( (*( current_position)) ) {
		case 12u: goto tr347;
		case 13u: goto tr348;
		case 20u: goto tr347;
		case 21u: goto tr348;
		case 28u: goto tr347;
		case 29u: goto tr348;
		case 76u: goto tr350;
		case 84u: goto tr350;
		case 92u: goto tr350;
		case 140u: goto tr351;
		case 148u: goto tr351;
		case 156u: goto tr351;
	}
	if ( (*( current_position)) < 72u ) {
		if ( 8u <= (*( current_position)) && (*( current_position)) <= 31u )
			goto tr346;
	} else if ( (*( current_position)) > 95u ) {
		if ( (*( current_position)) > 159u ) {
			if ( 200u <= (*( current_position)) && (*( current_position)) <= 223u )
				goto tr346;
		} else if ( (*( current_position)) >= 136u )
			goto tr348;
	} else
		goto tr349;
	goto tr16;
tr339:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st159;
st159:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof159;
case 159:
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto st160;
	} else if ( (*( current_position)) > 175u ) {
		if ( (*( current_position)) > 191u ) {
			if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
				goto st161;
		} else if ( (*( current_position)) >= 182u )
			goto st160;
	} else
		goto st160;
	goto tr16;
st160:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof160;
case 160:
	switch( (*( current_position)) ) {
		case 4u: goto tr355;
		case 5u: goto tr356;
		case 12u: goto tr355;
		case 13u: goto tr356;
		case 20u: goto tr355;
		case 21u: goto tr356;
		case 28u: goto tr355;
		case 29u: goto tr356;
		case 36u: goto tr355;
		case 37u: goto tr356;
		case 44u: goto tr355;
		case 45u: goto tr356;
		case 52u: goto tr355;
		case 53u: goto tr356;
		case 60u: goto tr355;
		case 61u: goto tr356;
		case 68u: goto tr358;
		case 76u: goto tr358;
		case 84u: goto tr358;
		case 92u: goto tr358;
		case 100u: goto tr358;
		case 108u: goto tr358;
		case 116u: goto tr358;
		case 124u: goto tr358;
		case 132u: goto tr359;
		case 140u: goto tr359;
		case 148u: goto tr359;
		case 156u: goto tr359;
		case 164u: goto tr359;
		case 172u: goto tr359;
		case 180u: goto tr359;
		case 188u: goto tr359;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr356;
	} else if ( (*( current_position)) >= 64u )
		goto tr357;
	goto tr354;
st161:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof161;
case 161:
	switch( (*( current_position)) ) {
		case 4u: goto tr361;
		case 5u: goto tr362;
		case 12u: goto tr361;
		case 13u: goto tr362;
		case 20u: goto tr361;
		case 21u: goto tr362;
		case 28u: goto tr361;
		case 29u: goto tr362;
		case 36u: goto tr361;
		case 37u: goto tr362;
		case 44u: goto tr361;
		case 45u: goto tr362;
		case 52u: goto tr361;
		case 53u: goto tr362;
		case 60u: goto tr361;
		case 61u: goto tr362;
		case 68u: goto tr364;
		case 76u: goto tr364;
		case 84u: goto tr364;
		case 92u: goto tr364;
		case 100u: goto tr364;
		case 108u: goto tr364;
		case 116u: goto tr364;
		case 124u: goto tr364;
		case 132u: goto tr365;
		case 140u: goto tr365;
		case 148u: goto tr365;
		case 156u: goto tr365;
		case 164u: goto tr365;
		case 172u: goto tr365;
		case 180u: goto tr365;
		case 188u: goto tr365;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr362;
	} else if ( (*( current_position)) >= 64u )
		goto tr363;
	goto tr360;
tr340:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st162;
st162:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof162;
case 162:
	switch( (*( current_position)) ) {
		case 154u: goto st160;
		case 156u: goto st160;
		case 158u: goto st160;
		case 170u: goto st160;
		case 172u: goto st160;
		case 174u: goto st160;
		case 186u: goto st160;
		case 188u: goto st160;
		case 190u: goto st160;
	}
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 152u )
			goto st160;
	} else if ( (*( current_position)) > 168u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
			goto st160;
	} else
		goto st160;
	goto tr16;
tr341:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st163;
st163:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof163;
case 163:
	if ( (*( current_position)) == 19u )
		goto st164;
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto st160;
	} else if ( (*( current_position)) > 175u ) {
		if ( (*( current_position)) > 191u ) {
			if ( 219u <= (*( current_position)) && (*( current_position)) <= 223u )
				goto st161;
		} else if ( (*( current_position)) >= 182u )
			goto st160;
	} else
		goto st160;
	goto tr16;
st164:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof164;
case 164:
	switch( (*( current_position)) ) {
		case 4u: goto tr368;
		case 5u: goto tr369;
		case 12u: goto tr368;
		case 13u: goto tr369;
		case 20u: goto tr368;
		case 21u: goto tr369;
		case 28u: goto tr368;
		case 29u: goto tr369;
		case 36u: goto tr368;
		case 37u: goto tr369;
		case 44u: goto tr368;
		case 45u: goto tr369;
		case 52u: goto tr368;
		case 53u: goto tr369;
		case 60u: goto tr368;
		case 61u: goto tr369;
		case 68u: goto tr371;
		case 76u: goto tr371;
		case 84u: goto tr371;
		case 92u: goto tr371;
		case 100u: goto tr371;
		case 108u: goto tr371;
		case 116u: goto tr371;
		case 124u: goto tr371;
		case 132u: goto tr372;
		case 140u: goto tr372;
		case 148u: goto tr372;
		case 156u: goto tr372;
		case 164u: goto tr372;
		case 172u: goto tr372;
		case 180u: goto tr372;
		case 188u: goto tr372;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr369;
	} else if ( (*( current_position)) >= 64u )
		goto tr370;
	goto tr367;
tr342:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st165;
st165:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof165;
case 165:
	switch( (*( current_position)) ) {
		case 19u: goto st164;
		case 154u: goto st160;
		case 156u: goto st160;
		case 158u: goto st160;
		case 170u: goto st160;
		case 172u: goto st160;
		case 174u: goto st160;
		case 186u: goto st160;
		case 188u: goto st160;
		case 190u: goto st160;
	}
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 152u )
			goto st160;
	} else if ( (*( current_position)) > 168u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 184u )
			goto st160;
	} else
		goto st160;
	goto tr16;
tr343:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st166;
st166:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof166;
case 166:
	if ( (*( current_position)) < 166u ) {
		if ( 150u <= (*( current_position)) && (*( current_position)) <= 159u )
			goto st160;
	} else if ( (*( current_position)) > 175u ) {
		if ( 182u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto st160;
	} else
		goto st160;
	goto tr16;
st167:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof167;
case 167:
	switch( (*( current_position)) ) {
		case 65u: goto tr373;
		case 69u: goto tr374;
		case 73u: goto tr373;
		case 77u: goto tr374;
		case 81u: goto tr373;
		case 85u: goto tr374;
		case 89u: goto tr373;
		case 93u: goto tr374;
		case 97u: goto tr373;
		case 101u: goto tr374;
		case 105u: goto tr373;
		case 109u: goto tr374;
		case 113u: goto tr373;
		case 117u: goto tr374;
		case 121u: goto tr375;
		case 125u: goto tr376;
		case 193u: goto tr377;
		case 197u: goto tr374;
		case 201u: goto tr377;
		case 205u: goto tr374;
		case 209u: goto tr377;
		case 213u: goto tr374;
		case 217u: goto tr377;
		case 221u: goto tr374;
		case 225u: goto tr377;
		case 229u: goto tr374;
		case 233u: goto tr377;
		case 237u: goto tr374;
		case 241u: goto tr377;
		case 245u: goto tr374;
		case 249u: goto tr377;
		case 253u: goto tr374;
	}
	goto tr16;
tr373:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st168;
st168:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof168;
case 168:
	switch( (*( current_position)) ) {
		case 68u: goto st169;
		case 223u: goto st180;
	}
	if ( (*( current_position)) < 104u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto st170;
	} else if ( (*( current_position)) > 111u ) {
		if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto st170;
	} else
		goto st170;
	goto tr16;
st169:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof169;
case 169:
	switch( (*( current_position)) ) {
		case 4u: goto tr382;
		case 5u: goto tr383;
		case 12u: goto tr382;
		case 13u: goto tr383;
		case 20u: goto tr382;
		case 21u: goto tr383;
		case 28u: goto tr382;
		case 29u: goto tr383;
		case 36u: goto tr382;
		case 37u: goto tr383;
		case 44u: goto tr382;
		case 45u: goto tr383;
		case 52u: goto tr382;
		case 53u: goto tr383;
		case 60u: goto tr382;
		case 61u: goto tr383;
		case 68u: goto tr385;
		case 76u: goto tr385;
		case 84u: goto tr385;
		case 92u: goto tr385;
		case 100u: goto tr385;
		case 108u: goto tr385;
		case 116u: goto tr385;
		case 124u: goto tr385;
		case 132u: goto tr386;
		case 140u: goto tr386;
		case 148u: goto tr386;
		case 156u: goto tr386;
		case 164u: goto tr386;
		case 172u: goto tr386;
		case 180u: goto tr386;
		case 188u: goto tr386;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr383;
	} else if ( (*( current_position)) >= 64u )
		goto tr384;
	goto tr381;
st170:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof170;
case 170:
	switch( (*( current_position)) ) {
		case 4u: goto tr388;
		case 5u: goto tr389;
		case 12u: goto tr388;
		case 13u: goto tr389;
		case 20u: goto tr388;
		case 21u: goto tr389;
		case 28u: goto tr388;
		case 29u: goto tr389;
		case 36u: goto tr388;
		case 37u: goto tr389;
		case 44u: goto tr388;
		case 45u: goto tr389;
		case 52u: goto tr388;
		case 53u: goto tr389;
		case 60u: goto tr388;
		case 61u: goto tr389;
		case 68u: goto tr391;
		case 76u: goto tr391;
		case 84u: goto tr391;
		case 92u: goto tr391;
		case 100u: goto tr391;
		case 108u: goto tr391;
		case 116u: goto tr391;
		case 124u: goto tr391;
		case 132u: goto tr392;
		case 140u: goto tr392;
		case 148u: goto tr392;
		case 156u: goto tr392;
		case 164u: goto tr392;
		case 172u: goto tr392;
		case 180u: goto tr392;
		case 188u: goto tr392;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr389;
	} else if ( (*( current_position)) >= 64u )
		goto tr390;
	goto tr387;
tr399:
	{
    SET_DISP_TYPE(DISP32);
    SET_DISP_PTR(current_position - 3);
  }
	{}
	goto st171;
tr400:
	{
    SET_DISP_TYPE(DISP8);
    SET_DISP_PTR(current_position);
  }
	{}
	goto st171;
tr387:
	{ SET_CPU_FEATURE(CPUFeature_FMA4);      }
	goto st171;
st171:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof171;
case 171:
	switch( (*( current_position)) ) {
		case 0u: goto tr393;
		case 16u: goto tr393;
		case 32u: goto tr393;
		case 48u: goto tr393;
		case 64u: goto tr393;
		case 80u: goto tr393;
		case 96u: goto tr393;
		case 112u: goto tr393;
	}
	goto tr16;
tr388:
	{ SET_CPU_FEATURE(CPUFeature_FMA4);      }
	goto st172;
st172:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof172;
case 172:
	switch( (*( current_position)) ) {
		case 5u: goto st173;
		case 13u: goto st173;
		case 21u: goto st173;
		case 29u: goto st173;
		case 37u: goto st173;
		case 45u: goto st173;
		case 53u: goto st173;
		case 61u: goto st173;
		case 69u: goto st173;
		case 77u: goto st173;
		case 85u: goto st173;
		case 93u: goto st173;
		case 101u: goto st173;
		case 109u: goto st173;
		case 117u: goto st173;
		case 125u: goto st173;
		case 133u: goto st173;
		case 141u: goto st173;
		case 149u: goto st173;
		case 157u: goto st173;
		case 165u: goto st173;
		case 173u: goto st173;
		case 181u: goto st173;
		case 189u: goto st173;
		case 197u: goto st173;
		case 205u: goto st173;
		case 213u: goto st173;
		case 221u: goto st173;
		case 229u: goto st173;
		case 237u: goto st173;
		case 245u: goto st173;
		case 253u: goto st173;
	}
	goto st171;
tr389:
	{ SET_CPU_FEATURE(CPUFeature_FMA4);      }
	goto st173;
st173:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof173;
case 173:
	goto tr396;
tr396:
	{}
	goto st174;
st174:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof174;
case 174:
	goto tr397;
tr397:
	{}
	goto st175;
st175:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof175;
case 175:
	goto tr398;
tr398:
	{}
	goto st176;
st176:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof176;
case 176:
	goto tr399;
tr390:
	{ SET_CPU_FEATURE(CPUFeature_FMA4);      }
	goto st177;
st177:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof177;
case 177:
	goto tr400;
tr391:
	{ SET_CPU_FEATURE(CPUFeature_FMA4);      }
	goto st178;
st178:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof178;
case 178:
	goto st177;
tr392:
	{ SET_CPU_FEATURE(CPUFeature_FMA4);      }
	goto st179;
st179:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof179;
case 179:
	goto st173;
st180:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof180;
case 180:
	switch( (*( current_position)) ) {
		case 4u: goto tr403;
		case 5u: goto tr404;
		case 12u: goto tr403;
		case 13u: goto tr404;
		case 20u: goto tr403;
		case 21u: goto tr404;
		case 28u: goto tr403;
		case 29u: goto tr404;
		case 36u: goto tr403;
		case 37u: goto tr404;
		case 44u: goto tr403;
		case 45u: goto tr404;
		case 52u: goto tr403;
		case 53u: goto tr404;
		case 60u: goto tr403;
		case 61u: goto tr404;
		case 68u: goto tr406;
		case 76u: goto tr406;
		case 84u: goto tr406;
		case 92u: goto tr406;
		case 100u: goto tr406;
		case 108u: goto tr406;
		case 116u: goto tr406;
		case 124u: goto tr406;
		case 132u: goto tr407;
		case 140u: goto tr407;
		case 148u: goto tr407;
		case 156u: goto tr407;
		case 164u: goto tr407;
		case 172u: goto tr407;
		case 180u: goto tr407;
		case 188u: goto tr407;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr404;
	} else if ( (*( current_position)) >= 64u )
		goto tr405;
	goto tr402;
tr374:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st181;
st181:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof181;
case 181:
	if ( (*( current_position)) < 108u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 104u <= (*( current_position)) && (*( current_position)) <= 105u )
				goto st170;
		} else if ( (*( current_position)) >= 92u )
			goto st170;
	} else if ( (*( current_position)) > 109u ) {
		if ( (*( current_position)) > 121u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto st170;
		} else if ( (*( current_position)) >= 120u )
			goto st170;
	} else
		goto st170;
	goto tr16;
tr375:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st182;
st182:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof182;
case 182:
	switch( (*( current_position)) ) {
		case 29u: goto st183;
		case 68u: goto st169;
		case 223u: goto st180;
	}
	if ( (*( current_position)) < 104u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto st170;
	} else if ( (*( current_position)) > 111u ) {
		if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto st170;
	} else
		goto st170;
	goto tr16;
st183:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof183;
case 183:
	switch( (*( current_position)) ) {
		case 4u: goto tr410;
		case 5u: goto tr411;
		case 12u: goto tr410;
		case 13u: goto tr411;
		case 20u: goto tr410;
		case 21u: goto tr411;
		case 28u: goto tr410;
		case 29u: goto tr411;
		case 36u: goto tr410;
		case 37u: goto tr411;
		case 44u: goto tr410;
		case 45u: goto tr411;
		case 52u: goto tr410;
		case 53u: goto tr411;
		case 60u: goto tr410;
		case 61u: goto tr411;
		case 68u: goto tr413;
		case 76u: goto tr413;
		case 84u: goto tr413;
		case 92u: goto tr413;
		case 100u: goto tr413;
		case 108u: goto tr413;
		case 116u: goto tr413;
		case 124u: goto tr413;
		case 132u: goto tr414;
		case 140u: goto tr414;
		case 148u: goto tr414;
		case 156u: goto tr414;
		case 164u: goto tr414;
		case 172u: goto tr414;
		case 180u: goto tr414;
		case 188u: goto tr414;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr411;
	} else if ( (*( current_position)) >= 64u )
		goto tr412;
	goto tr409;
tr376:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st184;
st184:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof184;
case 184:
	if ( (*( current_position)) == 29u )
		goto st183;
	if ( (*( current_position)) < 108u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 104u <= (*( current_position)) && (*( current_position)) <= 105u )
				goto st170;
		} else if ( (*( current_position)) >= 92u )
			goto st170;
	} else if ( (*( current_position)) > 109u ) {
		if ( (*( current_position)) > 121u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto st170;
		} else if ( (*( current_position)) >= 120u )
			goto st170;
	} else
		goto st170;
	goto tr16;
tr377:
	{
    SET_VEX_PREFIX3(*current_position);
  }
	goto st185;
st185:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof185;
case 185:
	if ( (*( current_position)) < 104u ) {
		if ( 92u <= (*( current_position)) && (*( current_position)) <= 95u )
			goto st170;
	} else if ( (*( current_position)) > 111u ) {
		if ( 120u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto st170;
	} else
		goto st170;
	goto tr16;
st186:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof186;
case 186:
	switch( (*( current_position)) ) {
		case 4u: goto st39;
		case 5u: goto st40;
		case 68u: goto st45;
		case 132u: goto st46;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st10;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st10;
		} else if ( (*( current_position)) >= 128u )
			goto st40;
	} else
		goto st44;
	goto tr16;
st187:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof187;
case 187:
	switch( (*( current_position)) ) {
		case 4u: goto st132;
		case 5u: goto st133;
		case 68u: goto st138;
		case 132u: goto st139;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 7u )
			goto st11;
	} else if ( (*( current_position)) > 71u ) {
		if ( (*( current_position)) > 135u ) {
			if ( 192u <= (*( current_position)) && (*( current_position)) <= 199u )
				goto st11;
		} else if ( (*( current_position)) >= 128u )
			goto st133;
	} else
		goto st137;
	goto tr16;
st188:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof188;
case 188:
	switch( (*( current_position)) ) {
		case 4u: goto tr416;
		case 5u: goto tr417;
		case 12u: goto tr416;
		case 13u: goto tr417;
		case 20u: goto tr416;
		case 21u: goto tr417;
		case 28u: goto tr416;
		case 29u: goto tr417;
		case 36u: goto tr416;
		case 37u: goto tr417;
		case 44u: goto tr416;
		case 45u: goto tr417;
		case 52u: goto tr416;
		case 53u: goto tr417;
		case 60u: goto tr416;
		case 61u: goto tr417;
		case 68u: goto tr419;
		case 76u: goto tr419;
		case 84u: goto tr419;
		case 92u: goto tr419;
		case 100u: goto tr419;
		case 108u: goto tr419;
		case 116u: goto tr419;
		case 124u: goto tr419;
		case 132u: goto tr420;
		case 140u: goto tr420;
		case 148u: goto tr420;
		case 156u: goto tr420;
		case 164u: goto tr420;
		case 172u: goto tr420;
		case 180u: goto tr420;
		case 188u: goto tr420;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr417;
	} else if ( (*( current_position)) >= 64u )
		goto tr418;
	goto tr415;
st189:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof189;
case 189:
	switch( (*( current_position)) ) {
		case 4u: goto tr416;
		case 5u: goto tr417;
		case 20u: goto tr416;
		case 21u: goto tr417;
		case 28u: goto tr416;
		case 29u: goto tr417;
		case 36u: goto tr416;
		case 37u: goto tr417;
		case 44u: goto tr416;
		case 45u: goto tr417;
		case 52u: goto tr416;
		case 53u: goto tr417;
		case 60u: goto tr416;
		case 61u: goto tr417;
		case 68u: goto tr419;
		case 84u: goto tr419;
		case 92u: goto tr419;
		case 100u: goto tr419;
		case 108u: goto tr419;
		case 116u: goto tr419;
		case 124u: goto tr419;
		case 132u: goto tr420;
		case 148u: goto tr420;
		case 156u: goto tr420;
		case 164u: goto tr420;
		case 172u: goto tr420;
		case 180u: goto tr420;
		case 188u: goto tr420;
		case 239u: goto tr16;
	}
	if ( (*( current_position)) < 128u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 8u <= (*( current_position)) && (*( current_position)) <= 15u )
				goto tr16;
		} else if ( (*( current_position)) > 71u ) {
			if ( (*( current_position)) > 79u ) {
				if ( 80u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr418;
			} else if ( (*( current_position)) >= 72u )
				goto tr16;
		} else
			goto tr418;
	} else if ( (*( current_position)) > 135u ) {
		if ( (*( current_position)) < 209u ) {
			if ( (*( current_position)) > 143u ) {
				if ( 144u <= (*( current_position)) && (*( current_position)) <= 191u )
					goto tr417;
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
		goto tr417;
	goto tr415;
st190:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof190;
case 190:
	switch( (*( current_position)) ) {
		case 4u: goto tr416;
		case 12u: goto tr416;
		case 20u: goto tr416;
		case 28u: goto tr416;
		case 36u: goto tr416;
		case 44u: goto tr416;
		case 52u: goto tr416;
		case 60u: goto tr416;
		case 68u: goto tr419;
		case 76u: goto tr419;
		case 84u: goto tr419;
		case 92u: goto tr419;
		case 100u: goto tr419;
		case 108u: goto tr419;
		case 116u: goto tr419;
		case 124u: goto tr419;
		case 132u: goto tr420;
		case 140u: goto tr420;
		case 148u: goto tr420;
		case 156u: goto tr420;
		case 164u: goto tr420;
		case 172u: goto tr420;
		case 180u: goto tr420;
		case 188u: goto tr420;
		case 233u: goto tr415;
	}
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr415;
			} else
				goto tr415;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr415;
			} else if ( (*( current_position)) >= 22u )
				goto tr415;
		} else
			goto tr415;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr415;
			} else if ( (*( current_position)) >= 46u )
				goto tr415;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) < 192u ) {
				if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr418;
			} else if ( (*( current_position)) > 223u ) {
				if ( 224u <= (*( current_position)) )
					goto tr16;
			} else
				goto tr421;
		} else
			goto tr415;
	} else
		goto tr415;
	goto tr417;
st191:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof191;
case 191:
	switch( (*( current_position)) ) {
		case 4u: goto tr416;
		case 5u: goto tr417;
		case 12u: goto tr416;
		case 13u: goto tr417;
		case 20u: goto tr416;
		case 21u: goto tr417;
		case 28u: goto tr416;
		case 29u: goto tr417;
		case 44u: goto tr416;
		case 45u: goto tr417;
		case 60u: goto tr416;
		case 61u: goto tr417;
		case 68u: goto tr419;
		case 76u: goto tr419;
		case 84u: goto tr419;
		case 92u: goto tr419;
		case 108u: goto tr419;
		case 124u: goto tr419;
		case 132u: goto tr420;
		case 140u: goto tr420;
		case 148u: goto tr420;
		case 156u: goto tr420;
		case 172u: goto tr420;
		case 188u: goto tr420;
	}
	if ( (*( current_position)) < 120u ) {
		if ( (*( current_position)) < 56u ) {
			if ( (*( current_position)) > 31u ) {
				if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
					goto tr415;
			} else
				goto tr415;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 95u ) {
				if ( 104u <= (*( current_position)) && (*( current_position)) <= 111u )
					goto tr418;
			} else if ( (*( current_position)) >= 64u )
				goto tr418;
		} else
			goto tr415;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 184u ) {
			if ( (*( current_position)) > 159u ) {
				if ( 168u <= (*( current_position)) && (*( current_position)) <= 175u )
					goto tr417;
			} else if ( (*( current_position)) >= 128u )
				goto tr417;
		} else if ( (*( current_position)) > 191u ) {
			if ( (*( current_position)) < 226u ) {
				if ( 192u <= (*( current_position)) && (*( current_position)) <= 223u )
					goto tr421;
			} else if ( (*( current_position)) > 227u ) {
				if ( 232u <= (*( current_position)) && (*( current_position)) <= 247u )
					goto tr415;
			} else
				goto tr415;
		} else
			goto tr417;
	} else
		goto tr418;
	goto tr16;
st192:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof192;
case 192:
	switch( (*( current_position)) ) {
		case 4u: goto tr416;
		case 5u: goto tr417;
		case 12u: goto tr416;
		case 13u: goto tr417;
		case 20u: goto tr416;
		case 21u: goto tr417;
		case 28u: goto tr416;
		case 29u: goto tr417;
		case 36u: goto tr416;
		case 37u: goto tr417;
		case 44u: goto tr416;
		case 45u: goto tr417;
		case 52u: goto tr416;
		case 53u: goto tr417;
		case 60u: goto tr416;
		case 61u: goto tr417;
		case 68u: goto tr419;
		case 76u: goto tr419;
		case 84u: goto tr419;
		case 92u: goto tr419;
		case 100u: goto tr419;
		case 108u: goto tr419;
		case 116u: goto tr419;
		case 124u: goto tr419;
		case 132u: goto tr420;
		case 140u: goto tr420;
		case 148u: goto tr420;
		case 156u: goto tr420;
		case 164u: goto tr420;
		case 172u: goto tr420;
		case 180u: goto tr420;
		case 188u: goto tr420;
	}
	if ( (*( current_position)) < 128u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto tr418;
	} else if ( (*( current_position)) > 191u ) {
		if ( 208u <= (*( current_position)) && (*( current_position)) <= 223u )
			goto tr16;
	} else
		goto tr417;
	goto tr415;
st193:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof193;
case 193:
	switch( (*( current_position)) ) {
		case 4u: goto tr416;
		case 5u: goto tr417;
		case 12u: goto tr416;
		case 13u: goto tr417;
		case 20u: goto tr416;
		case 21u: goto tr417;
		case 28u: goto tr416;
		case 29u: goto tr417;
		case 36u: goto tr416;
		case 37u: goto tr417;
		case 52u: goto tr416;
		case 53u: goto tr417;
		case 60u: goto tr416;
		case 61u: goto tr417;
		case 68u: goto tr419;
		case 76u: goto tr419;
		case 84u: goto tr419;
		case 92u: goto tr419;
		case 100u: goto tr419;
		case 116u: goto tr419;
		case 124u: goto tr419;
		case 132u: goto tr420;
		case 140u: goto tr420;
		case 148u: goto tr420;
		case 156u: goto tr420;
		case 164u: goto tr420;
		case 180u: goto tr420;
		case 188u: goto tr420;
	}
	if ( (*( current_position)) < 128u ) {
		if ( (*( current_position)) < 64u ) {
			if ( 40u <= (*( current_position)) && (*( current_position)) <= 47u )
				goto tr16;
		} else if ( (*( current_position)) > 103u ) {
			if ( (*( current_position)) > 111u ) {
				if ( 112u <= (*( current_position)) && (*( current_position)) <= 127u )
					goto tr418;
			} else if ( (*( current_position)) >= 104u )
				goto tr16;
		} else
			goto tr418;
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
			goto tr417;
	} else
		goto tr417;
	goto tr415;
st194:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof194;
case 194:
	switch( (*( current_position)) ) {
		case 4u: goto tr416;
		case 5u: goto tr417;
		case 12u: goto tr416;
		case 13u: goto tr417;
		case 20u: goto tr416;
		case 21u: goto tr417;
		case 28u: goto tr416;
		case 29u: goto tr417;
		case 36u: goto tr416;
		case 37u: goto tr417;
		case 44u: goto tr416;
		case 45u: goto tr417;
		case 52u: goto tr416;
		case 53u: goto tr417;
		case 60u: goto tr416;
		case 61u: goto tr417;
		case 68u: goto tr419;
		case 76u: goto tr419;
		case 84u: goto tr419;
		case 92u: goto tr419;
		case 100u: goto tr419;
		case 108u: goto tr419;
		case 116u: goto tr419;
		case 124u: goto tr419;
		case 132u: goto tr420;
		case 140u: goto tr420;
		case 148u: goto tr420;
		case 156u: goto tr420;
		case 164u: goto tr420;
		case 172u: goto tr420;
		case 180u: goto tr420;
		case 188u: goto tr420;
	}
	if ( (*( current_position)) < 128u ) {
		if ( 64u <= (*( current_position)) && (*( current_position)) <= 127u )
			goto tr418;
	} else if ( (*( current_position)) > 191u ) {
		if ( (*( current_position)) > 216u ) {
			if ( 218u <= (*( current_position)) && (*( current_position)) <= 223u )
				goto tr16;
		} else if ( (*( current_position)) >= 208u )
			goto tr16;
	} else
		goto tr417;
	goto tr415;
st195:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof195;
case 195:
	switch( (*( current_position)) ) {
		case 4u: goto tr416;
		case 5u: goto tr417;
		case 12u: goto tr416;
		case 13u: goto tr417;
		case 20u: goto tr416;
		case 21u: goto tr417;
		case 28u: goto tr416;
		case 29u: goto tr417;
		case 36u: goto tr416;
		case 37u: goto tr417;
		case 44u: goto tr416;
		case 45u: goto tr417;
		case 52u: goto tr416;
		case 53u: goto tr417;
		case 60u: goto tr416;
		case 61u: goto tr417;
		case 68u: goto tr419;
		case 76u: goto tr419;
		case 84u: goto tr419;
		case 92u: goto tr419;
		case 100u: goto tr419;
		case 108u: goto tr419;
		case 116u: goto tr419;
		case 124u: goto tr419;
		case 132u: goto tr420;
		case 140u: goto tr420;
		case 148u: goto tr420;
		case 156u: goto tr420;
		case 164u: goto tr420;
		case 172u: goto tr420;
		case 180u: goto tr420;
		case 188u: goto tr420;
	}
	if ( (*( current_position)) < 192u ) {
		if ( (*( current_position)) > 127u ) {
			if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
				goto tr417;
		} else if ( (*( current_position)) >= 64u )
			goto tr418;
	} else if ( (*( current_position)) > 223u ) {
		if ( (*( current_position)) > 231u ) {
			if ( 248u <= (*( current_position)) )
				goto tr16;
		} else if ( (*( current_position)) >= 225u )
			goto tr16;
	} else
		goto tr16;
	goto tr415;
st196:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof196;
case 196:
	goto tr422;
tr422:
	{}
	goto st197;
st197:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof197;
case 197:
	goto tr423;
tr423:
	{}
	goto st198;
st198:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof198;
case 198:
	goto tr424;
tr424:
	{}
	goto st199;
st199:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof199;
case 199:
	goto tr425;
st200:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof200;
case 200:
	switch( (*( current_position)) ) {
		case 15u: goto st201;
		case 102u: goto st120;
		case 128u: goto st122;
		case 129u: goto st202;
		case 131u: goto st122;
	}
	if ( (*( current_position)) < 32u ) {
		if ( (*( current_position)) < 8u ) {
			if ( (*( current_position)) <= 1u )
				goto st116;
		} else if ( (*( current_position)) > 9u ) {
			if ( (*( current_position)) > 17u ) {
				if ( 24u <= (*( current_position)) && (*( current_position)) <= 25u )
					goto st116;
			} else if ( (*( current_position)) >= 16u )
				goto st116;
		} else
			goto st116;
	} else if ( (*( current_position)) > 33u ) {
		if ( (*( current_position)) < 134u ) {
			if ( (*( current_position)) > 41u ) {
				if ( 48u <= (*( current_position)) && (*( current_position)) <= 49u )
					goto st116;
			} else if ( (*( current_position)) >= 40u )
				goto st116;
		} else if ( (*( current_position)) > 135u ) {
			if ( (*( current_position)) > 247u ) {
				if ( 254u <= (*( current_position)) )
					goto st124;
			} else if ( (*( current_position)) >= 246u )
				goto st123;
		} else
			goto st116;
	} else
		goto st116;
	goto tr16;
st201:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof201;
case 201:
	if ( (*( current_position)) == 199u )
		goto st61;
	if ( (*( current_position)) > 177u ) {
		if ( 192u <= (*( current_position)) && (*( current_position)) <= 193u )
			goto st116;
	} else if ( (*( current_position)) >= 176u )
		goto st116;
	goto tr16;
st202:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof202;
case 202:
	switch( (*( current_position)) ) {
		case 4u: goto st132;
		case 5u: goto st133;
		case 12u: goto st132;
		case 13u: goto st133;
		case 20u: goto st132;
		case 21u: goto st133;
		case 28u: goto st132;
		case 29u: goto st133;
		case 36u: goto st132;
		case 37u: goto st133;
		case 44u: goto st132;
		case 45u: goto st133;
		case 52u: goto st132;
		case 53u: goto st133;
		case 68u: goto st138;
		case 76u: goto st138;
		case 84u: goto st138;
		case 92u: goto st138;
		case 100u: goto st138;
		case 108u: goto st138;
		case 116u: goto st138;
		case 132u: goto st139;
		case 140u: goto st139;
		case 148u: goto st139;
		case 156u: goto st139;
		case 164u: goto st139;
		case 172u: goto st139;
		case 180u: goto st139;
	}
	if ( (*( current_position)) < 64u ) {
		if ( (*( current_position)) <= 55u )
			goto st11;
	} else if ( (*( current_position)) > 119u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 183u )
			goto st133;
	} else
		goto st137;
	goto tr16;
st203:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof203;
case 203:
	switch( (*( current_position)) ) {
		case 15u: goto st204;
		case 102u: goto st125;
	}
	if ( (*( current_position)) > 167u ) {
		if ( 174u <= (*( current_position)) && (*( current_position)) <= 175u )
			goto tr0;
	} else if ( (*( current_position)) >= 166u )
		goto tr0;
	goto tr16;
st204:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof204;
case 204:
	switch( (*( current_position)) ) {
		case 18u: goto st97;
		case 43u: goto st205;
		case 56u: goto st206;
		case 81u: goto st32;
		case 112u: goto st93;
		case 120u: goto st207;
		case 121u: goto st96;
		case 194u: goto st93;
		case 208u: goto st28;
		case 214u: goto st92;
		case 230u: goto st32;
		case 240u: goto st210;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) > 17u ) {
			if ( 42u <= (*( current_position)) && (*( current_position)) <= 45u )
				goto st32;
		} else if ( (*( current_position)) >= 16u )
			goto st32;
	} else if ( (*( current_position)) > 90u ) {
		if ( (*( current_position)) > 95u ) {
			if ( 124u <= (*( current_position)) && (*( current_position)) <= 125u )
				goto st97;
		} else if ( (*( current_position)) >= 92u )
			goto st32;
	} else
		goto st32;
	goto tr16;
st205:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof205;
case 205:
	switch( (*( current_position)) ) {
		case 4u: goto tr433;
		case 12u: goto tr433;
		case 20u: goto tr433;
		case 28u: goto tr433;
		case 36u: goto tr433;
		case 44u: goto tr433;
		case 52u: goto tr433;
		case 60u: goto tr433;
		case 68u: goto tr436;
		case 76u: goto tr436;
		case 84u: goto tr436;
		case 92u: goto tr436;
		case 100u: goto tr436;
		case 108u: goto tr436;
		case 116u: goto tr436;
		case 124u: goto tr436;
		case 132u: goto tr437;
		case 140u: goto tr437;
		case 148u: goto tr437;
		case 156u: goto tr437;
		case 164u: goto tr437;
		case 172u: goto tr437;
		case 180u: goto tr437;
		case 188u: goto tr437;
	}
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr257;
			} else
				goto tr257;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr257;
			} else if ( (*( current_position)) >= 22u )
				goto tr257;
		} else
			goto tr257;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr257;
			} else if ( (*( current_position)) >= 46u )
				goto tr257;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 127u ) {
				if ( 192u <= (*( current_position)) )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto tr435;
		} else
			goto tr257;
	} else
		goto tr257;
	goto tr434;
st206:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof206;
case 206:
	if ( 240u <= (*( current_position)) && (*( current_position)) <= 241u )
		goto st85;
	goto tr16;
st207:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof207;
case 207:
	if ( 192u <= (*( current_position)) )
		goto tr438;
	goto tr16;
tr438:
	{ SET_CPU_FEATURE(CPUFeature_SSE4A);     }
	goto st208;
st208:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof208;
case 208:
	goto tr439;
tr439:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	goto st209;
st209:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof209;
case 209:
	goto tr440;
st210:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof210;
case 210:
	switch( (*( current_position)) ) {
		case 4u: goto tr259;
		case 12u: goto tr259;
		case 20u: goto tr259;
		case 28u: goto tr259;
		case 36u: goto tr259;
		case 44u: goto tr259;
		case 52u: goto tr259;
		case 60u: goto tr259;
		case 68u: goto tr262;
		case 76u: goto tr262;
		case 84u: goto tr262;
		case 92u: goto tr262;
		case 100u: goto tr262;
		case 108u: goto tr262;
		case 116u: goto tr262;
		case 124u: goto tr262;
		case 132u: goto tr263;
		case 140u: goto tr263;
		case 148u: goto tr263;
		case 156u: goto tr263;
		case 164u: goto tr263;
		case 172u: goto tr263;
		case 180u: goto tr263;
		case 188u: goto tr263;
	}
	if ( (*( current_position)) < 38u ) {
		if ( (*( current_position)) < 14u ) {
			if ( (*( current_position)) > 3u ) {
				if ( 6u <= (*( current_position)) && (*( current_position)) <= 11u )
					goto tr258;
			} else
				goto tr258;
		} else if ( (*( current_position)) > 19u ) {
			if ( (*( current_position)) > 27u ) {
				if ( 30u <= (*( current_position)) && (*( current_position)) <= 35u )
					goto tr258;
			} else if ( (*( current_position)) >= 22u )
				goto tr258;
		} else
			goto tr258;
	} else if ( (*( current_position)) > 43u ) {
		if ( (*( current_position)) < 62u ) {
			if ( (*( current_position)) > 51u ) {
				if ( 54u <= (*( current_position)) && (*( current_position)) <= 59u )
					goto tr258;
			} else if ( (*( current_position)) >= 46u )
				goto tr258;
		} else if ( (*( current_position)) > 63u ) {
			if ( (*( current_position)) > 127u ) {
				if ( 192u <= (*( current_position)) )
					goto tr16;
			} else if ( (*( current_position)) >= 64u )
				goto tr261;
		} else
			goto tr258;
	} else
		goto tr258;
	goto tr260;
st211:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof211;
case 211:
	switch( (*( current_position)) ) {
		case 15u: goto st212;
		case 102u: goto st128;
		case 144u: goto tr0;
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
st212:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof212;
case 212:
	switch( (*( current_position)) ) {
		case 18u: goto st97;
		case 22u: goto st97;
		case 43u: goto st205;
		case 111u: goto st32;
		case 112u: goto st93;
		case 184u: goto st213;
		case 188u: goto st214;
		case 189u: goto st215;
		case 194u: goto st58;
		case 214u: goto st92;
		case 230u: goto st32;
	}
	if ( (*( current_position)) < 88u ) {
		if ( (*( current_position)) < 42u ) {
			if ( 16u <= (*( current_position)) && (*( current_position)) <= 17u )
				goto st28;
		} else if ( (*( current_position)) > 45u ) {
			if ( 81u <= (*( current_position)) && (*( current_position)) <= 83u )
				goto st28;
		} else
			goto st28;
	} else if ( (*( current_position)) > 89u ) {
		if ( (*( current_position)) < 92u ) {
			if ( 90u <= (*( current_position)) && (*( current_position)) <= 91u )
				goto st32;
		} else if ( (*( current_position)) > 95u ) {
			if ( 126u <= (*( current_position)) && (*( current_position)) <= 127u )
				goto st32;
		} else
			goto st28;
	} else
		goto st28;
	goto tr16;
st213:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof213;
case 213:
	switch( (*( current_position)) ) {
		case 4u: goto tr446;
		case 5u: goto tr447;
		case 12u: goto tr446;
		case 13u: goto tr447;
		case 20u: goto tr446;
		case 21u: goto tr447;
		case 28u: goto tr446;
		case 29u: goto tr447;
		case 36u: goto tr446;
		case 37u: goto tr447;
		case 44u: goto tr446;
		case 45u: goto tr447;
		case 52u: goto tr446;
		case 53u: goto tr447;
		case 60u: goto tr446;
		case 61u: goto tr447;
		case 68u: goto tr449;
		case 76u: goto tr449;
		case 84u: goto tr449;
		case 92u: goto tr449;
		case 100u: goto tr449;
		case 108u: goto tr449;
		case 116u: goto tr449;
		case 124u: goto tr449;
		case 132u: goto tr450;
		case 140u: goto tr450;
		case 148u: goto tr450;
		case 156u: goto tr450;
		case 164u: goto tr450;
		case 172u: goto tr450;
		case 180u: goto tr450;
		case 188u: goto tr450;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr447;
	} else if ( (*( current_position)) >= 64u )
		goto tr448;
	goto tr445;
st214:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof214;
case 214:
	switch( (*( current_position)) ) {
		case 4u: goto tr452;
		case 5u: goto tr453;
		case 12u: goto tr452;
		case 13u: goto tr453;
		case 20u: goto tr452;
		case 21u: goto tr453;
		case 28u: goto tr452;
		case 29u: goto tr453;
		case 36u: goto tr452;
		case 37u: goto tr453;
		case 44u: goto tr452;
		case 45u: goto tr453;
		case 52u: goto tr452;
		case 53u: goto tr453;
		case 60u: goto tr452;
		case 61u: goto tr453;
		case 68u: goto tr455;
		case 76u: goto tr455;
		case 84u: goto tr455;
		case 92u: goto tr455;
		case 100u: goto tr455;
		case 108u: goto tr455;
		case 116u: goto tr455;
		case 124u: goto tr455;
		case 132u: goto tr456;
		case 140u: goto tr456;
		case 148u: goto tr456;
		case 156u: goto tr456;
		case 164u: goto tr456;
		case 172u: goto tr456;
		case 180u: goto tr456;
		case 188u: goto tr456;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr453;
	} else if ( (*( current_position)) >= 64u )
		goto tr454;
	goto tr451;
st215:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof215;
case 215:
	switch( (*( current_position)) ) {
		case 4u: goto tr458;
		case 5u: goto tr459;
		case 12u: goto tr458;
		case 13u: goto tr459;
		case 20u: goto tr458;
		case 21u: goto tr459;
		case 28u: goto tr458;
		case 29u: goto tr459;
		case 36u: goto tr458;
		case 37u: goto tr459;
		case 44u: goto tr458;
		case 45u: goto tr459;
		case 52u: goto tr458;
		case 53u: goto tr459;
		case 60u: goto tr458;
		case 61u: goto tr459;
		case 68u: goto tr461;
		case 76u: goto tr461;
		case 84u: goto tr461;
		case 92u: goto tr461;
		case 100u: goto tr461;
		case 108u: goto tr461;
		case 116u: goto tr461;
		case 124u: goto tr461;
		case 132u: goto tr462;
		case 140u: goto tr462;
		case 148u: goto tr462;
		case 156u: goto tr462;
		case 164u: goto tr462;
		case 172u: goto tr462;
		case 180u: goto tr462;
		case 188u: goto tr462;
	}
	if ( (*( current_position)) > 127u ) {
		if ( 128u <= (*( current_position)) && (*( current_position)) <= 191u )
			goto tr459;
	} else if ( (*( current_position)) >= 64u )
		goto tr460;
	goto tr457;
st216:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof216;
case 216:
	switch( (*( current_position)) ) {
		case 4u: goto st39;
		case 5u: goto st40;
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
		case 68u: goto st45;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st46;
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
				goto st44;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr16;
			} else if ( (*( current_position)) >= 128u )
				goto st40;
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
st217:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof217;
case 217:
	switch( (*( current_position)) ) {
		case 4u: goto st132;
		case 5u: goto st133;
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
		case 68u: goto st138;
		case 84u: goto st8;
		case 92u: goto st8;
		case 100u: goto st8;
		case 108u: goto st8;
		case 116u: goto st8;
		case 124u: goto st8;
		case 132u: goto st139;
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
				goto st137;
		} else
			goto tr16;
	} else if ( (*( current_position)) > 127u ) {
		if ( (*( current_position)) < 144u ) {
			if ( (*( current_position)) > 135u ) {
				if ( 136u <= (*( current_position)) && (*( current_position)) <= 143u )
					goto tr16;
			} else if ( (*( current_position)) >= 128u )
				goto st133;
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
st218:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof218;
case 218:
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
st219:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof219;
case 219:
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
		case 208u: goto tr463;
		case 224u: goto tr464;
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
st220:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof220;
case 220:
	if ( (*( current_position)) == 224u )
		goto tr465;
	goto tr11;
tr465:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st236;
st236:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof236;
case 236:
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
		case 46u: goto st66;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st66;
		case 101u: goto st69;
		case 102u: goto st75;
		case 104u: goto st11;
		case 105u: goto st131;
		case 106u: goto st10;
		case 107u: goto st56;
		case 128u: goto st56;
		case 129u: goto st131;
		case 131u: goto st140;
		case 141u: goto st116;
		case 143u: goto st142;
		case 155u: goto tr415;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st154;
		case 198u: goto st186;
		case 199u: goto st187;
		case 201u: goto tr0;
		case 216u: goto st188;
		case 217u: goto st189;
		case 218u: goto st190;
		case 219u: goto st191;
		case 220u: goto st192;
		case 221u: goto st193;
		case 222u: goto st194;
		case 223u: goto st195;
		case 232u: goto st196;
		case 233u: goto st52;
		case 235u: goto st68;
		case 240u: goto st200;
		case 242u: goto st203;
		case 243u: goto st211;
		case 246u: goto st216;
		case 247u: goto st217;
		case 254u: goto st218;
		case 255u: goto st221;
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
						goto st68;
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
						goto st117;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st119;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
st221:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof221;
case 221:
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
		case 209u: goto tr463;
		case 225u: goto tr464;
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
st222:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof222;
case 222:
	if ( (*( current_position)) == 224u )
		goto tr466;
	goto tr11;
tr466:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st237;
st237:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof237;
case 237:
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
		case 46u: goto st66;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st66;
		case 101u: goto st69;
		case 102u: goto st75;
		case 104u: goto st11;
		case 105u: goto st131;
		case 106u: goto st10;
		case 107u: goto st56;
		case 128u: goto st56;
		case 129u: goto st131;
		case 131u: goto st140;
		case 141u: goto st116;
		case 143u: goto st142;
		case 155u: goto tr415;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st154;
		case 198u: goto st186;
		case 199u: goto st187;
		case 201u: goto tr0;
		case 216u: goto st188;
		case 217u: goto st189;
		case 218u: goto st190;
		case 219u: goto st191;
		case 220u: goto st192;
		case 221u: goto st193;
		case 222u: goto st194;
		case 223u: goto st195;
		case 232u: goto st196;
		case 233u: goto st52;
		case 235u: goto st68;
		case 240u: goto st200;
		case 242u: goto st203;
		case 243u: goto st211;
		case 246u: goto st216;
		case 247u: goto st217;
		case 254u: goto st218;
		case 255u: goto st223;
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
						goto st68;
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
						goto st117;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st119;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
st223:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof223;
case 223:
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
		case 210u: goto tr463;
		case 226u: goto tr464;
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
st224:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof224;
case 224:
	if ( (*( current_position)) == 224u )
		goto tr467;
	goto tr11;
tr467:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st238;
st238:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof238;
case 238:
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
		case 46u: goto st66;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st66;
		case 101u: goto st69;
		case 102u: goto st75;
		case 104u: goto st11;
		case 105u: goto st131;
		case 106u: goto st10;
		case 107u: goto st56;
		case 128u: goto st56;
		case 129u: goto st131;
		case 131u: goto st140;
		case 141u: goto st116;
		case 143u: goto st142;
		case 155u: goto tr415;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st154;
		case 198u: goto st186;
		case 199u: goto st187;
		case 201u: goto tr0;
		case 216u: goto st188;
		case 217u: goto st189;
		case 218u: goto st190;
		case 219u: goto st191;
		case 220u: goto st192;
		case 221u: goto st193;
		case 222u: goto st194;
		case 223u: goto st195;
		case 232u: goto st196;
		case 233u: goto st52;
		case 235u: goto st68;
		case 240u: goto st200;
		case 242u: goto st203;
		case 243u: goto st211;
		case 246u: goto st216;
		case 247u: goto st217;
		case 254u: goto st218;
		case 255u: goto st225;
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
						goto st68;
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
						goto st117;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st119;
		} else
			goto st10;
	} else
		goto st1;
	goto tr16;
st225:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof225;
case 225:
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
		case 211u: goto tr463;
		case 227u: goto tr464;
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
st226:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof226;
case 226:
	if ( (*( current_position)) == 224u )
		goto tr468;
	goto tr11;
tr468:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st239;
st239:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof239;
case 239:
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
		case 46u: goto st66;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st66;
		case 101u: goto st69;
		case 102u: goto st75;
		case 104u: goto st11;
		case 105u: goto st131;
		case 106u: goto st10;
		case 107u: goto st56;
		case 128u: goto st56;
		case 129u: goto st131;
		case 131u: goto st140;
		case 141u: goto st116;
		case 143u: goto st142;
		case 155u: goto tr415;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st154;
		case 198u: goto st186;
		case 199u: goto st187;
		case 201u: goto tr0;
		case 216u: goto st188;
		case 217u: goto st189;
		case 218u: goto st190;
		case 219u: goto st191;
		case 220u: goto st192;
		case 221u: goto st193;
		case 222u: goto st194;
		case 223u: goto st195;
		case 232u: goto st196;
		case 233u: goto st52;
		case 235u: goto st68;
		case 240u: goto st200;
		case 242u: goto st203;
		case 243u: goto st211;
		case 246u: goto st216;
		case 247u: goto st217;
		case 254u: goto st218;
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
						goto st68;
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
						goto st117;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st119;
		} else
			goto st10;
	} else
		goto st1;
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
		case 212u: goto tr463;
		case 228u: goto tr464;
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
		goto tr469;
	goto tr11;
tr469:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st240;
st240:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof240;
case 240:
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
		case 46u: goto st66;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st66;
		case 101u: goto st69;
		case 102u: goto st75;
		case 104u: goto st11;
		case 105u: goto st131;
		case 106u: goto st10;
		case 107u: goto st56;
		case 128u: goto st56;
		case 129u: goto st131;
		case 131u: goto st140;
		case 141u: goto st116;
		case 143u: goto st142;
		case 155u: goto tr415;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st154;
		case 198u: goto st186;
		case 199u: goto st187;
		case 201u: goto tr0;
		case 216u: goto st188;
		case 217u: goto st189;
		case 218u: goto st190;
		case 219u: goto st191;
		case 220u: goto st192;
		case 221u: goto st193;
		case 222u: goto st194;
		case 223u: goto st195;
		case 232u: goto st196;
		case 233u: goto st52;
		case 235u: goto st68;
		case 240u: goto st200;
		case 242u: goto st203;
		case 243u: goto st211;
		case 246u: goto st216;
		case 247u: goto st217;
		case 254u: goto st218;
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
						goto st68;
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
						goto st117;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st119;
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
		case 213u: goto tr463;
		case 229u: goto tr464;
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
		goto tr470;
	goto tr11;
tr470:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
    instruction_info_collected = 0;
  }
	goto st241;
st241:
	if ( ++( current_position) == ( end_of_bundle) )
		goto _test_eof241;
case 241:
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
		case 46u: goto st66;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st66;
		case 101u: goto st69;
		case 102u: goto st75;
		case 104u: goto st11;
		case 105u: goto st131;
		case 106u: goto st10;
		case 107u: goto st56;
		case 128u: goto st56;
		case 129u: goto st131;
		case 131u: goto st140;
		case 141u: goto st116;
		case 143u: goto st142;
		case 155u: goto tr415;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st154;
		case 198u: goto st186;
		case 199u: goto st187;
		case 201u: goto tr0;
		case 216u: goto st188;
		case 217u: goto st189;
		case 218u: goto st190;
		case 219u: goto st191;
		case 220u: goto st192;
		case 221u: goto st193;
		case 222u: goto st194;
		case 223u: goto st195;
		case 232u: goto st196;
		case 233u: goto st52;
		case 235u: goto st68;
		case 240u: goto st200;
		case 242u: goto st203;
		case 243u: goto st211;
		case 246u: goto st216;
		case 247u: goto st217;
		case 254u: goto st218;
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
						goto st68;
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
						goto st117;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st119;
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
		case 214u: goto tr463;
		case 230u: goto tr464;
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
		goto tr471;
	goto tr11;
tr471:
	{
    SET_IMM_TYPE(IMM8);
    SET_IMM_PTR(current_position);
  }
	{}
	{
    /* Mark start of this instruction as a valid target for jump.  */
    MarkValidJumpTarget(instruction_begin - data, valid_targets);

    /* Call user-supplied callback.  */
    instruction_end = current_position + 1;
    if ((instruction_info_collected & VALIDATION_ERRORS_MASK) ||
        (options & CALL_USER_CALLBACK_ON_EACH_INSTRUCTION)) {
      result &= user_callback(instruction_begin, instruction_end,
                              instruction_info_collected, callback_data);
    }

    /* On successful match the instruction_begin must point to the next byte
     * to be able to report the new offset as the start of instruction
     * causing error.  */
    instruction_begin = instruction_end;

    /* Clear variables (well, one variable currently).  */
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
		case 46u: goto st66;
		case 52u: goto st10;
		case 53u: goto st11;
		case 60u: goto st10;
		case 61u: goto st11;
		case 62u: goto st66;
		case 101u: goto st69;
		case 102u: goto st75;
		case 104u: goto st11;
		case 105u: goto st131;
		case 106u: goto st10;
		case 107u: goto st56;
		case 128u: goto st56;
		case 129u: goto st131;
		case 131u: goto st140;
		case 141u: goto st116;
		case 143u: goto st142;
		case 155u: goto tr415;
		case 168u: goto st10;
		case 169u: goto st11;
		case 196u: goto st154;
		case 198u: goto st186;
		case 199u: goto st187;
		case 201u: goto tr0;
		case 216u: goto st188;
		case 217u: goto st189;
		case 218u: goto st190;
		case 219u: goto st191;
		case 220u: goto st192;
		case 221u: goto st193;
		case 222u: goto st194;
		case 223u: goto st195;
		case 232u: goto st196;
		case 233u: goto st52;
		case 235u: goto st68;
		case 240u: goto st200;
		case 242u: goto st203;
		case 243u: goto st211;
		case 246u: goto st216;
		case 247u: goto st217;
		case 254u: goto st218;
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
						goto st68;
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
						goto st117;
				} else if ( (*( current_position)) >= 184u )
					goto st11;
			} else if ( (*( current_position)) > 211u ) {
				if ( (*( current_position)) > 249u ) {
					if ( 252u <= (*( current_position)) && (*( current_position)) <= 253u )
						goto tr0;
				} else if ( (*( current_position)) >= 244u )
					goto tr0;
			} else
				goto st119;
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
		case 215u: goto tr463;
		case 231u: goto tr464;
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
	_test_eof234: ( current_state) = 234; goto _test_eof; 
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
	_test_eof235: ( current_state) = 235; goto _test_eof; 
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
	_test_eof236: ( current_state) = 236; goto _test_eof; 
	_test_eof221: ( current_state) = 221; goto _test_eof; 
	_test_eof222: ( current_state) = 222; goto _test_eof; 
	_test_eof237: ( current_state) = 237; goto _test_eof; 
	_test_eof223: ( current_state) = 223; goto _test_eof; 
	_test_eof224: ( current_state) = 224; goto _test_eof; 
	_test_eof238: ( current_state) = 238; goto _test_eof; 
	_test_eof225: ( current_state) = 225; goto _test_eof; 
	_test_eof226: ( current_state) = 226; goto _test_eof; 
	_test_eof239: ( current_state) = 239; goto _test_eof; 
	_test_eof227: ( current_state) = 227; goto _test_eof; 
	_test_eof228: ( current_state) = 228; goto _test_eof; 
	_test_eof240: ( current_state) = 240; goto _test_eof; 
	_test_eof229: ( current_state) = 229; goto _test_eof; 
	_test_eof230: ( current_state) = 230; goto _test_eof; 
	_test_eof241: ( current_state) = 241; goto _test_eof; 
	_test_eof231: ( current_state) = 231; goto _test_eof; 
	_test_eof232: ( current_state) = 232; goto _test_eof; 
	_test_eof242: ( current_state) = 242; goto _test_eof; 
	_test_eof233: ( current_state) = 233; goto _test_eof; 

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
	{
    result &= user_callback(instruction_begin, current_position,
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
  if (jump_dests != &jump_dests_small) free(jump_dests);
  if (valid_targets != &valid_targets_small) free(valid_targets);
  if (!result) errno = EINVAL;
  return result;
}
