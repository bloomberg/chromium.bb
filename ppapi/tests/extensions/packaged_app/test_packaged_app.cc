// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

namespace {

class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance) : pp::Instance(instance) { }
  virtual ~MyInstance() { }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    PostMessage("hello");
    return true;
  }
};

class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() { }
  virtual ~MyModule() { }

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

}  // namespace

namespace pp {

Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
