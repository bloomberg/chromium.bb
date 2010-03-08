// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

int Sandbox::sandbox_getpid() {
  Debug::syscall(__NR_getpid, "Executing handler");
  return pid_;
}

} // namespace
