// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_inmemory_strike_database.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

TestInMemoryStrikeDatabase::TestInMemoryStrikeDatabase() = default;

TestInMemoryStrikeDatabase::~TestInMemoryStrikeDatabase() = default;

int TestInMemoryStrikeDatabase::AddStrikes(int strikes_increase,
                                           const std::string& key) {
  DCHECK_GT(strikes_increase, 0);
  int num_strikes =
      strike_map_cache_.count(key)  // Cache has entry for |key|.
          ? strike_map_cache_[key].num_strikes() + strikes_increase
          : strikes_increase;
  SetStrikeData(key, num_strikes);
  return num_strikes;
}

int TestInMemoryStrikeDatabase::RemoveStrikes(int strikes_decrease,
                                              const std::string& key) {
  int num_strikes = GetStrikes(key);
  num_strikes = std::max(0, num_strikes - strikes_decrease);
  SetStrikeData(key, num_strikes);
  return num_strikes;
}

int TestInMemoryStrikeDatabase::GetStrikes(const std::string& key) {
  auto iter = strike_map_cache_.find(key);
  return (iter != strike_map_cache_.end()) ? iter->second.num_strikes() : 0;
}

void TestInMemoryStrikeDatabase::ClearStrikes(const std::string& key) {
  strike_map_cache_.erase(key);
}

std::map<std::string, StrikeData>&
TestInMemoryStrikeDatabase::GetStrikeCache() {
  return strike_map_cache_;
}

std::vector<std::string> TestInMemoryStrikeDatabase::GetAllStrikeKeysForProject(
    const std::string& project_prefix) {
  std::vector<std::string> project_keys;
  for (std::pair<std::string, StrikeData> entry : strike_map_cache_) {
    if (entry.first.find(project_prefix) == 0) {
      project_keys.push_back(entry.first);
    }
  }
  return project_keys;
}

void TestInMemoryStrikeDatabase::ClearAllStrikesForProject(
    const std::string& project_prefix) {
  ClearStrikesForKeys(GetAllStrikeKeysForProject(project_prefix));
}

void TestInMemoryStrikeDatabase::ClearStrikesForKeys(
    const std::vector<std::string>& keys_to_remove) {
  for (const auto& key : keys_to_remove) {
    strike_map_cache_.erase(key);
  }
}

void TestInMemoryStrikeDatabase::ClearAllStrikes() {
  strike_map_cache_.clear();
}

std::string TestInMemoryStrikeDatabase::GetPrefixFromKey(
    const std::string& key) const {
  return key.substr(0, key.find(kKeyDeliminator));
}

void TestInMemoryStrikeDatabase::SetStrikeData(const std::string& key,
                                               int num_strikes) {
  if (num_strikes == 0) {
    ClearStrikes(key);
    return;
  }
  StrikeData data;
  data.set_num_strikes(num_strikes);
  data.set_last_update_timestamp(
      AutofillClock::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  strike_map_cache_[key] = data;
}

}  // namespace autofill
