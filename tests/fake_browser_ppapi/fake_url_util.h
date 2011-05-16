/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_URL_UTIL_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_URL_UTIL_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"

namespace fake_browser_ppapi {

// Implements the PPB_URLUtil_Dev interface.
class URLUtil_Dev {
 public:
  explicit URLUtil_Dev(PP_Instance instance_id)
      : instance_id_(instance_id) {}

  // Return an interface pointer usable by PPAPI.
  static const struct PPB_URLUtil_Dev* GetInterface();

 private:
  PP_Instance instance_id_;
  NACL_DISALLOW_COPY_AND_ASSIGN(URLUtil_Dev);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_URL_UTIL_H_
