// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_CONFIG_H_
#define CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_CONFIG_H_

#include <vector>

#include "base/time/time.h"
#include "chrome/browser/feature_guide/notifications/feature_type.h"

namespace feature_guide {

// Contains various finch configuration params used by the feature notification
// guide.
struct Config {
  Config();
  ~Config();

  Config(const Config& other);
  Config& operator=(const Config& other);

  // The list of features enabled via finch for showing feature notifications.
  std::vector<FeatureType> enabled_features;

  // Relative start time for launching the notification.
  base::TimeDelta notification_deliver_time_delta;
};

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_CONFIG_H_
