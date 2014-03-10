// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_TESTS_TEST_UTILS_H_
#define SANDBOX_LINUX_TESTS_TEST_UTILS_H_

#include "base/basictypes.h"

namespace sandbox {

// This class provide small helpers to help writing tests.
class TestUtils {
 public:
  static bool CurrentProcessHasChildren();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TestUtils);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_TESTS_TEST_UTILS_H_
