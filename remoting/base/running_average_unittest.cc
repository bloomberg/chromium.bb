// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/running_average.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

static const int64 kTestValues[] = { 10, 20, 30, 10, 25, 16, 15 };

// Average across a single element, i.e. just return the most recent.
TEST(RunningAverageTest, OneElementWindow) {
  RunningAverage running_average(1);
  EXPECT_EQ(0, running_average.Average());

  for (size_t i = 0; i < arraysize(kTestValues); ++i) {
    running_average.Record(kTestValues[i]);
    EXPECT_EQ(static_cast<double>(kTestValues[i]), running_average.Average());
  }
}

// Average the two most recent elements.
TEST(RunningAverageTest, TwoElementWindow) {
  RunningAverage running_average(2);
  EXPECT_EQ(0, running_average.Average());

  for (size_t i = 0; i < arraysize(kTestValues); ++i) {
    running_average.Record(kTestValues[i]);

    double expected = kTestValues[i];
    if (i > 0)
      expected = (expected + kTestValues[i-1]) / 2;

    EXPECT_EQ(expected, running_average.Average());
  }
}

// Average across all the elements if the window size exceeds the element count.
TEST(RunningAverageTest, LongWindow) {
  RunningAverage running_average(arraysize(kTestValues) + 1);
  EXPECT_EQ(0, running_average.Average());

  for (size_t i = 0; i < arraysize(kTestValues); ++i) {
    running_average.Record(kTestValues[i]);

    double expected = 0.0;
    for (size_t j = 0; j <= i; ++j)
      expected += kTestValues[j];
    expected /= i + 1;

    EXPECT_EQ(expected, running_average.Average());
  }
}

}  // namespace remoting
