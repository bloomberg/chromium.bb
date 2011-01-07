/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_INSTANCE_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_INSTANCE_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/tests/fake_browser_ppapi/fake_window.h"
#include "ppapi/c/ppb_instance.h"

namespace fake_browser_ppapi {

class Host;

// Implements the PPB_Instance interface.
class Instance {
 public:
  // You must call set_window to complete initialization.
  explicit Instance() : window_(NULL) {}
  virtual ~Instance() {}

  void set_window(FakeWindow* window) { window_ = window; }

  // The bindings for the methods invoked by the PPAPI interface.
  virtual PP_Var GetWindowObject();
  virtual PP_Var GetOwnerElementObject();
  virtual bool BindGraphics(PP_Resource device);
  virtual bool IsFullFrame();
  virtual PP_Var ExecuteScript(PP_Var script,
                               PP_Var* exception);
  static const PPB_Instance* GetInterface();

 private:
  FakeWindow* window_;
  NACL_DISALLOW_COPY_AND_ASSIGN(Instance);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_INSTANCE_H_
