// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_broker.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_broker_trusted.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Broker);

TestBroker::TestBroker(TestingInstance* instance)
    : TestCase(instance),
      broker_interface_(NULL) {
}

bool TestBroker::Init() {
  broker_interface_ = static_cast<PPB_BrokerTrusted const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_BROKER_TRUSTED_INTERFACE));
  return !!broker_interface_;
}

void TestBroker::RunTest() {
  RUN_TEST(Create);
}

std::string TestBroker::TestCreate() {
  // Very simplistic test to make sure we can create a broker interface.
  PP_Resource broker = broker_interface_->CreateTrusted(
      instance_->pp_instance());
  ASSERT_TRUE(broker);

  ASSERT_FALSE(broker_interface_->IsBrokerTrusted(0));
  ASSERT_TRUE(broker_interface_->IsBrokerTrusted(broker));

  // Test getting the handle for an invalid resource.
  int32_t handle;
  ASSERT_TRUE(broker_interface_->GetHandle(0, &handle) == PP_ERROR_BADRESOURCE);

  // Connect hasn't been called so this should fail.
  ASSERT_TRUE(broker_interface_->GetHandle(broker, &handle) == PP_ERROR_FAILED);

  PASS();
}
