// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "native_client/src/trusted/plugin/nexe_arch.h"

#if defined(NACL_STANDALONE)
const char* NaClPluginGetSandboxISA() {
  return plugin::GetSandboxISA();
}
#endif

