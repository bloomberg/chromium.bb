// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/bpf_tester_compatibility_delegate.h"

namespace sandbox {

// static
template <>
void* BPFTesterCompatibilityDelegate<void>::NewAux() {
  return NULL;
}

// static
template <>
void BPFTesterCompatibilityDelegate<void>::DeleteAux(void* aux) {
  CHECK(!aux);
}

}  // namespace sandbox
