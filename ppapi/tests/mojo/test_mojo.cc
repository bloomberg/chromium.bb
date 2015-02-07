// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/mojo/test_mojo.h"

#include "third_party/mojo/src/mojo/public/cpp/system/message_pipe.h"

REGISTER_TEST_CASE(Mojo);

TestMojo::TestMojo(TestingInstance* instance) : TestCase(instance) { }
bool TestMojo::Init() {
  return true;
}

void TestMojo::RunTests(const std::string& filter) {
  RUN_TEST(CreateMessagePipe, filter);
}

std::string TestMojo::TestCreateMessagePipe() {
  MojoHandle h0;
  MojoHandle h1;
  ASSERT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(NULL, &h0, &h1));
  PASS();
}
