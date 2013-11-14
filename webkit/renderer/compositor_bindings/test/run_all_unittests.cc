// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"

namespace {

int RunTestSuite(TestSuite* test_suite) {
  base::MessageLoop message_loop;
  return test_suite->Run();
}

}  // namespace

int main(int argc, char** argv) {
  TestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&RunTestSuite,
                             base::Unretained(&test_suite)));
}
