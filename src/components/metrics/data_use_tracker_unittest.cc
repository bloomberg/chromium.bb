// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/data_use_tracker.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

const char kTodayStr[] = "2016-03-16";
const char kYesterdayStr[] = "2016-03-15";
const char kExpiredDateStr1[] = "2016-03-09";
const char kExpiredDateStr2[] = "2016-03-01";

class TestDataUsePrefService : public TestingPrefServiceSimple {
 public:
  TestDataUsePrefService() { DataUseTracker::RegisterPrefs(registry()); }

  TestDataUsePrefService(const TestDataUsePrefService&) = delete;
  TestDataUsePrefService& operator=(const TestDataUsePrefService&) = delete;

  void ClearDataUsePrefs() {
    ClearPref(metrics::prefs::kUserCellDataUse);
    ClearPref(metrics::prefs::kUmaCellDataUse);
  }
};

class FakeDataUseTracker : public DataUseTracker {
 public:
  FakeDataUseTracker(PrefService* local_state) : DataUseTracker(local_state) {}

  FakeDataUseTracker(const FakeDataUseTracker&) = delete;
  FakeDataUseTracker& operator=(const FakeDataUseTracker&) = delete;

  bool GetUmaWeeklyQuota(int* uma_weekly_quota_bytes) const override {
    *uma_weekly_quota_bytes = 200;
    return true;
  }

  bool GetUmaRatio(double* ratio) const override {
    *ratio = 0.05;
    return true;
  }

  base::Time GetCurrentMeasurementDate() const override {
    base::Time today_for_test;
    EXPECT_TRUE(base::Time::FromUTCString(kTodayStr, &today_for_test));
    return today_for_test;
  }

  std::string GetCurrentMeasurementDateAsString() const override {
    return kTodayStr;
  }
};

// Sets up data usage prefs with mock values so that UMA traffic is above the
// allowed ratio.
void SetPrefTestValuesOverRatio(PrefService* local_state) {
  base::DictionaryValue user_pref_dict;
  user_pref_dict.SetInteger(kTodayStr, 2 * 100);
  user_pref_dict.SetInteger(kYesterdayStr, 2 * 100);
  user_pref_dict.SetInteger(kExpiredDateStr1, 2 * 100);
  user_pref_dict.SetInteger(kExpiredDateStr2, 2 * 100);
  local_state->Set(prefs::kUserCellDataUse, user_pref_dict);

  base::DictionaryValue uma_pref_dict;
  uma_pref_dict.SetInteger(kTodayStr, 50);
  uma_pref_dict.SetInteger(kYesterdayStr, 50);
  uma_pref_dict.SetInteger(kExpiredDateStr1, 50);
  uma_pref_dict.SetInteger(kExpiredDateStr2, 50);
  local_state->Set(prefs::kUmaCellDataUse, uma_pref_dict);
}

// Sets up data usage prefs with mock values which can be valid.
void SetPrefTestValuesValidRatio(PrefService* local_state) {
  base::DictionaryValue user_pref_dict;
  user_pref_dict.SetInteger(kTodayStr, 100 * 100);
  user_pref_dict.SetInteger(kYesterdayStr, 100 * 100);
  user_pref_dict.SetInteger(kExpiredDateStr1, 100 * 100);
  user_pref_dict.SetInteger(kExpiredDateStr2, 100 * 100);
  local_state->Set(prefs::kUserCellDataUse, user_pref_dict);

  // Should be 4% of user traffic
  base::DictionaryValue uma_pref_dict;
  uma_pref_dict.SetInteger(kTodayStr, 4 * 100);
  uma_pref_dict.SetInteger(kYesterdayStr, 4 * 100);
  uma_pref_dict.SetInteger(kExpiredDateStr1, 4 * 100);
  uma_pref_dict.SetInteger(kExpiredDateStr2, 4 * 100);
  local_state->Set(prefs::kUmaCellDataUse, uma_pref_dict);
}

}  // namespace

TEST(DataUseTrackerTest, CheckUpdateUsagePref) {
  TestDataUsePrefService local_state;
  FakeDataUseTracker data_use_tracker(&local_state);
  local_state.ClearDataUsePrefs();

  data_use_tracker.UpdateMetricsUsagePrefsInternal(2 * 100, true, false);
  EXPECT_EQ(2 * 100, local_state.GetDictionary(prefs::kUserCellDataUse)
                         ->FindIntKey(kTodayStr));
  EXPECT_FALSE(
      local_state.GetDictionary(prefs::kUmaCellDataUse)->FindIntKey(kTodayStr));

  data_use_tracker.UpdateMetricsUsagePrefsInternal(100, true, true);
  EXPECT_EQ(3 * 100, local_state.GetDictionary(prefs::kUserCellDataUse)
                         ->FindIntKey(kTodayStr));
  EXPECT_EQ(
      100,
      local_state.GetDictionary(prefs::kUmaCellDataUse)->FindIntKey(kTodayStr));
}

TEST(DataUseTrackerTest, CheckRemoveExpiredEntries) {
  TestDataUsePrefService local_state;
  FakeDataUseTracker data_use_tracker(&local_state);
  local_state.ClearDataUsePrefs();
  SetPrefTestValuesOverRatio(&local_state);
  data_use_tracker.RemoveExpiredEntries();

  EXPECT_FALSE(local_state.GetDictionary(prefs::kUserCellDataUse)
                   ->FindIntKey(kExpiredDateStr1));
  EXPECT_FALSE(local_state.GetDictionary(prefs::kUmaCellDataUse)
                   ->FindIntKey(kExpiredDateStr1));

  EXPECT_FALSE(local_state.GetDictionary(prefs::kUserCellDataUse)
                   ->FindIntKey(kExpiredDateStr2));
  EXPECT_FALSE(local_state.GetDictionary(prefs::kUmaCellDataUse)
                   ->FindIntKey(kExpiredDateStr2));

  EXPECT_EQ(2 * 100, local_state.GetDictionary(prefs::kUserCellDataUse)
                         ->FindIntKey(kTodayStr));
  EXPECT_EQ(
      50,
      local_state.GetDictionary(prefs::kUmaCellDataUse)->FindIntKey(kTodayStr));

  EXPECT_EQ(2 * 100, local_state.GetDictionary(prefs::kUserCellDataUse)
                         ->FindIntKey(kYesterdayStr));
  EXPECT_EQ(50, local_state.GetDictionary(prefs::kUmaCellDataUse)
                    ->FindIntKey(kYesterdayStr));
}

TEST(DataUseTrackerTest, CheckComputeTotalDataUse) {
  TestDataUsePrefService local_state;
  FakeDataUseTracker data_use_tracker(&local_state);
  local_state.ClearDataUsePrefs();
  SetPrefTestValuesOverRatio(&local_state);

  int user_data_use =
      data_use_tracker.ComputeTotalDataUse(prefs::kUserCellDataUse);
  EXPECT_EQ(8 * 100, user_data_use);
  int uma_data_use =
      data_use_tracker.ComputeTotalDataUse(prefs::kUmaCellDataUse);
  EXPECT_EQ(4 * 50, uma_data_use);
}

TEST(DataUseTrackerTest, CheckShouldUploadLogOnCellular) {
  TestDataUsePrefService local_state;
  FakeDataUseTracker data_use_tracker(&local_state);
  local_state.ClearDataUsePrefs();
  SetPrefTestValuesOverRatio(&local_state);

  bool can_upload = data_use_tracker.ShouldUploadLogOnCellular(50);
  EXPECT_TRUE(can_upload);
  can_upload = data_use_tracker.ShouldUploadLogOnCellular(100);
  EXPECT_TRUE(can_upload);
  can_upload = data_use_tracker.ShouldUploadLogOnCellular(150);
  EXPECT_FALSE(can_upload);

  local_state.ClearDataUsePrefs();
  SetPrefTestValuesValidRatio(&local_state);
  can_upload = data_use_tracker.ShouldUploadLogOnCellular(100);
  EXPECT_TRUE(can_upload);
  // this is about 0.49%
  can_upload = data_use_tracker.ShouldUploadLogOnCellular(200);
  EXPECT_TRUE(can_upload);
  can_upload = data_use_tracker.ShouldUploadLogOnCellular(300);
  EXPECT_FALSE(can_upload);
}

}  // namespace metrics
