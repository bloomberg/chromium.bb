/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// Portable interface for browser interaction - API invariant portions.

#include "base/rand_util_c.h"

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/trusted/plugin/nacl_entry_points.h"


extern "C" {
  int GetUrandomFD(void) {
    return get_urandom_fd();
  }
}

