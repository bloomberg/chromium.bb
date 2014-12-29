// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/mojo/test_mojo.h"

#include "mojo/public/cpp/system/message_pipe.h"

REGISTER_TEST_CASE(Mojo);

TestMojo::TestMojo(TestingInstance* instance) : TestCase(instance) { }
bool TestMojo::Init() {
  return true;
}

void TestMojo::RunTests(const std::string& filter) {
  RUN_TEST(CreateMessagePipe, filter);
}

std::string TestMojo::TestCreateMessagePipe() {
  mojo::MessagePipe pipe;
  PASS();
}
