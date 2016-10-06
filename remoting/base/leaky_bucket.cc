// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/leaky_bucket.h"

#include "base/logging.h"

namespace remoting {

LeakyBucket::LeakyBucket(int depth, int rate)
    : depth_(depth),
      rate_(rate),
      current_level_(0),
      level_updated_time_(base::TimeTicks::Now()) {}

LeakyBucket::~LeakyBucket() {}

bool LeakyBucket::RefillOrSpill(int drops, base::TimeTicks now) {
  UpdateLevel(now);

  int new_level = current_level_ + drops;
  if (depth_ >= 0 && new_level > depth_)
    return false;
  current_level_ = new_level;
  return true;
}

base::TimeTicks LeakyBucket::GetEmptyTime() {
  return level_updated_time_ +
         base::TimeDelta::FromMicroseconds(
             base::TimeTicks::kMicrosecondsPerSecond * current_level_ / rate_);
}

void LeakyBucket::UpdateRate(int new_rate, base::TimeTicks now) {
  UpdateLevel(now);
  rate_ = new_rate;
}

void LeakyBucket::UpdateLevel(base::TimeTicks now) {
  current_level_ -= rate_ * (now - level_updated_time_).InMicroseconds() /
            base::TimeTicks::kMicrosecondsPerSecond;
  if (current_level_ < 0)
    current_level_ = 0;
  level_updated_time_ = now;
}

}  // namespace remoting
