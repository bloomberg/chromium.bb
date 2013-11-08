// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_TEST_SUITE_H_
#define UI_EVENTS_TEST_TEST_SUITE_H_

#include "base/compiler_specific.h"
#include "base/test/test_suite.h"

namespace ui {
namespace test {

class EventsTestSuite : public base::TestSuite {
 public:
  EventsTestSuite(int argc, char** argv);
  virtual ~EventsTestSuite();
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_TEST_SUITE_H_
