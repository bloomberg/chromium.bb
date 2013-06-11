/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/pthread/futex.h"

const struct nacl_irt_futex nacl_irt_futex = {
  __nc_futex_wait,
  __nc_futex_wake,
};
