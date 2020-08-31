// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_utils.h"

#include "base/cancelable_callback.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/transform.h"

namespace ui {

//TODO(avallee): Make this into a predicate and add some matrix pretty printing.
void CheckApproximatelyEqual(const gfx::Transform& lhs,
                             const gfx::Transform& rhs) {
  unsigned int errors = 0;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      EXPECT_FLOAT_EQ(lhs.matrix().get(i, j), rhs.matrix().get(i, j))
        << "(i, j) = (" << i << ", " << j << "), error count: " << ++errors;
    }
  }

  if (errors) {
    ADD_FAILURE() << "Expected matrix:\n"
                  << lhs.ToString() << "\n"
                  << "Actual matrix:\n"
                  << rhs.ToString();
  }
}

void CheckApproximatelyEqual(const gfx::Rect& lhs, const gfx::Rect& rhs) {
  EXPECT_FLOAT_EQ(lhs.x(), rhs.x());
  EXPECT_FLOAT_EQ(lhs.y(), rhs.y());
  EXPECT_FLOAT_EQ(lhs.width(), rhs.width());
  EXPECT_FLOAT_EQ(lhs.height(), rhs.height());
}

void CheckApproximatelyEqual(const gfx::RoundedCornersF& lhs,
                             const gfx::RoundedCornersF& rhs) {
  EXPECT_FLOAT_EQ(lhs.upper_left(), rhs.upper_left());
  EXPECT_FLOAT_EQ(lhs.upper_right(), rhs.upper_right());
  EXPECT_FLOAT_EQ(lhs.lower_left(), rhs.lower_left());
  EXPECT_FLOAT_EQ(lhs.lower_right(), rhs.lower_right());
}

bool WaitForNextFrameToBePresented(ui::Compositor* compositor,
                                   base::Optional<base::TimeDelta> timeout) {
  bool frames_presented = false;
  base::RunLoop runloop;
  base::CancelableOnceCallback<void(const gfx::PresentationFeedback&)>
      cancelable_callback(base::BindLambdaForTesting(
          [&](const gfx::PresentationFeedback& feedback) {
            frames_presented = true;
            runloop.Quit();
          }));
  compositor->RequestPresentationTimeForNextFrame(
      cancelable_callback.callback());

  base::Optional<base::OneShotTimer> timer;
  if (timeout.has_value()) {
    timer.emplace();
    timer->Start(FROM_HERE, timeout.value(), runloop.QuitClosure());
  }

  runloop.Run();

  return frames_presented;
}

}  // namespace ui
