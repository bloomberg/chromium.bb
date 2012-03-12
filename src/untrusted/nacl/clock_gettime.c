/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <time.h>

#include "native_client/src/untrusted/nacl/nacl_irt.h"

NACL_OPTIONAL_FN(__libnacl_irt_clock, NACL_IRT_CLOCK_v0_1,
                 clock_gettime,
                 (clk_id, tp), (clockid_t clk_id, struct timespec *tp))
