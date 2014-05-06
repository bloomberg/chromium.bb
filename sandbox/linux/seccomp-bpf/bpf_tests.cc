// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/bpf_tests.h"

namespace sandbox {

// static
template <>
void* BPFTesterSimpleDelegate<void>::NewAux() {
  return NULL;
}

// static
template <>
void BPFTesterSimpleDelegate<void>::DeleteAux(void* aux) {
  CHECK(!aux);
}

}  // namespace sandbox
