/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Defines the notion of a code segment.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_SEGMENT_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_SEGMENT_H_

#include "native_client/src/trusted/validator_x86/nc_inst_state.h"

/* Model of a code segment. */
typedef struct NcSegment {
  /* Points the the beginning of the sequence of bytes in the code segment. */
  uint8_t* mbase;
  /* Defines the virtual pc value associated with the beginning
   * of the code segment.
   */
  PcAddress vbase;
  /* Defines the maximum+1 (virtual) pc value. Used to define
   * when the end of the segment is reached. Corresponds to
   * vbase + size;
   */
  PcAddress vlimit;
  /* The number of bytes in the code segment. */
  MemorySize size;
} NcSegment;

/* Initializes the given code segment with the given values. */
void NcSegmentInitialize(
    uint8_t* mbase,
    PcAddress vbase,
    MemorySize size,
    NcSegment* segment);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_SEGMENT_H_ */
