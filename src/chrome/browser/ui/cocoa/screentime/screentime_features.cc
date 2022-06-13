// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screentime/screentime_features.h"

#include <Availability.h>

namespace screentime {

const base::Feature kScreenTime{
    "ScreenTime",
    base::FEATURE_DISABLED_BY_DEFAULT,
};

bool IsScreenTimeEnabled() {
  if (__builtin_available(macOS 12.1, *)) {
    return base::FeatureList::IsEnabled(kScreenTime);
  }
  return false;
}

}  // namespace screentime
