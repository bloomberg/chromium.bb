// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_TEST_UTILS_H_
#define UI_COMPOSITOR_TEST_TEST_UTILS_H_

#include "base/optional.h"
#include "base/time/time.h"

namespace gfx {
class Rect;
class RoundedCornersF;
class Transform;
}

namespace ui {

class Compositor;

void CheckApproximatelyEqual(const gfx::Transform& lhs,
                             const gfx::Transform& rhs);
void CheckApproximatelyEqual(const gfx::Rect& lhs, const gfx::Rect& rhs);
void CheckApproximatelyEqual(const gfx::RoundedCornersF& lhs,
                             const gfx::RoundedCornersF& rhs);

// Runs a RunLoop until the next frame is presented with an optional timeout.
// Returns true if a frame is presented. Otherwise, returns false.
bool WaitForNextFrameToBePresented(
    ui::Compositor* compositor,
    base::Optional<base::TimeDelta> timeout = base::nullopt) WARN_UNUSED_RESULT;

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_TEST_UTILS_H_
