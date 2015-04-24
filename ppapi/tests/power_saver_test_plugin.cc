// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/tests/test_utils.h"

// Windows defines 'PostMessage', so we have to undef it.
#ifdef PostMessage
#undef PostMessage
#endif

// This is a simple C++ Pepper plugin that enables Plugin Power Saver tests.
class PowerSaverTestInstance : public pp::Instance {
 public:
  explicit PowerSaverTestInstance(PP_Instance instance)
      : pp::Instance(instance), received_first_did_change_view_(false) {}
  ~PowerSaverTestInstance() override {}

  // For browser tests, responds to:
  //  - When postMessage("isPeripheral") is called on the plugin DOM element.
  //  - When the plugin throttler posts a message notifying us that our
  //    peripheral status has changed.
  void HandleMessage(const pp::Var& message_data) override {
    if (message_data.is_string()) {
      if (message_data.AsString() == "getPeripheralStatus")
        BroadcastIsPeripheralStatus("getPeripheralStatusResponse");
      else if (message_data.AsString() == "peripheralStatusChange")
        BroadcastIsPeripheralStatus("peripheralStatusChange");
    }
  }

  // Broadcast our peripheral status after the initial view data. This is for
  // tests that await initial plugin creation.
  void DidChangeView(const pp::View& view) override {
    if (!received_first_did_change_view_) {
      BroadcastIsPeripheralStatus("initial");
      received_first_did_change_view_ = true;
    }
  }

 private:
  void BroadcastIsPeripheralStatus(const std::string& source) {
    pp::VarDictionary message;
    message.Set(
        "isPeripheral",
        pp::Var(PP_ToBool(GetTestingInterface()->IsPeripheral(pp_instance()))));
    message.Set("source", pp::Var(source));
    PostMessage(message);
  }

  bool received_first_did_change_view_;
};

class PowerSaverTestModule : public pp::Module {
 public:
  PowerSaverTestModule() : pp::Module() {}
  virtual ~PowerSaverTestModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new PowerSaverTestInstance(instance);
  }
};

namespace pp {

Module* CreateModule() {
  return new PowerSaverTestModule();
}

}  // namespace pp
