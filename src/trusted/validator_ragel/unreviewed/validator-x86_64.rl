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

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator.h"

#if defined(_MSC_VER)
#define inline __inline
#endif

#include "native_client/src/trusted/validator_ragel/gen/validator-x86_64-instruction-consts.c"

#define check_jump_dest \
    if ((jump_dest & bundle_mask) != bundle_mask) { \
      if (jump_dest >= size) { \
        printf("direct jump out of range: %"NACL_PRIxS"\n", jump_dest); \
        result = 1; \
        goto error_detected; \
      } else { \
        BitmapSetBit(jump_dests, jump_dest + 1); \
      } \
    } \
    SET_OPERAND_NAME(0, JMP_TO); \
    SET_MODRM_BASE(REG_RIP); \
    SET_MODRM_INDEX(NO_REG);

static void PrintError(const char* msg, uintptr_t ptr) {
  printf("offset 0x%"NACL_PRIxS": %s", ptr, msg);
}

%%{
  machine x86_64_decoder;
  alphtype unsigned char;

  action check_access {
    if ((base == REG_RIP) || (base == REG_R15) ||
        ((base == REG_RSP) && (restricted_register != REG_RSP)) ||
        ((base == REG_RBP) && (restricted_register != REG_RBP))) {
      if ((index == restricted_register) ||
          ((index == REG_RDI) &&
           (restricted_register == kSandboxedRsiRestrictedRdi))) {
        BitmapClearBit(valid_targets, begin - data);
      } else if ((index != NO_REG) && (index != REG_RIZ)) {
        PrintError("Improper sandboxing in instruction\n", begin - data);
        result = 1;
        goto error_detected;
      }
    } else if ((index == REG_RIP) || (index == REG_R15) ||
               ((index == REG_RSP) && (restricted_register != REG_RSP)) ||
               ((index == REG_RBP) && (restricted_register != REG_RBP))) {
      if ((base == restricted_register) ||
          ((base == REG_RDI) &&
           (restricted_register == kSandboxedRsiRestrictedRdi))) {
        BitmapClearBit(valid_targets, begin - data);
      } else if ((base != NO_REG) && (base != REG_RIZ)) {
        PrintError("Improper sandboxing in instruction\n", begin - data);
        result = 1;
        goto error_detected;
      }
    } else {
      PrintError("Improper sandboxing in instruction\n", begin - data);
      result = 1;
      goto error_detected;
    }
  }

  action rel8_operand {
    int8_t offset = (uint8_t) (p[0]);
    size_t jump_dest = offset + (p - data);
    check_jump_dest;
  }
  action rel16_operand {
    assert(FALSE);
  }
  action rel32_operand {
    int32_t offset =
           (uint32_t) (p[-3] + 256U * (p[-2] + 256U * (p[-1] + 256U * (p[0]))));
    size_t jump_dest = offset + (p - data);
    check_jump_dest;
  }

  action process_0_operands {
    /* Restricted %rsp or %rbp must be processed by appropriate nacl-special
     * instruction, not with regular instruction.  */
    if (restricted_register == REG_RSP) {
      PrintError("Incorrectly modified register %%rsp\n", begin - data);
      result = 1;
      goto error_detected;
    } else if (restricted_register == REG_RBP) {
      PrintError("Incorrectly modified register %%rbp\n", begin - data);
      result = 1;
      goto error_detected;
    }
    restricted_register = kNoRestrictedReg;
  }

  action process_1_operands {
    /* Restricted %rsp or %rbp must be processed by appropriate nacl-special
     * instruction, not with regular instruction.  */
    if (restricted_register == REG_RSP) {
      PrintError("Incorrectly modified register %%rsp\n", begin - data);
      result = 1;
      goto error_detected;
    } else if (restricted_register == REG_RBP) {
      PrintError("Incorrectly modified register %%rbp\n", begin - data);
      result = 1;
      goto error_detected;
    }
    /* If Sandboxed Rsi is destroyed then we must detect that.  */
    if (restricted_register == kSandboxedRsi) {
      if (CHECK_OPERAND(0, REG_RSI, OperandSandboxRestricted) ||
          CHECK_OPERAND(0, REG_RSI, OperandSandboxUnrestricted)) {
        restricted_register = kNoRestrictedReg;
      }
    }
    if (restricted_register == kSandboxedRsi) {
      if (CHECK_OPERAND(0, REG_RDI, OperandSandboxRestricted)) {
        sandboxed_rsi_restricted_rdi = begin;
        restricted_register = kSandboxedRsiRestrictedRdi;
      }
    }
    if (restricted_register != kSandboxedRsiRestrictedRdi) {
      restricted_register = kNoRestrictedReg;
      if (CHECK_OPERAND(0, REG_R15, OperandSandbox8bit) ||
          CHECK_OPERAND(0, REG_R15, OperandSandboxRestricted) ||
          CHECK_OPERAND(0, REG_R15, OperandSandboxUnrestricted)) {
        PrintError("Incorrectly modified register %%r15\n", begin - data);
        result = 1;
        goto error_detected;
      } else if ((CHECK_OPERAND(0, REG_RBP, OperandSandbox8bit) &&
                  GET_REX_PREFIX()) ||
                 CHECK_OPERAND(0, REG_RBP, OperandSandboxUnrestricted)) {
        PrintError("Incorrectly modified register %%rbp\n", begin - data);
        result = 1;
        goto error_detected;
      } else if ((CHECK_OPERAND(0, REG_RSP, OperandSandbox8bit) &&
                  GET_REX_PREFIX()) ||
                 CHECK_OPERAND(0, REG_RSP, OperandSandboxUnrestricted)) {
        PrintError("Incorrectly modified register %%rsp\n", begin - data);
        result = 1;
        goto error_detected;
      /* Take 2 bits of operand type from operand_states as restricted_register,
       * make sure operand_states denotes a register (4th bit == 0). */
      } else if ((operand_states & 0x70) == (OperandSandboxRestricted << 5)) {
        restricted_register = operand_states & 0x0f;
      }
    }
  }

  action process_2_operands {
    /* Restricted %rsp or %rbp must be processed by appropriate nacl-special
     * instruction, not with regular instruction.  */
    if (restricted_register == REG_RSP) {
      PrintError("Incorrectly modified register %%rsp\n", begin - data);
      result = 1;
      goto error_detected;
    } else if (restricted_register == REG_RBP) {
      PrintError("Incorrectly modified register %%rbp\n", begin - data);
      result = 1;
      goto error_detected;
    }
    /* If Sandboxed Rsi is destroyed then we must detect that.  */
    if (restricted_register == kSandboxedRsi) {
      if (CHECK_OPERAND(0, REG_RSI, OperandSandboxRestricted) ||
          CHECK_OPERAND(0, REG_RSI, OperandSandboxUnrestricted) ||
          CHECK_OPERAND(1, REG_RSI, OperandSandboxRestricted) ||
          CHECK_OPERAND(1, REG_RSI, OperandSandboxUnrestricted)) {
        restricted_register = kNoRestrictedReg;
      }
    }
    if (restricted_register == kSandboxedRsi) {
      if (CHECK_OPERAND(0, REG_RDI, OperandSandboxRestricted) ||
          CHECK_OPERAND(1, REG_RDI, OperandSandboxRestricted)) {
        sandboxed_rsi_restricted_rdi = begin;
        restricted_register = kSandboxedRsiRestrictedRdi;
      }
    }
    if (restricted_register != kSandboxedRsiRestrictedRdi) {
      restricted_register = kNoRestrictedReg;
      if (CHECK_OPERAND(0, REG_R15, OperandSandbox8bit) ||
          CHECK_OPERAND(0, REG_R15, OperandSandboxRestricted) ||
          CHECK_OPERAND(0, REG_R15, OperandSandboxUnrestricted) ||
          CHECK_OPERAND(1, REG_R15, OperandSandbox8bit) ||
          CHECK_OPERAND(1, REG_R15, OperandSandboxRestricted) ||
          CHECK_OPERAND(1, REG_R15, OperandSandboxUnrestricted)) {
        PrintError("Incorrectly modified register %%r15\n", begin - data);
        result = 1;
        goto error_detected;
      } else if ((CHECK_OPERAND(0, REG_RBP, OperandSandbox8bit) &&
                  GET_REX_PREFIX()) ||
                 CHECK_OPERAND(0, REG_RBP, OperandSandboxUnrestricted) ||
                 (CHECK_OPERAND(1, REG_RBP, OperandSandbox8bit) &&
                  GET_REX_PREFIX()) ||
                 CHECK_OPERAND(1, REG_RBP, OperandSandboxUnrestricted)) {
        PrintError("Incorrectly modified register %%rbp\n", begin - data);
        result = 1;
        goto error_detected;
      } else if ((CHECK_OPERAND(0, REG_RSP, OperandSandbox8bit) &&
                  GET_REX_PREFIX()) ||
                 CHECK_OPERAND(0, REG_RSP, OperandSandboxUnrestricted) ||
                 (CHECK_OPERAND(1, REG_RSP, OperandSandbox8bit) &&
                  GET_REX_PREFIX()) ||
                 CHECK_OPERAND(1, REG_RSP, OperandSandboxUnrestricted)) {
        PrintError("Incorrectly modified register %%rsp\n", begin - data);
        result = 1;
        goto error_detected;
      /* Take 2 bits of operand type from operand_states as restricted_register,
       * make sure operand_states denotes a register (4th bit == 0).  */
      } else if ((operand_states & 0x70) == (OperandSandboxRestricted << 5)) {
        restricted_register = operand_states & 0x0f;
      /* Take 2 bits of operand type from operand_states as restricted_register,
       * make sure operand_states denotes a register (12th bit == 0).  */
      } else if ((operand_states & 0x7000) ==
          (OperandSandboxRestricted << (5 + 8))) {
        restricted_register = (operand_states & 0x0f00) >> 8;
      }
    }
  }

  include decode_x86_64 "validator-x86_64-instruction.rl";

  # Remove special instructions which are only allowed in special cases.
  normal_instruction = one_instruction - (
    (0x48 0x89 0xe5)             | # mov %rsp,%rbp
    (0x48 0x89 0xec)             | # mov %rbp,%rsp
    (0x48 0x81 0xe4 any{4})      | # and $XXX,%rsp
    (0x48 0x83 0xe4 any)         | # and $XXX,%rsp
    (0x4c 0x01 0xfd)             | # add %r15,%rbp
    (0x49 0x8d 0x2c 0x2f)        | # lea (%r15,%rbp,1),%rbp
    (0x4a 0x8d 0x6c 0x3d any)    | # lea 0x0(%rbp,%r15,1),%rbp
    (0x48 0x81 0xe5 any{4})      | # and $XXX,%rsp
    (0x48 0x83 0xe5 any)         | # and $XXX,%rsp
    (0x4c 0x01 0xfc)             | # add %r15,%rsp
    (0x4a 0x8d 0x24 0x3c)        | # lea (%rsp,%r15,1),%rsp
    (0x49 0x8d 0x34 0x37)        | # lea (%r15,%rsi,1),%rsi
    (0x49 0x8d 0x3c 0x3f)          # lea (%r15,%rdi,1),%rdi
  );

  data16condrep = (data16 | condrep data16 | data16 condrep);
  data16rep = (data16 | rep data16 | data16 rep);

  special_instruction =
    (0x48 0x89 0xe5)                       | # mov %rsp,%rbp
    (0x48 0x81 0xe4 any{3} (0x80 .. 0xff)) | # and $XXX,%rsp
    (0x48 0x83 0xe4 (0x80 .. 0xff))          # and $XXX,%rsp
    @{ if (restricted_register == REG_RSP) {
         PrintError("Incorrectly modified register %%rsp\n", begin - data);
         result = 1;
         goto error_detected;
       }
       restricted_register = kNoRestrictedReg;
    } |
    (0x48 0x89 0xec)                       | # mov %rbp,%rsp
    (0x48 0x81 0xe5 any{3} (0x80 .. 0xff)) | # and $XXX,%rsp
    (0x48 0x83 0xe5 (0x80 .. 0xff))          # and $XXX,%rsp
    @{ if (restricted_register == REG_RBP) {
         PrintError("Incorrectly modified register %%rbp\n", begin - data);
         result = 1;
         goto error_detected;
       }
       restricted_register = kNoRestrictedReg;
    } |
    (0x4c 0x01 0xfd            | # add %r15,%rbp
     0x49 0x8d 0x2c 0x2f       | # lea (%r15,%rbp,1),%rbp
     0x4a 0x8d 0x6c 0x3d 0x00)   # lea 0x0(%rbp,%r15,1),%rbp
    @{ if (restricted_register != REG_RBP) {
         PrintError("Incorrectly sandboxed %%rbp\n", begin - data);
         result = 1;
         goto error_detected;
       }
       restricted_register = kNoRestrictedReg;
       BitmapClearBit(valid_targets, (begin - data));
    } |
    (0x4c 0x01 0xfc       | # add %r15,%rsp
     0x4a 0x8d 0x24 0x3c)   # lea (%rsp,%r15,1),%rsp
    @{ if (restricted_register != REG_RSP) {
         PrintError("Incorrectly sandboxed %%rsp\n", begin - data);
         result = 1;
         goto error_detected;
       }
       restricted_register = kNoRestrictedReg;
       BitmapClearBit(valid_targets, (begin - data));
    } |
    (0x83 0xe0 0xe0 0x4c 0x01 0xf8 0xff (0xd0|0xe0) | # naclcall/jmp %eax, %r15
     0x83 0xe1 0xe0 0x4c 0x01 0xf9 0xff (0xd1|0xe1) | # naclcall/jmp %ecx, %r15
     0x83 0xe2 0xe0 0x4c 0x01 0xfa 0xff (0xd2|0xe2) | # naclcall/jmp %edx, %r15
     0x83 0xe3 0xe0 0x4c 0x01 0xfb 0xff (0xd3|0xe3) | # naclcall/jmp %ebx, %r15
     0x83 0xe4 0xe0 0x4c 0x01 0xfc 0xff (0xd4|0xe4) | # naclcall/jmp %esp, %r15
     0x83 0xe5 0xe0 0x4c 0x01 0xfd 0xff (0xd5|0xe5) | # naclcall/jmp %ebp, %r15
     0x83 0xe6 0xe0 0x4c 0x01 0xfe 0xff (0xd6|0xe6) | # naclcall/jmp %esi, %r15
     0x83 0xe7 0xe0 0x4c 0x01 0xff 0xff (0xd7|0xe7))  # naclcall/jmp %edi, %r15
    @{ if (restricted_register == REG_RSP) {
         PrintError("Incorrectly modified register %%rsp\n", begin - data);
         result = 1;
         goto error_detected;
       } else if (restricted_register == REG_RBP) {
         PrintError("Incorrectly modified register %%rbp\n", begin - data);
         result = 1;
         goto error_detected;
       }
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
    @{ if (restricted_register == REG_RSP) {
         PrintError("Incorrectly modified register %%rsp\n", begin - data);
         result = 1;
         goto error_detected;
       } else if (restricted_register == REG_RBP) {
         PrintError("Incorrectly modified register %%rbp\n", begin - data);
         result = 1;
         goto error_detected;
       }
       BitmapClearBit(valid_targets, (p - data) - 5);
       BitmapClearBit(valid_targets, (p - data) - 2);
       restricted_register = kNoRestrictedReg;
    } |
    (0x49 0x8d 0x34 0x37) # lea (%r15,%rsi,1),%rsi
    @{ if (restricted_register == REG_RSI) {
         sandboxed_rsi = begin;
         restricted_register = kSandboxedRsi;
       } else {
         restricted_register = kNoRestrictedReg;
       }
    } |
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
    } |
    (rep? 0xac                 | # lods   %ds:(%rsi),%al
     data16rep 0xad            | # lods   %ds:(%rsi),%ax
     rep? REXW_NONE? 0xad)       # lods   %ds:(%rsi),%eax/%rax
    @{ if (restricted_register != kSandboxedRsi) {
         PrintError("Incorrectly sandboxed %%rdi\n", begin - data);
         result = 1;
         goto error_detected;
       }
       restricted_register = kNoRestrictedReg;
       BitmapClearBit(valid_targets, (begin - data));
       BitmapClearBit(valid_targets, (sandboxed_rdi - data));
    } |
    (condrep? 0xae             | # scas   %es:(%rdi),%al
     data16condrep 0xaf        | # scas   %es:(%rdi),%ax
     condrep? REXW_NONE? 0xaf  | # scas   %es:(%rdi),%eax/%rax

     rep? 0xaa                 | # stos   %al,%es:(%rdi)
     data16rep 0xab            | # stos   %ax,%es:(%rdi)
     rep? REXW_NONE? 0xab)       # stos   %eax/%rax,%es:(%rdi)
    @{ if (restricted_register != kSandboxedRdi &&
           restricted_register != kSandboxedRsiSandboxedRdi) {
         PrintError("Incorrectly sandboxed %%rdi\n", begin - data);
         result = 1;
         goto error_detected;
       }
       restricted_register = kNoRestrictedReg;
       BitmapClearBit(valid_targets, (begin - data));
       BitmapClearBit(valid_targets, (sandboxed_rdi - data));
    } |
    (condrep? 0xa6            | # cmpsb    %es:(%rdi),%ds:(%rsi)
     data16condrep 0xa7       | # cmpsw    %es:(%rdi),%ds:(%rsi)
     condrep? REXW_NONE? 0xa7 | # cmps[lq] %es:(%rdi),%ds:(%rsi)

     rep? 0xa4                | # movsb    %es:(%rdi),%ds:(%rsi)
     data16rep 0xa5           | # movsw    %es:(%rdi),%ds:(%rsi)
     rep? REXW_NONE? 0xa5)      # movs[lq] %es:(%rdi),%ds:(%rsi)
    @{ if (restricted_register != kSandboxedRsiSandboxedRdi) {
         PrintError("Incorrectly sandboxed %%rsi or %%rdi\n", begin - data);
         result = 1;
         goto error_detected;
       }
       restricted_register = kNoRestrictedReg;
       BitmapClearBit(valid_targets, (begin - data));
       BitmapClearBit(valid_targets, (sandboxed_rsi - data));
       BitmapClearBit(valid_targets, (sandboxed_rsi_restricted_rdi - data));
       BitmapClearBit(valid_targets, (sandboxed_rdi - data));
    };

  main := ((normal_instruction | special_instruction) >{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
        SET_REX_PREFIX(FALSE);
        SET_VEX_PREFIX2(0xe0);
        SET_VEX_PREFIX3(0x00);
        operand_states = 0;
     })*
     @{
       /* On successful match the instruction start must point to the next byte
        * to be able to report the new offset as the start of instruction
        * causing error.  */
       begin = p + 1;
     }
    $err{
        process_error(begin, userdata);
        result = 1;
        goto error_detected;
    };

}%%

%% write data;

#define GET_REX_PREFIX() rex_prefix
#define SET_REX_PREFIX(P) rex_prefix = (P)
#define GET_VEX_PREFIX2() vex_prefix2
#define SET_VEX_PREFIX2(P) vex_prefix2 = (P)
#define GET_VEX_PREFIX3() vex_prefix3
#define SET_VEX_PREFIX3(P) vex_prefix3 = (P)

/* Ignore this information for now.  */
#define SET_DATA16_PREFIX(S)
#define SET_LOCK_PREFIX(S)
#define SET_REPZ_PREFIX(S)
#define SET_REPNZ_PREFIX(S)
#define SET_BRANCH_TAKEN(S)
#define SET_BRANCH_NOT_TAKEN(S)

enum operand_kind {
  OperandSandboxIrrelevant = 0,
  /*
   * Currently we do not distinguish 8bit and 16bit modifications from
   * OperandSandboxUnrestricted to match the behavior of the old validator.
   *
   * 8bit operands must be distinguished from other types because the REX prefix
   * regulates the choice between %ah and %spl, as well as %ch and %bpl.
   */
  OperandSandbox8bit,
  OperandSandboxRestricted,
  OperandSandboxUnrestricted
};

#define SET_OPERAND_NAME(N, S) operand_states |= ((S) << ((N) << 3))
#define SET_OPERAND_TYPE(N, T) SET_OPERAND_TYPE_ ## T(N)
#define SET_OPERAND_TYPE_OperandSize8bit(N) \
  operand_states |= OperandSandbox8bit << (5 + ((N) << 3))
#define SET_OPERAND_TYPE_OperandSize16bit(N) \
  operand_states |= OperandSandboxUnrestricted << (5 + ((N) << 3))
#define SET_OPERAND_TYPE_OperandSize32bit(N) \
  operand_states |= OperandSandboxRestricted << (5 + ((N) << 3))
#define SET_OPERAND_TYPE_OperandSize64bit(N) \
  operand_states |= OperandSandboxUnrestricted << (5 + ((N) << 3))
#define CHECK_OPERAND(N, S, T) \
  ((operand_states & (0xff << ((N) << 3))) == ((S | (T << 5)) << ((N) << 3)))
#define SET_OPERANDS_COUNT(N)
#define SET_MODRM_BASE(N) base = (N)
#define SET_MODRM_INDEX(N) index = (N)

/* Ignore this information for now.  */
#define SET_MODRM_SCALE(S)
#define SET_DISP_TYPE(T)
#define SET_DISP_PTR(P)
#define SET_IMM_TYPE(T)
#define SET_IMM_PTR(P)
#define SET_IMM2_TYPE(T)
#define SET_IMM2_PTR(P)

#define SET_CPU_FEATURE(F) \
  if (!(F)) { \
    printf("offset 0x%"NACL_PRIxS": CPU Feature not found", \
           (uintptr_t)(begin - data)); \
    result = 1; \
    goto error_detected; \
  }
#define CPUFeature_3DNOW    cpu_features->data[NaClCPUFeature_3DNOW]
#define CPUFeature_3DPRFTCH CPUFeature_3DNOW || CPUFeature_PRE || CPUFeature_LM
#define CPUFeature_AES      cpu_features->data[NaClCPUFeature_AES]
#define CPUFeature_AESAVX   CPUFeature_AES && CPUFeature_AVX
#define CPUFeature_AVX      cpu_features->data[NaClCPUFeature_AVX]
#define CPUFeature_BMI1     cpu_features->data[NaClCPUFeature_BMI1]
#define CPUFeature_CLFLUSH  cpu_features->data[NaClCPUFeature_CLFLUSH]
#define CPUFeature_CLMUL    cpu_features->data[NaClCPUFeature_CLMUL]
#define CPUFeature_CLMULAVX CPUFeature_CLMUL && CPUFeature_AVX
#define CPUFeature_CMOV     cpu_features->data[NaClCPUFeature_CMOV]
#define CPUFeature_CMOVx87  CPUFeature_CMOV && CPUFeature_x87
#define CPUFeature_CX16     cpu_features->data[NaClCPUFeature_CX16]
#define CPUFeature_CX8      cpu_features->data[NaClCPUFeature_CX8]
#define CPUFeature_E3DNOW   cpu_features->data[NaClCPUFeature_E3DNOW]
#define CPUFeature_EMMX     cpu_features->data[NaClCPUFeature_EMMX]
#define CPUFeature_EMMXSSE  CPUFeature_EMMX || CPUFeature_SSE
#define CPUFeature_F16C     cpu_features->data[NaClCPUFeature_F16C]
#define CPUFeature_FMA      cpu_features->data[NaClCPUFeature_FMA]
#define CPUFeature_FMA4     cpu_features->data[NaClCPUFeature_FMA4]
#define CPUFeature_FXSR     cpu_features->data[NaClCPUFeature_FXSR]
#define CPUFeature_LAHF     cpu_features->data[NaClCPUFeature_LAHF]
#define CPUFeature_LM       cpu_features->data[NaClCPUFeature_LM]
#define CPUFeature_LWP      cpu_features->data[NaClCPUFeature_LWP]
#define CPUFeature_LZCNT    cpu_features->data[NaClCPUFeature_LZCNT]
#define CPUFeature_MMX      cpu_features->data[NaClCPUFeature_MMX]
#define CPUFeature_MON      cpu_features->data[NaClCPUFeature_MON]
#define CPUFeature_MOVBE    cpu_features->data[NaClCPUFeature_MOVBE]
#define CPUFeature_OSXSAVE  cpu_features->data[NaClCPUFeature_OSXSAVE]
#define CPUFeature_POPCNT   cpu_features->data[NaClCPUFeature_POPCNT]
#define CPUFeature_PRE      cpu_features->data[NaClCPUFeature_PRE]
#define CPUFeature_SSE      cpu_features->data[NaClCPUFeature_SSE]
#define CPUFeature_SSE2     cpu_features->data[NaClCPUFeature_SSE2]
#define CPUFeature_SSE3     cpu_features->data[NaClCPUFeature_SSE3]
#define CPUFeature_SSE41    cpu_features->data[NaClCPUFeature_SSE41]
#define CPUFeature_SSE42    cpu_features->data[NaClCPUFeature_SSE42]
#define CPUFeature_SSE4A    cpu_features->data[NaClCPUFeature_SSE4A]
#define CPUFeature_SSSE3    cpu_features->data[NaClCPUFeature_SSSE3]
#define CPUFeature_TBM      cpu_features->data[NaClCPUFeature_TBM]
#define CPUFeature_TSC      cpu_features->data[NaClCPUFeature_TSC]
#define CPUFeature_x87      cpu_features->data[NaClCPUFeature_x87]
#define CPUFeature_XOP      cpu_features->data[NaClCPUFeature_XOP]

enum {
  REX_B = 1,
  REX_X = 2,
  REX_R = 4,
  REX_W = 8
};

static const int kBitsPerByte = 8;

static inline uint8_t *BitmapAllocate(size_t indexes) {
  size_t byte_count = (indexes + kBitsPerByte - 1) / kBitsPerByte;
  uint8_t *bitmap = malloc(byte_count);
  if (bitmap != NULL) {
    memset(bitmap, 0, byte_count);
  }
  return bitmap;
}

static inline int BitmapIsBitSet(uint8_t *bitmap, size_t index) {
  return (bitmap[index / kBitsPerByte] & (1 << (index % kBitsPerByte))) != 0;
}

static inline void BitmapSetBit(uint8_t *bitmap, size_t index) {
  bitmap[index / kBitsPerByte] |= 1 << (index % kBitsPerByte);
}

static inline void BitmapClearBit(uint8_t *bitmap, size_t index) {
  bitmap[index / kBitsPerByte] &= ~(1 << (index % kBitsPerByte));
}

static int CheckJumpTargets(uint8_t *valid_targets, uint8_t *jump_dests,
                            size_t size) {
  size_t i;
  for (i = 0; i < size / 32; i++) {
    uint32_t jump_dest_mask = ((uint32_t *) jump_dests)[i];
    uint32_t valid_target_mask = ((uint32_t *) valid_targets)[i];
    if ((jump_dest_mask & ~valid_target_mask) != 0) {
      printf("bad jump to around %x\n", (unsigned)(i * 32));
      return 1;
    }
  }
  return 0;
}

int ValidateChunkAMD64(const uint8_t *data, size_t size,
                       const NaClCPUFeaturesX86 *cpu_features,
                       process_validation_error_func process_error,
                       void *userdata) {
  const size_t bundle_size = 32;
  const size_t bundle_mask = bundle_size - 1;

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
  int operand_states = 0;
  enum register_name base = NO_REG;
  enum register_name index = NO_REG;
  int result = 0;
  /* These are borders of the appropriate instructions.  Initialize them to make
   * compiler happy: they are never used uninitialized even without explicit
   * initialization but GCC is not sophysicated enough to prove that.  */
  const uint8_t *sandboxed_rsi = 0;
  const uint8_t *sandboxed_rsi_restricted_rdi = 0;
  const uint8_t *sandboxed_rdi = 0;

  assert(size % bundle_size == 0);

  while (p < data + size) {
    const uint8_t *pe = p + bundle_size;
    const uint8_t *eof = pe;
    int cs;
    enum {
      kNoRestrictedReg = 32,
      kSandboxedRsi,
      kSandboxedRdi,
      kSandboxedRsiRestrictedRdi,
      kSandboxedRsiSandboxedRdi
    }; uint8_t restricted_register = kNoRestrictedReg;

    %% write init;
  /* Ragel-generated code stores a difference between pointers into an "int"
   * variable. This produces C4244 warning on Windows x64.  */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244) // possible loss of data
#endif
    %% write exec;
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    if (restricted_register == REG_RBP) {
      PrintError("Incorrectly sandboxed %%rbp\n", begin - data);
      result = 1;
      goto error_detected;
    } else if (restricted_register == REG_RSP) {
      PrintError("Incorrectly sandboxed %%rsp\n", begin - data);
      result = 1;
      goto error_detected;
    }
  }

  if (CheckJumpTargets(valid_targets, jump_dests, size)) {
    return 1;
  }

error_detected:
  return result;
}
