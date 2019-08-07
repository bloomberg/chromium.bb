// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_TEST_UTILS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_TEST_UTILS_H_

#include "chrome/browser/notifications/scheduler/impression_history_tracker.h"
#include "chrome/browser/notifications/scheduler/impression_types.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_types.h"

namespace notifications {
namespace test {

// Flattened type state data used in test to generate client states.
struct ImpressionTestData {
  ImpressionTestData(SchedulerClientType type,
                     int current_max_daily_show,
                     std::vector<Impression> impressions,
                     base::Optional<SuppressionInfo> suppression_info);

  ImpressionTestData(const ImpressionTestData& other);
  ~ImpressionTestData();

  SchedulerClientType type;
  int current_max_daily_show;
  std::vector<Impression> impressions;
  base::Optional<SuppressionInfo> suppression_info;
};

// Add one impression test data into a client state.
void AddImpressionTestData(const ImpressionTestData& data,
                           ClientState* client_state);

// Adds impression test data into client states container.
void AddImpressionTestData(
    const std::vector<ImpressionTestData>& test_data,
    ImpressionHistoryTracker::ClientStates* client_states);

// Adds impression test data into client states container.
void AddImpressionTestData(
    const std::vector<ImpressionTestData>& test_data,
    std::vector<std::unique_ptr<ClientState>>* client_states);

}  // namespace test
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_TEST_UTILS_H_
