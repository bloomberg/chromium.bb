// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/test_suite.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace test {

EventsTestSuite::EventsTestSuite(int argc, char** argv)
    : TestSuite(argc, argv) {}

EventsTestSuite::~EventsTestSuite() {}

}  // namespace test
}  // namespace ui
