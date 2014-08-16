// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/leaky_bucket.h"

#include "base/logging.h"

namespace remoting {

LeakyBucket::LeakyBucket(double depth, double rate)
    : depth_(depth),
      rate_(rate),
      level_(0.0),
      last_update_(base::TimeTicks::Now()) {
}

LeakyBucket::~LeakyBucket() {
}

base::TimeDelta LeakyBucket::AddPacket(int size) {
  UpdateLevel();

  double new_level = level_ + size;
  if (new_level > depth_)
    return base::TimeDelta::Max();

  base::TimeDelta result = base::TimeDelta::FromSecondsD(level_ / rate_);
  level_ = new_level;
  return result;
}

void LeakyBucket::UpdateLevel() {
  base::TimeTicks now = base::TimeTicks::Now();
  level_ -= rate_ * (now - last_update_).InSecondsF();
  if (level_ < 0.0)
    level_ = 0.0;
  last_update_ = now;
}

}  // namespace remoting
