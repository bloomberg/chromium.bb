// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_INSTANCE_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_INSTANCE_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb_instance.h"

struct PPB_Instance_Private;

namespace fake_browser_ppapi {

class FakeWindow;
class Host;

// Implements the PPB_Instance interface.
class Instance {
 public:
  // You must call set_window to complete initialization.
  Instance() : instance_id_(0), window_(NULL) {}
  virtual ~Instance() {}

  void set_window(FakeWindow* window) { window_ = window; }

  void set_instance_id(PP_Instance instance_id) { instance_id_ = instance_id; }
  PP_Instance instance_id() const { return instance_id_; }

  // The bindings for the methods invoked by the PPAPI interface.
  virtual PP_Var GetWindowObject();
  virtual PP_Var GetOwnerElementObject();
  virtual bool BindGraphics(PP_Resource device);
  virtual bool IsFullFrame();
  virtual PP_Var ExecuteScript(PP_Var script,
                               PP_Var* exception);
  static const PPB_Instance* GetInterface();
  static const PPB_Instance_Private* GetPrivateInterface();
  static Instance* Invalid() { return &kInvalidInstance; }
 private:
  static Instance kInvalidInstance;
  PP_Instance instance_id_;
  FakeWindow* window_;
  NACL_DISALLOW_COPY_AND_ASSIGN(Instance);
};

// These are made global so that C API functions can access them from any file.
// To be implemented by main.cc.
PP_Instance TrackInstance(Instance* instance);
// Returns Instance::Invalid() on error.
Instance* GetInstance(PP_Instance instance_id);

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_INSTANCE_H_
