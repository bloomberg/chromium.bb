// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/tests/fake_browser_ppapi/fake_nacl_private.h"

#include "base/rand_util_c.h"
#include "ppapi/c/private/ppb_nacl_private.h"

namespace fake_browser_ppapi {

namespace {

bool LaunchSelLdr(const char* alleged_url, int socket_count,
                  void* imc_handles, void* nacl_process_handle,
                  int* nacl_process_id) {
  UNREFERENCED_PARAMETER(alleged_url);
  UNREFERENCED_PARAMETER(socket_count);
  UNREFERENCED_PARAMETER(imc_handles);
  UNREFERENCED_PARAMETER(nacl_process_handle);
  UNREFERENCED_PARAMETER(nacl_process_id);
  return false;
}

int UrandomFD(void) {
  return 0;
}

}  // namespace

const PPB_NaCl_Private ppb_nacl = {
  &LaunchSelLdr,
  &UrandomFD,
};

// static
const PPB_NaCl_Private* NaClPrivate::GetInterface() {
  return &ppb_nacl;
}


}  // namespace fake_browser_ppapi
