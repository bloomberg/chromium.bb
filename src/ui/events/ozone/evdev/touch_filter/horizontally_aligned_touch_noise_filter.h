// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_HORIZONTALLY_ALIGNED_TOUCH_NOISE_FILTER_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_HORIZONTALLY_ALIGNED_TOUCH_NOISE_FILTER_H_

#include "base/macros.h"
#include "ui/events/ozone/evdev/touch_filter/touch_filter.h"

namespace ui {

class HorizontallyAlignedTouchNoiseFilter : public TouchFilter {
 public:
  HorizontallyAlignedTouchNoiseFilter() {}
  ~HorizontallyAlignedTouchNoiseFilter() override {}

  // TouchFilter:
  void Filter(const std::vector<InProgressTouchEvdev>& touches,
              base::TimeTicks time,
              std::bitset<kNumTouchEvdevSlots>* slots_with_noise) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HorizontallyAlignedTouchNoiseFilter);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_HORIZONTALLY_ALIGNED_TOUCH_NOISE_FILTER_H_
