/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator_x86/ncdis_segments.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/ncdis_util.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/nc_segment.h"

Bool NACL_FLAGS_use_iter = (64 == NACL_TARGET_SUBARCH);

Bool NACL_FLAGS_internal = FALSE;

void NaClDisassembleSegment(uint8_t* mbase, NaClPcAddress vbase,
                            NaClMemorySize size) {
  if (NACL_FLAGS_use_iter) {
    NaClSegment segment;
    NaClInstIter* iter;
    struct Gio* gout = NaClLogGetGio();
    NaClSegmentInitialize(mbase, vbase, size, &segment);
    for (iter = NaClInstIterCreate(&segment); NaClInstIterHasNext(iter);
         NaClInstIterAdvance(iter)) {
      NaClInstState* state = NaClInstIterGetState(iter);
      NaClInstStateInstPrint(gout, state);
      if (NACL_FLAGS_internal) {
        NaClInstPrint(gout, NaClInstStateInst(state));
        NaClExpVectorPrint(gout, NaClInstStateExpVector(state));
      }
    }
    NaClInstIterDestroy(iter);
  } else {
    NCDecodeSegment(mbase, vbase, size);
  }
}
