/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_CORE_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_CORE_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/ppb_core.h"

namespace fake_browser_ppapi {

// Implements the PPB_Core interface.
class Core {
 public:
  // Return an interface pointer usable by PPAPI.
  static const PPB_Core* GetInterface();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(Core);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_CORE_H_
