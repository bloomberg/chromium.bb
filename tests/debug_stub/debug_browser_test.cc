// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <cstdio>
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

// Create a minimal instance and module so that the nexe will
// be loaded in a browser.  Since we are loading this in a browser
// which was started with debugging enabled (--enable-nacl-debug --no-sandbox)
// we don't need it to do anything because the debug_stub will 'break'
// as soon as the nexe gets loaded.
class MyInstance : public pp::Instance {
 private:
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance) {}
  virtual void HandleMessage(const pp::Var& var_message);
};

void MyInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_string()) {
    PostMessage("bad message");
    return;
  }
  std::string message = var_message.AsString();
  pp::Var return_var;
  // Post the return result back to the browser.
  return_var = std::string("Nexe received {") + message + "}";
  PostMessage(return_var);
}

class MyModule : public pp::Module {
 public:
  // Override CreateInstance to create your customized Instance object.
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
