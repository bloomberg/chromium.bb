/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub simple non-thread safe errno for porting support.
 */

int errno;

#include "native_client/src/untrusted/nosys/warning.h"
link_warning(errno,
  "the stub `errno\' is not thread-safe, don\'t use it in multi-threaded environment");
