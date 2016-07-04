// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_SUITE_H_
#define SERVICES_UI_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_SUITE_H_

#include "base/macros.h"
#include "base/test/test_suite.h"

namespace ui {

class WindowServerTestSuite : public base::TestSuite {
 public:
  WindowServerTestSuite(int argc, char** argv);
  ~WindowServerTestSuite() override;

 protected:
  void Initialize() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowServerTestSuite);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_SUITE_H_
