// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_websocket.h"

#include "ppapi/c/dev/ppb_websocket_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(WebSocket);

bool TestWebSocket::Init() {
  websocket_interface_ = reinterpret_cast<PPB_WebSocket_Dev const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_WEBSOCKET_DEV_INTERFACE));
  return !!websocket_interface_;
}

void TestWebSocket::RunTests(const std::string& filter) {
  instance_->LogTest("Create", TestCreate());
  instance_->LogTest("IsWebSocket", TestIsWebSocket());
}

std::string TestWebSocket::TestCreate() {
  PP_Resource rsrc = websocket_interface_->Create(instance_->pp_instance());
  if (!rsrc)
    return "Could not create websocket via C interface";

  PASS();
}

std::string TestWebSocket::TestIsWebSocket() {
  // Test that a NULL resource isn't a websocket.
  pp::Resource null_resource;
  if (websocket_interface_->IsWebSocket(null_resource.pp_resource()))
    return "Null resource was reported as a valid websocket";

  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  if (!websocket_interface_->IsWebSocket(ws))
    return "websocket was reported as an invalid websocket";

  PASS();
}
