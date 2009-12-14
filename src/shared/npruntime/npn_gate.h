// Copyright (c) 2009 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPN_GATE_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPN_GATE_H_

#include <stdlib.h>

#include "native_client/src/third_party/npapi/files/include/npupp.h"

namespace nacl {

extern const NPNetscapeFuncs* GetBrowserFuncs();

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPN_GATE_H_
