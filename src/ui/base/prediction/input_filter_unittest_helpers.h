// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PREDICTION_INPUT_FILTER_UNITTEST_HELPERS_H_
#define UI_BASE_PREDICTION_INPUT_FILTER_UNITTEST_HELPERS_H_

#include "ui/base/prediction/input_filter.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace test {

constexpr double kEpsilon = 0.0001;

// Base class for predictor unit tests
class InputFilterTest : public testing::Test {
 public:
  InputFilterTest();
  ~InputFilterTest() override;

  void TestCloneFilter();

  void TestResetFilter();

 protected:
  std::unique_ptr<InputFilter> filter_;

  DISALLOW_COPY_AND_ASSIGN(InputFilterTest);
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_PREDICTION_INPUT_FILTER_UNITTEST_HELPERS_H_
