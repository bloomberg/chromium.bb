/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/sel_universal/pepper_emu.h"

void PepperEmuInit3D(NaClCommandLoop* ncl, IMultimedia* im) {
  UNREFERENCED_PARAMETER(ncl);
  UNREFERENCED_PARAMETER(im);
  NaClLog(LOG_INFO, "Pepper 3D not supported\n");
}
