// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/test_suite.h"

// Don't use the multiprocess test harness. This test suite is fast enough and
// it makes the output more difficult to read.
int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  return test_suite.Run();
}
