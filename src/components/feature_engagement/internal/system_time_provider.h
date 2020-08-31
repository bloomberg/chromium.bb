// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SYSTEM_TIME_PROVIDER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SYSTEM_TIME_PROVIDER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "components/feature_engagement/internal/time_provider.h"

namespace feature_engagement {

// A TimeProvider that uses the system time.
class SystemTimeProvider : public TimeProvider {
 public:
  SystemTimeProvider();
  ~SystemTimeProvider() override;

  // TimeProvider implementation.
  uint32_t GetCurrentDay() const override;

 protected:
  // Return the current time.
  // virtual for testing.
  virtual base::Time Now() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemTimeProvider);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_SYSTEM_TIME_PROVIDER_H_
