// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

int Sandbox::sandbox_getpid() {
  long long tm;
  Debug::syscall(&tm, __NR_getpid, "Executing handler");
  Debug::elapsed(tm, __NR_getpid);
  return pid_;
}

} // namespace
