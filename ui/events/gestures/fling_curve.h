// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_FLING_CURVE_H_
#define UI_EVENTS_GESTURES_FLING_CURVE_H_

#include "base/time/time.h"
#include "ui/events/events_base_export.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {

// FlingCurve can be used to scroll a UI element suitable for touch screen-based
// flings.
class EVENTS_BASE_EXPORT FlingCurve {
 public:
  FlingCurve(const gfx::Vector2dF& velocity, base::TimeTicks start_timestamp);
  ~FlingCurve();

  gfx::Vector2dF GetScrollAmountAtTime(base::TimeTicks current_timestamp);
  base::TimeTicks start_timestamp() const { return start_timestamp_; }

 private:
  const float curve_duration_;
  const base::TimeTicks start_timestamp_;

  gfx::Vector2dF displacement_ratio_;
  gfx::Vector2dF cumulative_scroll_;
  base::TimeTicks last_timestamp_;
  float time_offset_;
  float position_offset_;

  DISALLOW_COPY_AND_ASSIGN(FlingCurve);
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_FLING_CURVE_H_
