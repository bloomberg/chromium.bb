// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/prefs.h"

#include <utility>

#include "base/value_conversions.h"
#include "base/values.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace prefs {

std::vector<int> GetThrottlerRequestCounts(PrefService* pref_service) {
  std::vector<int> result;
  const auto& value_list =
      pref_service->GetList(kThrottlerRequestCountListPrefName)->GetList();
  for (const base::Value& value : value_list) {
    result.push_back(value.is_int() ? value.GetInt() : 0);
  }
  return result;
}

void SetThrottlerRequestCounts(std::vector<int> request_counts,
                               PrefService* pref_service) {
  std::vector<base::Value> value_list;
  for (int count : request_counts) {
    value_list.push_back(base::Value(count));
  }

  pref_service->Set(kThrottlerRequestCountListPrefName,
                    base::Value(std::move(value_list)));
}

base::Time GetLastRequestTime(PrefService* pref_service) {
  return pref_service->GetTime(kThrottlerLastRequestTime);
}

void SetLastRequestTime(base::Time request_time, PrefService* pref_service) {
  return pref_service->SetTime(kThrottlerLastRequestTime, request_time);
}

DebugStreamData GetDebugStreamData(PrefService* pref_service) {
  return DeserializeDebugStreamData(pref_service->GetString(kDebugStreamData))
      .value_or(DebugStreamData());
}

void SetDebugStreamData(const DebugStreamData& data,
                        PrefService* pref_service) {
  pref_service->SetString(kDebugStreamData, SerializeDebugStreamData(data));
}

void SetRequestSchedule(const RequestSchedule& schedule,
                        PrefService* pref_service) {
  pref_service->Set(kRequestSchedule, RequestScheduleToValue(schedule));
}

RequestSchedule GetRequestSchedule(PrefService* pref_service) {
  return RequestScheduleFromValue(*pref_service->Get(kRequestSchedule));
}

}  // namespace prefs

}  // namespace feed
