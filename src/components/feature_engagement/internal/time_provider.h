// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TIME_PROVIDER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TIME_PROVIDER_H_

#include <stdint.h>

#include "base/macros.h"

namespace feature_engagement {

// A TimeProvider provides functionality related to time.
class TimeProvider {
 public:
  virtual ~TimeProvider() = default;

  // Returns the number of days since epoch (1970-01-01) in the local timezone.
  virtual uint32_t GetCurrentDay() const = 0;

 protected:
  TimeProvider() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeProvider);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TIME_PROVIDER_H_
