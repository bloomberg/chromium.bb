// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Wow64 implementation for native 64-bit Windows (in other words, never WOW).

#include "sandbox/src/wow64.h"

namespace sandbox {

Wow64::~Wow64() {
}

bool Wow64::IsWow64() {
  return false;
}

bool Wow64::WaitForNtdll(DWORD timeout_ms) {
  return true;
}

}  // namespace sandbox
