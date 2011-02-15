// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_transport.h"

#include <string.h>

#include <string>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_transport_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Transport);

bool TestTransport::Init() {
  transport_interface_ = reinterpret_cast<PPB_Transport_Dev const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_TRANSPORT_DEV_INTERFACE));
  return transport_interface_ && InitTestingInterface();
}

void TestTransport::RunTest() {
  RUN_TEST(FirstTransport);
  // TODO(juberti): more Transport tests here...
}

std::string TestTransport::TestFirstTransport() {
  // TODO(juberti): actual test
  PASS();
}
