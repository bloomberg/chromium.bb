// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"

namespace {

constexpr char kShowResultHistogramPrefix[] = "RelaunchNotification.ShowResult";
constexpr char kRecommendedSuffix[] = ".Recommended";
constexpr char kRequiredSuffix[] = ".Required";

}  // namespace

namespace relaunch_notification {

void RecordRecommendedShowResult(ShowResult show_result) {
  base::UmaHistogramEnumeration(
      std::string(kShowResultHistogramPrefix) + kRecommendedSuffix, show_result,
      ShowResult::kCount);
}

void RecordRequiredShowResult(ShowResult show_result) {
  base::UmaHistogramEnumeration(
      std::string(kShowResultHistogramPrefix) + kRequiredSuffix, show_result,
      ShowResult::kCount);
}

}  // namespace relaunch_notification
