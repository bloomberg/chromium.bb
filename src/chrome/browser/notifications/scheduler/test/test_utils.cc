// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/test/test_utils.h"

#include <sstream>
#include <utility>

#include "chrome/browser/notifications/scheduler/notification_data.h"
#include "chrome/browser/notifications/scheduler/notification_entry.h"

namespace notifications {
namespace test {

ImpressionTestData::ImpressionTestData(
    SchedulerClientType type,
    int current_max_daily_show,
    std::vector<Impression> impressions,
    base::Optional<SuppressionInfo> suppression_info)
    : type(type),
      current_max_daily_show(current_max_daily_show),
      impressions(std::move(impressions)),
      suppression_info(std::move(suppression_info)) {}

ImpressionTestData::ImpressionTestData(const ImpressionTestData& other) =
    default;

ImpressionTestData::~ImpressionTestData() = default;

void AddImpressionTestData(const ImpressionTestData& data,
                           ClientState* client_state) {
  DCHECK(client_state);
  client_state->type = data.type;
  client_state->current_max_daily_show = data.current_max_daily_show;
  for (const auto& impression : data.impressions) {
    client_state->impressions.emplace_back(impression);
  }
  client_state->suppression_info = data.suppression_info;
}

void AddImpressionTestData(
    const std::vector<ImpressionTestData>& test_data,
    ImpressionHistoryTracker::ClientStates* client_states) {
  DCHECK(client_states);
  for (const auto& test_data : test_data) {
    auto client_state = std::make_unique<ClientState>();
    AddImpressionTestData(test_data, client_state.get());
    client_states->emplace(test_data.type, std::move(client_state));
  }
}

void AddImpressionTestData(
    const std::vector<ImpressionTestData>& test_data,
    std::vector<std::unique_ptr<ClientState>>* client_states) {
  DCHECK(client_states);
  for (const auto& test_data : test_data) {
    auto client_state = std::make_unique<ClientState>();
    AddImpressionTestData(test_data, client_state.get());
    client_states->emplace_back(std::move(client_state));
  }
}

std::string DebugString(NotificationData* data) {
  DCHECK(data);
  std::ostringstream stream;
  stream << " Notification Data: \n id:" << data->id
         << " \n title:" << data->title << "\n message:" << data->message
         << " \n icon_id:" << data->icon_uuid << " \n url:" << data->url;
  return stream.str();
}

std::string DebugString(NotificationEntry* entry) {
  DCHECK(entry);
  std::ostringstream stream;
  stream << "NotificationEntry: \n  type: " << static_cast<int>(entry->type)
         << " \n guid: " << entry->guid << "\n create_time: "
         << entry->create_time.ToDeltaSinceWindowsEpoch().InMicroseconds()
         << " \n notification_data:" << DebugString(&entry->notification_data)
         << " \n schedule params: priority:"
         << static_cast<int>(entry->schedule_params.priority);
  return stream.str();
}

}  // namespace test
}  // namespace notifications
