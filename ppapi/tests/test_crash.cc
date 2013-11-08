// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "ppapi/tests/test_crash.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Crash);

TestCrash::TestCrash(TestingInstance* instance)
    : TestCase(instance) {
}

void TestCrash::RunTests(const std::string& filter) {
  RUN_TEST(CrashInCallOnMainThread, filter);
}

std::string TestCrash::TestCrashInCallOnMainThread() {
  // The tests are run via CallOnMainThread (see DidChangeView in
  // TestingInstance), so we just crash here.
  abort();
  PASS();
}

