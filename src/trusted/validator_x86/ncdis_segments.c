/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_x86/ncdis_segments.h"

#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/nc_segment.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"

Bool NACL_FLAGS_use_iter = (64 == NACL_TARGET_SUBARCH);

Bool NACL_FLAGS_internal = FALSE;

void NaClDisassembleSegment(uint8_t* mbase, NaClPcAddress vbase,
                            NaClMemorySize size) {
  if (NACL_FLAGS_use_iter) {
    NaClSegment segment;
    NaClInstIter* iter;
    NaClSegmentInitialize(mbase, vbase, size, &segment);
    for (iter = NaClInstIterCreate(&segment); NaClInstIterHasNext(iter);
         NaClInstIterAdvance(iter)) {
      NaClInstState* state = NaClInstIterGetState(iter);
      NaClInstStateInstPrint(stdout, state);
      if (NACL_FLAGS_internal) {
        NaClInstPrint(stdout, NaClInstStateInst(state));
        NaClExpVectorPrint(stdout, NaClInstStateExpVector(state));
      }
    }
    NaClInstIterDestroy(iter);
  } else {
    NCDecodeSegment(mbase, vbase, size, NULL);
  }
}
