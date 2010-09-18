/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_WINDOW_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_WINDOW_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/tests/fake_browser_ppapi/fake_host.h"
#include "native_client/tests/fake_browser_ppapi/fake_object.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"

namespace fake_browser_ppapi {

// Implements a fake window DOM object.
class FakeWindow {
 public:
  FakeWindow(PP_Module browser_module, Host* host, const char* page_url);
  ~FakeWindow();
  PP_Var FakeWindowObject();

 private:
  PP_Var window_var_;
  Host* host_;
  NACL_DISALLOW_COPY_AND_ASSIGN(FakeWindow);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_WINDOW_H_
