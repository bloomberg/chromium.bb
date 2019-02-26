// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_METRICS_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_METRICS_H_

namespace relaunch_notification {

// The result of an attempt to show a relaunch notification dialog. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class ShowResult {
  kShown = 0,
  kUnknownNotShownReason = 1,
  kBackgroundModeNoWindows = 2,
  kCount
};

// Record to UMA showing a relaunch recommended notification with the given
// |show_result|.
void RecordRecommendedShowResult(ShowResult show_result);

// Record to UMA showing a relaunch required notification with the given
// |show_result|.
void RecordRequiredShowResult(ShowResult show_result);

}  // namespace relaunch_notification

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_METRICS_H_
