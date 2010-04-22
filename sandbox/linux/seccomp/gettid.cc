// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

long Sandbox::sandbox_gettid() {
  long long tm;
  Debug::syscall(&tm, __NR_gettid, "Executing handler");
  pid_t t = tid();
  Debug::elapsed(tm, __NR_gettid);
  return t;
}

} // namespace
