// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_udp_socket_private_disallowed.h"

#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(UDPSocketPrivateDisallowed);

TestUDPSocketPrivateDisallowed::TestUDPSocketPrivateDisallowed(
    TestingInstance* instance)
    : TestCase(instance), udp_socket_private_interface_(NULL) {
}

bool TestUDPSocketPrivateDisallowed::Init() {
  udp_socket_private_interface_ = static_cast<const PPB_UDPSocket_Private*>(
      pp::Module::Get()->GetBrowserInterface(PPB_UDPSOCKET_PRIVATE_INTERFACE));
  if (!udp_socket_private_interface_)
    instance_->AppendError("UDPSocketPrivate interface not available");
  return udp_socket_private_interface_ && InitTestingInterface();
}

void TestUDPSocketPrivateDisallowed::RunTests(const std::string& filter) {
  RUN_TEST(Create, filter);
}

std::string TestUDPSocketPrivateDisallowed::TestCreate() {
  PP_Resource socket =
      udp_socket_private_interface_->Create(instance_->pp_instance());
  if (0 != socket) {
    return "PPB_UDPSocket_Private::Create returns valid socket " \
        "without allowing switch";
  }
  PASS();
}
