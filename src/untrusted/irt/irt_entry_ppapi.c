/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/ppruntime.h"

static __attribute__((constructor)) void CallIrtInit(void) {
  if (IrtInit()) {
    static const char fatal_msg[] =
        "irt initialization (IRTInit) failed.\n";
    write(2, fatal_msg, sizeof(fatal_msg) - 1);
    _exit(-1);
  }
}
