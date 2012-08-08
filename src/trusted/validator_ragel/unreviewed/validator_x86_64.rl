/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/validator_ragel/unreviewed/validator_internal.h"

#include "native_client/src/trusted/validator_ragel/gen/validator_x86_64_instruction_consts.c"

%%{
  machine x86_64_validator;
  alphtype unsigned char;
  variable p current_position;
  variable pe end_of_bundle;
  variable eof end_of_bundle;
  variable cs current_state;

  action check_access {
    check_access(instruction_start - data, base, index, restricted_register, valid_targets,
                 &errors_detected);
  }

  action rel8_operand {
    rel8_operand(current_position + 1, data, jump_dests, size,
                 &errors_detected);
  }
  action rel16_operand {
    #error rel16_operand should never be used in nacl
  }
  action rel32_operand {
    rel32_operand(current_position + 1, data, jump_dests, size,
                  &errors_detected);
  }

  # Do nothing when IMM operand is detected for now.  Will be used later for
  # dynamic code modification support.
  action imm2_operand { }
  action imm8_operand { }
  action imm16_operand { }
  action imm32_operand { }
  action imm64_operand { }
  action imm8_second_operand { }
  action imm16_second_operand { }
  action imm32_second_operand { }
  action imm64_second_operand { }

  action process_0_operands {
    process_0_operands(&restricted_register, &errors_detected);
  }
  action process_1_operands {
    process_1_operands(&restricted_register, &errors_detected, rex_prefix,
                       operand_states);
  }
  action process_2_operands {
    process_2_operands(&restricted_register, &errors_detected, rex_prefix,
                       operand_states);
  }

  include decode_x86_64 "validator_x86_64_instruction.rl";

  data16condrep = (data16 | condrep data16 | data16 condrep);
  data16rep = (data16 | rep data16 | data16 rep);

  # Special %rbp modifications without required sandboxing
  rbp_modifications =
    ((0x48 | 0x4a) 0x89 0xe5)               | # mov %rsp,%rbp
    ((0x48 | 0x4a) 0x8b 0xec)               # | mov %rsp,%rbp
    #(0x48 0x81 0xe5 any{3} (0x80 .. 0xff)) | # and $XXX,%rbp
    #(0x48 0x83 0xe5 (0x80 .. 0xff))          # and $XXX,%rbp
    @process_0_operands;

  # Special instructions used for %rbp sandboxing
  rbp_sandboxing =
    (0x4c 0x01 0xfd            | # add %r15,%rbp
     0x49 0x8d 0x2c 0x2f       | # lea (%r15,%rbp,1),%rbp
     0x4a 0x8d 0x6c 0x3d 0x00)   # lea 0x0(%rbp,%r15,1),%rbp
    @{ if (restricted_register != REG_RBP) {
         errors_detected |= UNRESTRICTED_RBP_PROCESSED;
       }
       restricted_register = NO_REG;
       BitmapClearBit(valid_targets, (instruction_start - data));
    };

  # Special %rbp modifications without required sandboxing
  rsp_modifications =
    ((0x48 | 0x4a) 0x89 0xec)              | # mov %rbp,%rsp
    ((0x48 | 0x4a) 0x8b 0xe5)              | # mov %rbp,%rsp
    (0x48 0x81 0xe4 any{3} (0x80 .. 0xff)) | # and $XXX,%rsp
    (0x48 0x83 0xe4 (0x80 .. 0xff))          # and $XXX,%rsp
    @process_0_operands;

  # Special instructions used for %rbp sandboxing
  rsp_sandboxing =
    (0x4c 0x01 0xfc       | # add %r15,%rsp
     0x4a 0x8d 0x24 0x3c)   # lea (%rsp,%r15,1),%rsp
    @{ if (restricted_register != REG_RSP) {
         errors_detected |= UNRESTRICTED_RSP_PROCESSED;
       }
       restricted_register = NO_REG;
       BitmapClearBit(valid_targets, (instruction_start - data));
    };

  # naclcall - old version (is not used by contemporary toolchain)
  # Note: first "and $~0x1f, %eXX" is normal instruction and as such will detect
  # case where %rbp/%rsp is illegally modified.
  old_naclcall_or_nacljmp =
    (((0x83 0xe0 0xe0 0x49 0x8d any 0x07 any{2}) | # and $~0x1f, %eax
      (0x83 0xe1 0xe0 0x49 0x8d any 0x0f any{2}) | #   lea (%r15,%rax),%rXX
      (0x83 0xe2 0xe0 0x49 0x8d any 0x17 any{2}) | # ...
      (0x83 0xe3 0xe0 0x49 0x8d any 0x1f any{2}) | # ...
      (0x83 0xe6 0xe0 0x49 0x8d any 0x37 any{2}) | # and $~0x1f, %edi
      (0x83 0xe7 0xe0 0x49 0x8d any 0x3f any{2}) | #   lea (%r15,%rdi),%rXX
      (0x41 0x83 0xe0 0xe0 0x49 0x8d any 0x07 any{2}) | # and $~0x1f, %r8d
      (0x41 0x83 0xe1 0xe0 0x49 0x8d any 0x0f any{2}) | #   lea (%r15,%rax),%rXX
      (0x41 0x83 0xe2 0xe0 0x49 0x8d any 0x17 any{2}) | #
      (0x41 0x83 0xe3 0xe0 0x49 0x8d any 0x1f any{2}) | # ...
      (0x41 0x83 0xe4 0xe0 0x49 0x8d any 0x27 any{2}) | #
      (0x41 0x83 0xe5 0xe0 0x49 0x8d any 0x2f any{2}) | # and $~0x1f, %r14d
      (0x41 0x83 0xe6 0xe0 0x49 0x8d any 0x37 any{2})) & #  lea (%r15,%rdi),%rXX
     ((any{3,4} 0x49 0x8d 0x04 any 0xff (0xd0|0xe0)) | # lea (%r15,%rXX),%rax
      (any{3,4} 0x49 0x8d 0x0c any 0xff (0xd1|0xe1)) | #   call/jmp %rax
      (any{3,4} 0x49 0x8d 0x14 any 0xff (0xd2|0xe2)) | # ...
      (any{3,4} 0x49 0x8d 0x1c any 0xff (0xd3|0xe3)) | # ...
      (any{3,4} 0x49 0x8d 0x34 any 0xff (0xd6|0xe6)) | # lea (%r15,%rXX),%rdi
      (any{3,4} 0x49 0x8d 0x3c any 0xff (0xd7|0xe7)))) #   call/jmp %rdi
    @{
       BitmapClearBit(valid_targets, (current_position - data) - 6);
       BitmapClearBit(valid_targets, (current_position - data) - 1);
       restricted_register = NO_REG;
    } |
    (((0x83 0xe0 0xe0 0x4d 0x8d any 0x07 any{3})  | # and $~0x1f, %eax
      (0x83 0xe1 0xe0 0x4d 0x8d any 0x0f any{3})  | #   lea (%r15,%rax),%rXX
      (0x83 0xe2 0xe0 0x4d 0x8d any 0x17 any{3})  | # ...
      (0x83 0xe3 0xe0 0x4d 0x8d any 0x1f any{3})  | # ...
      (0x83 0xe6 0xe0 0x4d 0x8d any 0x37 any{3})  | # and $~0x1f, %edi
      (0x83 0xe7 0xe0 0x4d 0x8d any 0x3f any{3})  | #   lea (%r15,%rdi),%rXX
      (0x41 0x83 0xe0 0xe0 0x4d 0x8d any 0x07 any{3}) | # and $~0x1f, %r8d
      (0x41 0x83 0xe1 0xe0 0x4d 0x8d any 0x0f any{3}) | #   lea (%r15,%rax),%rXX
      (0x41 0x83 0xe2 0xe0 0x4d 0x8d any 0x17 any{3}) | #
      (0x41 0x83 0xe3 0xe0 0x4d 0x8d any 0x1f any{3}) | # ...
      (0x41 0x83 0xe4 0xe0 0x4d 0x8d any 0x27 any{3}) | #
      (0x41 0x83 0xe5 0xe0 0x4d 0x8d any 0x2f any{3}) | # and $~0x1f, %r14d
      (0x41 0x83 0xe6 0xe0 0x4d 0x8d any 0x37 any{3})) & #  lea (%r15,%rdi),%rXX
     ((any{3,4} 0x4d 0x8d 0x04 any 0x41 0xff (0xd0|0xe0)) | # lea (%r15,%rXX),
      (any{3,4} 0x4d 0x8d 0x0c any 0x41 0xff (0xd1|0xe1)) | #                %r8
      (any{3,4} 0x4d 0x8d 0x14 any 0x41 0xff (0xd2|0xe2)) | #   call/jmp %r8
      (any{3,4} 0x4d 0x8d 0x1c any 0x41 0xff (0xd3|0xe3)) | # ...
      (any{3,4} 0x4d 0x8d 0x24 any 0x41 0xff (0xd4|0xe4)) | # lea (%r15,%rXX),
      (any{3,4} 0x4d 0x8d 0x2c any 0x41 0xff (0xd5|0xe5)) | #               %r14
      (any{3,4} 0x4d 0x8d 0x34 any 0x41 0xff (0xd6|0xe6)))) #   call/jmp %r14
    @{
       BitmapClearBit(valid_targets, (current_position - data) - 7);
       BitmapClearBit(valid_targets, (current_position - data) - 2);
       restricted_register = NO_REG;
    };

  # naclcall or nacljmp - this the form used by contemporary toolchain
  # Note: first "and $~0x1f, %eXX" is normal instruction and as such will detect
  # case where %rbp/%rsp is illegally modified.
  naclcall_or_nacljmp =
    (0x83 0xe0 0xe0 0x4c 0x01 0xf8 0xff (0xd0|0xe0) | # naclcall/jmp %eax, %r15
     0x83 0xe1 0xe0 0x4c 0x01 0xf9 0xff (0xd1|0xe1) | # naclcall/jmp %ecx, %r15
     0x83 0xe2 0xe0 0x4c 0x01 0xfa 0xff (0xd2|0xe2) | # naclcall/jmp %edx, %r15
     0x83 0xe3 0xe0 0x4c 0x01 0xfb 0xff (0xd3|0xe3) | # naclcall/jmp %ebx, %r15
     0x83 0xe6 0xe0 0x4c 0x01 0xfe 0xff (0xd6|0xe6) | # naclcall/jmp %esi, %r15
     0x83 0xe7 0xe0 0x4c 0x01 0xff 0xff (0xd7|0xe7))  # naclcall/jmp %edi, %r15
    @{
       BitmapClearBit(valid_targets, (current_position - data) - 4);
       BitmapClearBit(valid_targets, (current_position - data) - 1);
       restricted_register = NO_REG;
    } |
    (0x41 0x83 0xe0 0xe0 0x4d 0x01 0xf8 0x41 0xff (0xd0|0xe0) | # naclcall/jmp
     0x41 0x83 0xe1 0xe0 0x4d 0x01 0xf9 0x41 0xff (0xd1|0xe1) | #    %r8d, %r15
     0x41 0x83 0xe2 0xe0 0x4d 0x01 0xfa 0x41 0xff (0xd2|0xe2) | #
     0x41 0x83 0xe3 0xe0 0x4d 0x01 0xfb 0x41 0xff (0xd3|0xe3) | # ...
     0x41 0x83 0xe4 0xe0 0x4d 0x01 0xfc 0x41 0xff (0xd4|0xe4) | #
     0x41 0x83 0xe5 0xe0 0x4d 0x01 0xfd 0x41 0xff (0xd5|0xe5) | # naclcall/jmp
     0x41 0x83 0xe6 0xe0 0x4d 0x01 0xfe 0x41 0xff (0xd6|0xe6))  #   %r14d, %r15
    @{
       BitmapClearBit(valid_targets, (current_position - data) - 5);
       BitmapClearBit(valid_targets, (current_position - data) - 2);
       restricted_register = NO_REG;
    };

  # EMMS/SSE2/AVX instructions which have implicit %ds:(%rsi) operand
  mmx_sse_rdi_instructions =
    (REX_WRXB? (0x0f 0xf7)
      @CPUFeature_EMMX modrm_registers | # maskmovq %mmX,%mmY
     0x66 REX_WRXB? (0x0f 0xf7) @not_data16_prefix
      @CPUFeature_SSE2 modrm_registers | # maskmovdqu %xmmX, %xmmY
     ((0xc4 (VEX_RB & VEX_map00001) 0x79 @vex_prefix3) |
      (0xc5 (0x79 | 0xf9) @vex_prefix_short)) 0xf7
      @CPUFeature_AVX modrm_registers);  # vmaskmovdqu %xmmX, %xmmY

  # String instructions which use only %ds:(%rsi)
  string_instructions_rsi_no_rdi =
    (rep? 0xac                 | # lods   %ds:(%rsi),%al
     data16rep 0xad            | # lods   %ds:(%rsi),%ax
     rep? REXW_NONE? 0xad)     ; # lods   %ds:(%rsi),%eax/%rax

  # String instructions which use only %ds:(%rdi)
  string_instructions_rdi_no_rsi =
    condrep? 0xae             | # scas   %es:(%rdi),%al
    data16condrep 0xaf        | # scas   %es:(%rdi),%ax
    condrep? REXW_NONE? 0xaf  | # scas   %es:(%rdi),%eax/%rax

    rep? 0xaa                 | # stos   %al,%es:(%rdi)
    data16rep 0xab            | # stos   %ax,%es:(%rdi)
    rep? REXW_NONE? 0xab      ; # stos   %eax/%rax,%es:(%rdi)

  # String instructions which use both %ds:(%rsi) and %ds:(%rdi)
  string_instructions_rsi_rdi =
    condrep? 0xa6            | # cmpsb    %es:(%rdi),%ds:(%rsi)
    data16condrep 0xa7       | # cmpsw    %es:(%rdi),%ds:(%rsi)
    condrep? REXW_NONE? 0xa7 | # cmps[lq] %es:(%rdi),%ds:(%rsi)

    rep? 0xa4                | # movsb    %es:(%rdi),%ds:(%rsi)
    data16rep 0xa5           | # movsw    %es:(%rdi),%ds:(%rsi)
    rep? REXW_NONE? 0xa5     ; # movs[lq] %es:(%rdi),%ds:(%rsi)

  sandbox_string_instructions_rsi_no_rdi =
    (0x89 | 0x8b) 0xf6         . # mov %esi,%esi
    0x49 0x8d 0x34 0x37        . # lea (%r15,%rsi,1),%rsi
    string_instructions_rsi_no_rdi
    @{
       BitmapClearBit(valid_targets, (instruction_start - data));
       BitmapClearBit(valid_targets, (instruction_start - 4 - data));
       restricted_register = NO_REG;
    };

  sandbox_string_instructions_rdi_no_rsi =
    (0x89 | 0x8b) 0xff         . # mov %edi,%edi
    0x49 0x8d 0x3c 0x3f        . # lea (%r15,%rdi,1),%rdi
    string_instructions_rdi_no_rsi
    @{
       BitmapClearBit(valid_targets, (instruction_start - data));
       BitmapClearBit(valid_targets, (instruction_start - 4 - data));
       restricted_register = NO_REG;
    };

  # String instructions which use both %ds:(%rsi) and %ds:(%rdi)
  sandbox_string_instructions_rsi_rdi =
    (0x89 | 0x8b) 0xf6        . # mov %esi,%esi
    0x49 0x8d 0x34 0x37       . # lea (%r15,%rsi,1),%rsi
    (0x89 | 0x8b) 0xff        . # mov %edi,%edi
    0x49 0x8d 0x3c 0x3f       . # lea (%r15,%rdi,1),%rdi
    string_instructions_rsi_rdi
    @{
       BitmapClearBit(valid_targets, (instruction_start - data));
       BitmapClearBit(valid_targets, (instruction_start - 4 - data));
       BitmapClearBit(valid_targets, (instruction_start - 6 - data));
       BitmapClearBit(valid_targets, (instruction_start - 10 - data));
       restricted_register = NO_REG;
    };

  special_instruction =
    rbp_modifications |
    rsp_modifications |
    rbp_sandboxing |
    rsp_sandboxing |
    old_naclcall_or_nacljmp |
    naclcall_or_nacljmp |
    sandbox_string_instructions_rsi_no_rdi |
    sandbox_string_instructions_rdi_no_rsi |
    sandbox_string_instructions_rsi_rdi;

  # Remove special instructions which are only allowed in special cases.
  normal_instruction = one_instruction - special_instruction;

  main := ((normal_instruction | special_instruction)
     >{
       BitmapSetBit(valid_targets, current_position - data);
     }
     @{
       if (errors_detected) {
         process_error(instruction_start, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       instruction_start = current_position + 1;
       errors_detected = 0;
       SET_REX_PREFIX(FALSE);
       SET_VEX_PREFIX2(0xe0);
       SET_VEX_PREFIX3(0x00);
       operand_states = 0;
     })*
    $err{
        process_error(instruction_start, UNRECOGNIZED_INSTRUCTION, userdata);
        result = 1;
        goto error_detected;
    };

}%%

%% write data;

int ValidateChunkAMD64(const uint8_t *data, size_t size,
                       const NaClCPUFeaturesX86 *cpu_features,
                       process_validation_error_func process_error,
                       void *userdata) {
  uint8_t *valid_targets = BitmapAllocate(size);
  uint8_t *jump_dests = BitmapAllocate(size);

  const uint8_t *current_position = data;

  uint8_t rex_prefix = FALSE;
  uint8_t vex_prefix2 = 0xe0;
  uint8_t vex_prefix3 = 0x00;
  /* Keeps one byte of information per operand in the current instruction:
   *  2 bits for register kinds,
   *  5 bits for register numbers (16 regs plus RIZ). */
  uint32_t operand_states = 0;
  enum register_name base = NO_REG;
  enum register_name index = NO_REG;
  int result = 0;

  size_t i;

  uint32_t errors_detected = 0;

  assert(size % kBundleSize == 0);

  if (!valid_targets || !jump_dests) goto error_detected;

  while (current_position < data + size) {
    /* Start of the instruction being processed.  */
    const uint8_t *instruction_start = current_position;
    const uint8_t *end_of_bundle = current_position + kBundleSize;
    enum register_name restricted_register = NO_REG;
    int current_state;

    %% write init;
    %% write exec;

    if (restricted_register == REG_RBP) {
      process_error(instruction_start, RESTRICTED_RBP_UNPROCESSED, userdata);
      result = 1;
    } else if (restricted_register == REG_RSP) {
      process_error(instruction_start, RESTRICTED_RSP_UNPROCESSED, userdata);
      result = 1;
    }
  }

  for (i = 0; i < size / 32; i++) {
    uint32_t jump_dest_mask = ((uint32_t *) jump_dests)[i];
    uint32_t valid_target_mask = ((uint32_t *) valid_targets)[i];
    if ((jump_dest_mask & ~valid_target_mask) != 0) {
      process_error(data + i * 32, BAD_JUMP_TARGET, userdata);
      result = 1;
      break;
    }
  }

error_detected:
  free(jump_dests);
  free(valid_targets);
  return result;
}
