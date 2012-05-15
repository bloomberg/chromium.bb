/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <io.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/win/lock_impl.h"

static const char kMutexCreateFailed[] =
    "Unrecoverable failure on CreateMutex.\n";

NaCl::LockImpl::LockImpl() {
  /*
   * Since this is probably resource exhaustion, and/or this could be the
   * log's lock, then logging is risky so we write directly to stderr.
   */
  if (!NaClFastMutexCtor(&mu_)) {
    _write(2, kMutexCreateFailed, sizeof kMutexCreateFailed - 1);
    NaClAbort();
  }
}

NaCl::LockImpl::~LockImpl() {
  NaClFastMutexDtor(&mu_);
}

bool NaCl::LockImpl::Try() {
  return NaClFastMutexTryLock(&mu_) == 0;
}

void NaCl::LockImpl::Lock() {
  NaClFastMutexLock(&mu_);
}

void NaCl::LockImpl::Unlock() {
  NaClFastMutexUnlock(&mu_);
}
