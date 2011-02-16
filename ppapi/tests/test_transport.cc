// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_transport.h"

#include <string.h>

#include <string>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/dev/transport_dev.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Transport);

bool TestTransport::Init() {
  transport_interface_ = reinterpret_cast<PPB_Transport_Dev const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_TRANSPORT_DEV_INTERFACE));
  return transport_interface_ && InitTestingInterface();
}

void TestTransport::RunTest() {
  RUN_TEST(Basics);
  // TODO(juberti): more Transport tests here...
}

std::string TestTransport::TestBasics() {
  pp::Transport_Dev transport1(instance_, "test", "");
  pp::Transport_Dev transport2(instance_, "test", "");

  TestCompletionCallback connect_cb1(instance_->pp_instance());
  TestCompletionCallback connect_cb2(instance_->pp_instance());
  ASSERT_EQ(transport1.Connect(connect_cb1), PP_ERROR_WOULDBLOCK);
  ASSERT_EQ(transport2.Connect(connect_cb2), PP_ERROR_WOULDBLOCK);

  pp::Var address1;
  pp::Var address2;
  TestCompletionCallback next_address_cb1(instance_->pp_instance());
  TestCompletionCallback next_address_cb2(instance_->pp_instance());
  ASSERT_EQ(transport1.GetNextAddress(&address1, next_address_cb1),
            PP_ERROR_WOULDBLOCK);
  ASSERT_EQ(transport2.GetNextAddress(&address2, next_address_cb2),
            PP_ERROR_WOULDBLOCK);
  ASSERT_EQ(next_address_cb1.WaitForResult(), 0);
  ASSERT_EQ(next_address_cb2.WaitForResult(), 0);
  ASSERT_EQ(transport1.GetNextAddress(&address1, next_address_cb1), PP_OK);
  ASSERT_EQ(transport2.GetNextAddress(&address2, next_address_cb2), PP_OK);

  ASSERT_EQ(transport1.ReceiveRemoteAddress(address2), 0);
  ASSERT_EQ(transport2.ReceiveRemoteAddress(address1), 0);

  ASSERT_EQ(connect_cb1.WaitForResult(), 0);
  ASSERT_EQ(connect_cb2.WaitForResult(), 0);

  ASSERT_TRUE(transport1.IsWritable());
  ASSERT_TRUE(transport2.IsWritable());

  // TODO(sergeyu): Verify that data can be sent/received.

  PASS();
}
