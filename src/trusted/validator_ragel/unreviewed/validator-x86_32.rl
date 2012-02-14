/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <elf.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "validator.h"

#undef TRUE
#define TRUE    1

#undef FALSE
#define FALSE   0

#define check_jump_dest \
    if ((jump_dest & bundle_mask) != bundle_mask) { \
      if (jump_dest >= size) { \
        printf("direct jump out of range: %zx\n", jump_dest); \
        result = 1; \
        goto error_detected; \
      } else { \
        BitmapSetBit(jump_dests, jump_dest + 1); \
      } \
    }

%%{
  machine x86_64_decoder;
  alphtype unsigned char;

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
     0x83 0xe7 0xe0 0xff (0xd7|0xe7))	# naclcall %edi
    @{ BitmapClearBit(valid_targets, (p - data) - 1);
    };

  main := ((one_instruction | special_instruction) >{
	BitmapSetBit(valid_targets, p - data);
     })*
    $!{ process_error(p, userdata);
	result = 1;
	goto error_detected;
    };

}%%

%% write data;

/* Ignore this information for now.  */
#define data16_prefix if (0) result
#define lock_prefix if (0) result
#define repz_prefix if (0) result
#define repnz_prefix if (0) result
#define branch_not_taken if (0) result
#define branch_taken if (0) result
#define vex_prefix3 if (0) result
#define disp if (0) p
#define disp_type if (0) result

enum disp_mode {
  DISPNONE,
  DISP8,
  DISP16,
  DISP32
};

static const int kBitsPerByte = 8;

static inline uint8_t *BitmapAllocate(uint32_t indexes) {
  uint32_t byte_count = (indexes + kBitsPerByte - 1) / kBitsPerByte;
  uint8_t *bitmap = malloc(byte_count);
  if (bitmap != NULL) {
    memset(bitmap, 0, byte_count);
  }
  return bitmap;
}

static inline int BitmapIsBitSet(uint8_t *bitmap, uint32_t index) {
  return (bitmap[index / kBitsPerByte] & (1 << (index % kBitsPerByte))) != 0;
}

static inline void BitmapSetBit(uint8_t *bitmap, uint32_t index) {
  bitmap[index / kBitsPerByte] |= 1 << (index % kBitsPerByte);
}

static inline void BitmapClearBit(uint8_t *bitmap, uint32_t index) {
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

int ValidateChunkIA32(const uint8_t *data, size_t size,
		      process_error_func process_error, void *userdata) {
  const size_t bundle_size = 32;
  const size_t bundle_mask = bundle_size - 1;

  uint8_t *valid_targets = BitmapAllocate(size);
  uint8_t *jump_dests = BitmapAllocate(size);

  const uint8_t *p = data;
/*  const uint8_t *begin;*/

  int result = 0;

  assert(size % bundle_size == 0);

  while (p < data + size) {
    const uint8_t *pe = p + bundle_size;
    const uint8_t *eof = pe;
    int cs;

    %% write init;
    %% write exec;
  }

  if (CheckJumpTargets(valid_targets, jump_dests, size)) {
    result = 1;
    goto error_detected;
  }

error_detected:
  return result;
}
