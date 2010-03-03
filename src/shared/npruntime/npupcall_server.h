// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #include <limits>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/npruntime/npmodule.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

// Support for "upcalls" -- RPCs to the browser that are done from other than
// the NPAPI thread.  These calls are synchronized by the npruntime library
// at the caller end.

namespace nacl {

class NPUpcallServer {
 public:
  static DescWrapper* Start(NPModule* module, struct NaClThread* nacl_thread);
};

}  // namespace nacl
