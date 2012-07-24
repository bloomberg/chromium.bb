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
  machine x86_64_decoder;
  alphtype unsigned char;

  action check_access {
    check_access(begin - data, base, index, restricted_register, valid_targets,
                 &errors_detected);
  }

  action rel8_operand {
    rel8_operand(p + 1, data, jump_dests, size, &errors_detected);
  }
  action rel16_operand {
    #error rel16_operand should never be used in nacl
  }
  action rel32_operand {
    rel32_operand(p + 1, data, jump_dests, size, &errors_detected);
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
                       operand_states, begin, &sandboxed_rsi_restricted_rdi);
  }
  action process_2_operands {
    process_2_operands(&restricted_register, &errors_detected, rex_prefix,
                       operand_states, begin, &sandboxed_rsi_restricted_rdi);
  }

  include decode_x86_64 "validator_x86_64_instruction.rl";

  data16condrep = (data16 | condrep data16 | data16 condrep);
  data16rep = (data16 | rep data16 | data16 rep);

  # Special %rbp modifications without required sandboxing
  rbp_modifications =
    (0x48 0x89 0xe5)                       | # mov %rsp,%rbp
    (0x48 0x81 0xe5 any{3} (0x80 .. 0xff)) | # and $XXX,%rbp
    (0x48 0x83 0xe5 (0x80 .. 0xff))          # and $XXX,%rbp
    @process_0_operands;

  # Special instructions used for %rbp sandboxing
  rbp_sandboxing =
    (0x4c 0x01 0xfd            | # add %r15,%rbp
     0x49 0x8d 0x2c 0x2f       | # lea (%r15,%rbp,1),%rbp
     0x4a 0x8d 0x6c 0x3d 0x00)   # lea 0x0(%rbp,%r15,1),%rbp
    @{ if (restricted_register != REG_RBP) {
         errors_detected |= RESTRICTED_RBP_UNPROCESSED;
       }
       restricted_register = kNoRestrictedReg;
       BitmapClearBit(valid_targets, (begin - data));
    };

  # Special %rbp modifications without required sandboxing
  rsp_modifications =
    (0x48 0x89 0xec)                       | # mov %rbp,%rsp
    (0x48 0x81 0xe4 any{3} (0x80 .. 0xff)) | # and $XXX,%rsp
    (0x48 0x83 0xe4 (0x80 .. 0xff))          # and $XXX,%rsp
    @process_0_operands;

  # Special instructions used for %rbp sandboxing
  rsp_sandboxing =
    (0x4c 0x01 0xfc       | # add %r15,%rsp
     0x4a 0x8d 0x24 0x3c)   # lea (%rsp,%r15,1),%rsp
    @{ if (restricted_register != REG_RSP) {
         errors_detected |= RESTRICTED_RSP_UNPROCESSED;
       }
       restricted_register = kNoRestrictedReg;
       BitmapClearBit(valid_targets, (begin - data));
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
       BitmapClearBit(valid_targets, (p - data) - 6);
       BitmapClearBit(valid_targets, (p - data) - 1);
       restricted_register = kNoRestrictedReg;
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
       BitmapClearBit(valid_targets, (p - data) - 7);
       BitmapClearBit(valid_targets, (p - data) - 2);
       restricted_register = kNoRestrictedReg;
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
       BitmapClearBit(valid_targets, (p - data) - 4);
       BitmapClearBit(valid_targets, (p - data) - 1);
       restricted_register = kNoRestrictedReg;
    } |
    (0x41 0x83 0xe0 0xe0 0x4d 0x01 0xf8 0x41 0xff (0xd0|0xe0) | # naclcall/jmp
     0x41 0x83 0xe1 0xe0 0x4d 0x01 0xf9 0x41 0xff (0xd1|0xe1) | #    %r8d, %r15
     0x41 0x83 0xe2 0xe0 0x4d 0x01 0xfa 0x41 0xff (0xd2|0xe2) | #
     0x41 0x83 0xe3 0xe0 0x4d 0x01 0xfb 0x41 0xff (0xd3|0xe3) | # ...
     0x41 0x83 0xe4 0xe0 0x4d 0x01 0xfc 0x41 0xff (0xd4|0xe4) | #
     0x41 0x83 0xe5 0xe0 0x4d 0x01 0xfd 0x41 0xff (0xd5|0xe5) | # naclcall/jmp
     0x41 0x83 0xe6 0xe0 0x4d 0x01 0xfe 0x41 0xff (0xd6|0xe6))  #   %r14d, %r15
    @{
       BitmapClearBit(valid_targets, (p - data) - 5);
       BitmapClearBit(valid_targets, (p - data) - 2);
       restricted_register = kNoRestrictedReg;
    };

  # Special %rsi sandboxing for string instructions
  rsi_sandboxing =
    (0x49 0x8d 0x34 0x37) # lea (%r15,%rsi,1),%rsi
    @{ if (restricted_register == REG_RSI) {
         sandboxed_rsi = begin;
         restricted_register = kSandboxedRsi;
       } else {
         restricted_register = kNoRestrictedReg;
       }
    };

  # Special %rdi sandboxing for string instructions
  rdi_sandboxing =
    (0x49 0x8d 0x3c 0x3f) # lea (%r15,%rdi,1),%rdi
    @{ if (restricted_register == REG_RDI) {
         sandboxed_rdi = begin;
         restricted_register = kSandboxedRdi;
       } else if (restricted_register == kSandboxedRsiRestrictedRdi) {
         sandboxed_rdi = begin;
         restricted_register = kSandboxedRsiSandboxedRdi;
       } else {
         restricted_register = kNoRestrictedReg;
       }
    };

  # String instructions which use only %ds:(%rsi)
  string_instructions_rsi_no_rdi =
    (rep? 0xac                 | # lods   %ds:(%rsi),%al
     data16rep 0xad            | # lods   %ds:(%rsi),%ax
     rep? REXW_NONE? 0xad)       # lods   %ds:(%rsi),%eax/%rax
    @{ if (restricted_register != kSandboxedRsi) {
         errors_detected |= RSI_UNSANDBOXDED;
       } else {
         BitmapClearBit(valid_targets, (begin - data));
         BitmapClearBit(valid_targets, (sandboxed_rdi - data));
       }
       restricted_register = kNoRestrictedReg;
    };

  # String instructions which use only %ds:(%rdi)
  string_instructions_rdi_no_rsi =
    (condrep? 0xae             | # scas   %es:(%rdi),%al
     data16condrep 0xaf        | # scas   %es:(%rdi),%ax
     condrep? REXW_NONE? 0xaf  | # scas   %es:(%rdi),%eax/%rax

     rep? 0xaa                 | # stos   %al,%es:(%rdi)
     data16rep 0xab            | # stos   %ax,%es:(%rdi)
     rep? REXW_NONE? 0xab)       # stos   %eax/%rax,%es:(%rdi)
    @{ if (restricted_register != kSandboxedRdi &&
           restricted_register != kSandboxedRsiSandboxedRdi) {
         errors_detected |= RDI_UNSANDBOXDED;
       } else {
         BitmapClearBit(valid_targets, (begin - data));
         BitmapClearBit(valid_targets, (sandboxed_rdi - data));
       }
       restricted_register = kNoRestrictedReg;
    };

  # String instructions which use both %ds:(%rsi) and %ds:(%rdi)
  string_instructions_rsi_rdi =
    (condrep? 0xa6            | # cmpsb    %es:(%rdi),%ds:(%rsi)
     data16condrep 0xa7       | # cmpsw    %es:(%rdi),%ds:(%rsi)
     condrep? REXW_NONE? 0xa7 | # cmps[lq] %es:(%rdi),%ds:(%rsi)

     rep? 0xa4                | # movsb    %es:(%rdi),%ds:(%rsi)
     data16rep 0xa5           | # movsw    %es:(%rdi),%ds:(%rsi)
     rep? REXW_NONE? 0xa5)      # movs[lq] %es:(%rdi),%ds:(%rsi)
    @{ if (restricted_register != kSandboxedRsiSandboxedRdi) {
         errors_detected |= RDI_UNSANDBOXDED;
         if (restricted_register != kSandboxedRsi) {
           errors_detected |= RSI_UNSANDBOXDED;
         }
       } else {
         BitmapClearBit(valid_targets, (begin - data));
         BitmapClearBit(valid_targets, (sandboxed_rsi - data));
         BitmapClearBit(valid_targets, (sandboxed_rsi_restricted_rdi - data));
         BitmapClearBit(valid_targets, (sandboxed_rdi - data));
       }
       restricted_register = kNoRestrictedReg;
    };

  special_instruction =
    rbp_modifications |
    rsp_modifications |
    rbp_sandboxing |
    rsp_sandboxing |
    old_naclcall_or_nacljmp |
    naclcall_or_nacljmp |
    rsi_sandboxing |
    rdi_sandboxing |
    string_instructions_rsi_no_rdi |
    string_instructions_rdi_no_rsi |
    string_instructions_rsi_rdi;

  # Remove special instructions which are only allowed in special cases.
  normal_instruction = one_instruction - special_instruction;

  main := ((normal_instruction | special_instruction) >{
        begin = p;
        errors_detected = 0;
        BitmapSetBit(valid_targets, p - data);
        SET_REX_PREFIX(FALSE);
        SET_VEX_PREFIX2(0xe0);
        SET_VEX_PREFIX3(0x00);
        operand_states = 0;
     }
     @{
       if (errors_detected) {
         process_error(begin, errors_detected, userdata);
         result = 1;
       }
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     })*
    $err{
        process_error(begin, UNRECOGNIZED_INSTRUCTION, userdata);
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

  const uint8_t *p = data;
  const uint8_t *begin = p;  /* Start of the instruction being processed.  */

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
  /* These are borders of the appropriate instructions.  Initialize them to make
   * compiler happy: they are never used uninitialized even without explicit
   * initialization but GCC is not sophysicated enough to prove that.  */
  const uint8_t *sandboxed_rsi = 0;
  const uint8_t *sandboxed_rsi_restricted_rdi = 0;
  const uint8_t *sandboxed_rdi = 0;

  size_t i;

  uint32_t errors_detected = 0;

  assert(size % kBundleSize == 0);

  while (p < data + size) {
    const uint8_t *pe = p + kBundleSize;
    const uint8_t *eof = pe;
    int cs;
    uint8_t restricted_register = kNoRestrictedReg;

    %% write init;
    %% write exec;

    if (restricted_register == REG_RBP) {
      process_error(begin, RESTRICTED_RBP_UNPROCESSED, userdata);
      result = 1;
    } else if (restricted_register == REG_RSP) {
      process_error(begin, RESTRICTED_RSP_UNPROCESSED, userdata);
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
  return result;
}
