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

static const size_t kBundleSize = 32;
static const size_t kBundleMask = 31;

/* Mark the destination of a jump instruction and make an early validity check:
 * to jump outside given code region, the target address must be aligned.
 *
 * Returns TRUE iff the jump passes the early validity check.
 */
static int MarkJumpTarget(size_t jump_dest,
                          uint8_t *jump_dests,
                          size_t size,
                          size_t report_inst_offset) {
  if ((jump_dest & kBundleMask) == 0) {
    return TRUE;
  }
  if (jump_dest >= size) {
    printf("offset 0x%zx: direct jump out of range at destination: %"NACL_PRIxS
           "\n",
           report_inst_offset,
           jump_dest);
    return FALSE;
  }
  BitmapSetBit(jump_dests, jump_dest);
  return TRUE;
}

%%{
  machine x86_64_decoder;
  alphtype unsigned char;

  action rel8_operand {
    int8_t offset = (uint8_t) (p[0]);
    size_t jump_dest = offset + (p - data) + 1;

    if (!MarkJumpTarget(jump_dest, jump_dests, size, begin - data)) {
      result = 1;
      goto error_detected;
    }
  }
  action rel16_operand {
    assert(FALSE);
  }
  action rel32_operand {
    int32_t offset =
        (p[-3] + 256U * (p[-2] + 256U * (p[-1] + 256U * ((uint32_t) p[0]))));
    size_t jump_dest = offset + (p - data) + 1;

    if (!MarkJumpTarget(jump_dest, jump_dests, size, begin - data)) {
      result = 1;
      goto error_detected;
    }
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

  include decode_x86_32 "validator-x86_32-instruction.rl";

  special_instruction =
    (0x83 0xe0 0xe0 0xff (0xd0|0xe0)  | # naclcall/jmp %acx
     0x83 0xe1 0xe0 0xff (0xd1|0xe1)  | # naclcall %ecx
     0x83 0xe2 0xe0 0xff (0xd2|0xe2)  | # naclcall %edx
     0x83 0xe3 0xe0 0xff (0xd3|0xe3)  | # naclcall %ebx
     0x83 0xe4 0xe0 0xff (0xd4|0xe4)  | # naclcall %esp
     0x83 0xe5 0xe0 0xff (0xd5|0xe5)  | # naclcall %ebp
     0x83 0xe6 0xe0 0xff (0xd6|0xe6)  | # naclcall %esi
     0x83 0xe7 0xe0 0xff (0xd7|0xe7))   # naclcall %edi
    @{
      BitmapClearBit(valid_targets, (p - data) - 1);
    } |
    (0x65 0xa1 0x00 0x00 0x00 0x00      | # mov %gs:0x0,%eax
     0x65 0x8b (0x05|0x0d|0x015|0x1d|0x25|0x2d|0x35|0x3d)
          0x00 0x00 0x00 0x00);           # mov %gs:0x0,%reg

  main := ((one_instruction | special_instruction) >{
        begin = p;
        BitmapSetBit(valid_targets, p - data);
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

/* Ignore this information for now.  */
#define GET_VEX_PREFIX3 0
#define SET_VEX_PREFIX3(P)
#define SET_DATA16_PREFIX(S)
#define SET_LOCK_PREFIX(S)
#define SET_REPZ_PREFIX(S)
#define SET_REPNZ_PREFIX(S)
#define SET_BRANCH_TAKEN(S)
#define SET_BRANCH_NOT_TAKEN(S)
#define SET_DISP_TYPE(T)
#define SET_DISP_PTR(P)
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

int ValidateChunkIA32(const uint8_t *data, size_t size,
                      const NaClCPUFeaturesX86 *cpu_features,
                      process_validation_error_func process_error,
                      void *userdata) {
  uint8_t *valid_targets = BitmapAllocate(size);
  uint8_t *jump_dests = BitmapAllocate(size);

  const uint8_t *p = data;
  const uint8_t *begin = p;  /* Start of the instruction being processed.  */

  int result = 0;

  assert(size % kBundleSize == 0);

  while (p < data + size) {
    const uint8_t *pe = p + kBundleSize;
    const uint8_t *eof = pe;
    int cs;

    %% write init;
    %% write exec;
  }

  if (CheckJumpTargets(valid_targets, jump_dests, size)) {
    return 1;
  }

error_detected:
  return result;
}
