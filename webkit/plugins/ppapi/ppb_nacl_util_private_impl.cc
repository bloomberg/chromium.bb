// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_nacl_util_private_impl.h"

#include "base/rand_util_c.h"
#include "ppapi/c/private/ppb_nacl_util_private.h"
#include "webkit/glue/webkit_glue.h"

namespace webkit {
namespace ppapi {

namespace {

bool LaunchSelLdr(const char* alleged_url, int socket_count,
                  void* imc_handles, void* nacl_process_handle,
                  int* nacl_process_id) {
  return webkit_glue::LaunchSelLdr(alleged_url, socket_count, imc_handles,
                                   nacl_process_handle, nacl_process_id);
}

int UrandomFD(void) {
#if defined(OS_POSIX)
  return GetUrandomFD();
#else
  return 0;
#endif
}

}  // namespace

const PPB_NaClUtil_Private ppb_nacl_util = {
  &LaunchSelLdr,
  &UrandomFD,
};

// static
const PPB_NaClUtil_Private* PPB_NaClUtil_Private_Impl::GetInterface() {
  return &ppb_nacl_util;
}

}  // namespace ppapi
}  // namespace webkit
