/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_NACL_PRIVATE_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_NACL_PRIVATE_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"
#include "ppapi/c/private/ppb_nacl_private.h"

namespace fake_browser_ppapi {

class NaClPrivate {
 public:
  static const PPB_NaCl_Private* GetInterface();

  NACL_DISALLOW_COPY_AND_ASSIGN(NaClPrivate);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_NACL_PRIVATE_H_
