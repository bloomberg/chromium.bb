// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_tcp_socket_private_disallowed.h"

#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(TCPSocketPrivateDisallowed);

TestTCPSocketPrivateDisallowed::TestTCPSocketPrivateDisallowed(
    TestingInstance* instance)
    : TestCase(instance), tcp_socket_private_interface_(NULL) {
}

bool TestTCPSocketPrivateDisallowed::Init() {
  tcp_socket_private_interface_ = static_cast<const PPB_TCPSocket_Private*>(
      pp::Module::Get()->GetBrowserInterface(PPB_TCPSOCKET_PRIVATE_INTERFACE));
  if (!tcp_socket_private_interface_)
    instance_->AppendError("TCPSocketPrivate interface not available");
  return tcp_socket_private_interface_ && InitTestingInterface();
}

void TestTCPSocketPrivateDisallowed::RunTests(const std::string& filter) {
  RUN_TEST(Create, filter);
}

std::string TestTCPSocketPrivateDisallowed::TestCreate() {
  PP_Resource socket =
      tcp_socket_private_interface_->Create(instance_->pp_instance());
  if (0 != socket) {
    return "PPB_TCPSocket_Private::Create returns valid socket " \
        "without allowing switch";
  }
  PASS();
}
