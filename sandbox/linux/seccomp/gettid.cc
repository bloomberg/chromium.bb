// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug.h"
#include "sandbox_impl.h"

namespace playground {

int Sandbox::sandbox_gettid() {
  Debug::syscall(__NR_gettid, "Executing handler");
  return tid();
}

} // namespace
