// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/css/CSSAnimationData.h"

#include "core/animation/Timing.h"

namespace blink {

CSSAnimationData::CSSAnimationData() {
  name_list_.push_back(InitialName());
  iteration_count_list_.push_back(InitialIterationCount());
  direction_list_.push_back(InitialDirection());
  fill_mode_list_.push_back(InitialFillMode());
  play_state_list_.push_back(InitialPlayState());
}

CSSAnimationData::CSSAnimationData(const CSSAnimationData& other)
    : CSSTimingData(other),
      name_list_(other.name_list_),
      iteration_count_list_(other.iteration_count_list_),
      direction_list_(other.direction_list_),
      fill_mode_list_(other.fill_mode_list_),
      play_state_list_(other.play_state_list_) {}

const AtomicString& CSSAnimationData::InitialName() {
  DEFINE_STATIC_LOCAL(const AtomicString, name, ("none"));
  return name;
}

bool CSSAnimationData::AnimationsMatchForStyleRecalc(
    const CSSAnimationData& other) const {
  return name_list_ == other.name_list_ &&
         play_state_list_ == other.play_state_list_ &&
         iteration_count_list_ == other.iteration_count_list_ &&
         direction_list_ == other.direction_list_ &&
         fill_mode_list_ == other.fill_mode_list_ &&
         TimingMatchForStyleRecalc(other);
}

Timing CSSAnimationData::ConvertToTiming(size_t index) const {
  DCHECK_LT(index, name_list_.size());
  Timing timing = CSSTimingData::ConvertToTiming(index);

  timing.iteration_count = GetRepeated(iteration_count_list_, index);
  timing.direction = GetRepeated(direction_list_, index);
  timing.fill_mode = GetRepeated(fill_mode_list_, index);
  timing.AssertValid();
  return timing;
}

}  // namespace blink
