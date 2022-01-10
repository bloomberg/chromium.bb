// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_safe_mode_constants.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {
namespace {

using ::variations::SetUpExtendedSafeModeExperiment;

const wchar_t kDummyWindowsRegistryKey[] = L"";

// Creates and returns well-formed beacon file contents with the given values.
std::string CreateWellFormedBeaconFileContents(bool exited_cleanly,
                                               int crash_streak) {
  const std::string exited_cleanly_str = exited_cleanly ? "true" : "false";
  return base::StringPrintf(
      "{\n"
      "  \"user_experience_metrics.stability.exited_cleanly\": %s,\n"
      "  \"variations_crash_streak\": %s\n"
      "}",
      exited_cleanly_str.data(), base::NumberToString(crash_streak).data());
}

}  // namespace

class TestCleanExitBeacon : public CleanExitBeacon {
 public:
  explicit TestCleanExitBeacon(
      PrefService* local_state,
      const base::FilePath& user_data_dir = base::FilePath())
      : CleanExitBeacon(kDummyWindowsRegistryKey,
                        user_data_dir,
                        local_state,
                        version_info::Channel::UNKNOWN) {
    Initialize();
  }

  ~TestCleanExitBeacon() override = default;
};

class CleanExitBeaconTest : public ::testing::Test {
 public:
  void SetUp() override {
    metrics::CleanExitBeacon::RegisterPrefs(prefs_.registry());
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
  }

 protected:
  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple prefs_;
  base::ScopedTempDir user_data_dir_;

 private:
  base::test::TaskEnvironment task_environment_;
};

class BeaconFileTest : public testing::WithParamInterface<std::string>,
                       public CleanExitBeaconTest {};

struct BadBeaconTestParams {
  const std::string test_name;
  bool beacon_file_exists;
  const std::string beacon_file_contents;
};

// Used for testing beacon files that are not well-formed, do not exist, etc.
class BadBeaconFileTest
    : public testing::WithParamInterface<BadBeaconTestParams>,
      public CleanExitBeaconTest {};

struct BeaconConsistencyTestParams {
  // Inputs:
  const std::string test_name;
  absl::optional<bool> beacon_file_beacon_value;
  absl::optional<bool> platform_specific_beacon_value;
  absl::optional<bool> local_state_beacon_value;
  // Result:
  CleanExitBeaconConsistency expected_consistency;
};

// Used for testing the logic that emits CleanExitBeaconConsistency to
// histograms.
#if defined(OS_IOS)
class BackupBeaconConsistencyTest
    : public testing::WithParamInterface<BeaconConsistencyTestParams>,
      public CleanExitBeaconTest {};
#endif  // defined(OS_IOS)
class BeaconFileConsistencyTest
    : public testing::WithParamInterface<BeaconConsistencyTestParams>,
      public CleanExitBeaconTest {};

// Verify that the crash streak metric is 0 when default pref values are used.
TEST_F(CleanExitBeaconTest, CrashStreakMetricWithDefaultPrefs) {
  CleanExitBeacon::ResetStabilityExitedCleanlyForTesting(&prefs_);
  TestCleanExitBeacon clean_exit_beacon(&prefs_);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 0,
                                       1);
}

// Verify that the crash streak metric is 0 when prefs are explicitly set to
// their defaults.
TEST_F(CleanExitBeaconTest, CrashStreakMetricWithNoCrashes) {
  // The default value for kStabilityExitedCleanly is true, but defaults can
  // change, so we explicitly set it to true here. Similarly, we explicitly set
  // kVariationsCrashStreak to 0.
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, true);
  prefs_.SetInteger(variations::prefs::kVariationsCrashStreak, 0);
  TestCleanExitBeacon clean_exit_beacon(&prefs_);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 0,
                                       1);
}

// Verify that the crash streak metric is correctly recorded when there is a
// non-zero crash streak.
TEST_F(CleanExitBeaconTest, CrashStreakMetricWithSomeCrashes) {
  // The default value for kStabilityExitedCleanly is true, but defaults can
  // change, so we explicitly set it to true here.
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, true);
  prefs_.SetInteger(variations::prefs::kVariationsCrashStreak, 1);
  TestCleanExitBeacon clean_exit_beacon(&prefs_);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 1,
                                       1);
}

// Verify that the crash streak is correctly incremented and recorded when the
// last Chrome session did not exit cleanly.
TEST_F(CleanExitBeaconTest, CrashIncrementsCrashStreak) {
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, false);
  prefs_.SetInteger(variations::prefs::kVariationsCrashStreak, 1);
  TestCleanExitBeacon clean_exit_beacon(&prefs_);
  EXPECT_EQ(prefs_.GetInteger(variations::prefs::kVariationsCrashStreak), 2);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 2,
                                       1);
}

// Verify that the crash streak is correctly incremented and recorded when the
// last Chrome session did not exit cleanly and the default crash streak value
// is used.
TEST_F(CleanExitBeaconTest,
       CrashIncrementsCrashStreakWithDefaultCrashStreakPref) {
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, false);
  TestCleanExitBeacon clean_exit_beacon(&prefs_);
  EXPECT_EQ(prefs_.GetInteger(variations::prefs::kVariationsCrashStreak), 1);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 1,
                                       1);
}

// Verify that (a) the client is excluded from the Extended Variations Safe Mode
// experiment and (b) no attempt is made to read the beacon file when no user
// data dir is provided.
TEST_F(CleanExitBeaconTest, InitWithoutUserDataDir) {
  TestCleanExitBeacon beacon(&prefs_, base::FilePath());
  EXPECT_FALSE(
      base::FieldTrialList::IsTrialActive(variations::kExtendedSafeModeTrial));
  histogram_tester_.ExpectTotalCount(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", 0);
}

// Verify that the beacon file is not read when the client is not in the
// SignalAndWriteViaFileUtil experiment group. It is possible for a client to
// have the file and to not be in the SignalAndWriteViaFileUtil group when the
// client was in the group in a previous session and then switched groups, e.g.
// via kResetVariationState.
TEST_F(CleanExitBeaconTest, FileIgnoredByControlGroup) {
  // Deliberately set the prefs so that we can later verify that their values
  // have not changed.
  int expected_crash_streak = 0;
  prefs_.SetInteger(variations::prefs::kVariationsCrashStreak,
                    expected_crash_streak);
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, true);

  // Prepare a well-formed beacon file, which we expect to be ignored. (If it
  // were used, then the prefs' values would change.)
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  ASSERT_LT(0, base::WriteFile(temp_beacon_file_path,
                               CreateWellFormedBeaconFileContents(
                                   /*exited_cleanly=*/false, /*crash_streak=*/2)
                                   .data()));
  const std::string group_name = variations::kControlGroup;
  SetUpExtendedSafeModeExperiment(group_name);
  ASSERT_EQ(group_name, base::FieldTrialList::FindFullName(
                            variations::kExtendedSafeModeTrial));
  TestCleanExitBeacon beacon(&prefs_, user_data_dir_path);

  EXPECT_TRUE(prefs_.GetBoolean(prefs::kStabilityExitedCleanly));
  EXPECT_EQ(prefs_.GetInteger(variations::prefs::kVariationsCrashStreak),
            expected_crash_streak);
  histogram_tester_.ExpectTotalCount(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BadBeaconFileTest,
    ::testing::Values(
        BadBeaconTestParams{.test_name = "NoVariationsFile",
                            .beacon_file_exists = false,
                            .beacon_file_contents = ""},
        BadBeaconTestParams{.test_name = "EmptyVariationsFile",
                            .beacon_file_exists = true,
                            .beacon_file_contents = ""},
        BadBeaconTestParams{.test_name = "NotDictionary",
                            .beacon_file_exists = true,
                            .beacon_file_contents = "{abc123"},
        BadBeaconTestParams{.test_name = "EmptyDictionary",
                            .beacon_file_exists = true,
                            .beacon_file_contents = "{}"},
        BadBeaconTestParams{
            .test_name = "MissingCrashStreak",
            .beacon_file_exists = true,
            .beacon_file_contents =
                "{\"user_experience_metrics.stability.exited_cleanly\": true}"},
        BadBeaconTestParams{
            .test_name = "MissingBeacon",
            .beacon_file_exists = true,
            .beacon_file_contents = "{\"variations_crash_streak\": 1}"}),
    [](const ::testing::TestParamInfo<BadBeaconTestParams>& params) {
      return params.param.test_name;
    });

// Verify that the inability to get the beacon file's contents for a plethora of
// reasons (a) doesn't crash and (b) correctly records the
// GotVariationsFileContents metric.
TEST_P(BadBeaconFileTest, InitWithUnusableBeaconFile) {
  SetUpExtendedSafeModeExperiment(variations::kSignalAndWriteViaFileUtilGroup);
  BadBeaconTestParams params = GetParam();

  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  if (params.beacon_file_exists) {
    const base::FilePath temp_beacon_file_path =
        user_data_dir_path.Append(variations::kVariationsFilename);
    ASSERT_LT(0, base::WriteFile(temp_beacon_file_path,
                                 params.beacon_file_contents.data()));
  }

  TestCleanExitBeacon beacon(&prefs_, user_data_dir_path);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", false, 1);
}

// TODO(crbug/1248239): Enable these tests on Android when the Extended
// Variations Safe Mode experiment is fully enabled on Android Chrome.
#if !defined(OS_ANDROID)
// Verify that successfully reading the beacon file's contents results in
// correctly (a) setting the |did_previous_session_exit_cleanly_| field and (b)
// recording metrics when the last session exited cleanly.
TEST_F(CleanExitBeaconTest, InitWithBeaconFile) {
  SetUpExtendedSafeModeExperiment(variations::kSignalAndWriteViaFileUtilGroup);
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  const int num_crashes = 2;
  ASSERT_LT(0, base::WriteFile(
                   temp_beacon_file_path,
                   CreateWellFormedBeaconFileContents(
                       /*exited_cleanly=*/true, /*crash_streak=*/num_crashes)
                       .data()));

  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", true, 1);
  EXPECT_TRUE(clean_exit_beacon.exited_cleanly());
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes",
                                       num_crashes, 1);
}

// Verify that successfully reading the beacon file's contents results in
// correctly (a) setting the |did_previous_session_exit_cleanly_| field and (b)
// recording metrics when the last session did not exit cleanly.
TEST_F(CleanExitBeaconTest, InitWithCrashAndBeaconFile) {
  SetUpExtendedSafeModeExperiment(variations::kSignalAndWriteViaFileUtilGroup);
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  const int last_session_num_crashes = 2;
  ASSERT_LT(0, base::WriteFile(temp_beacon_file_path,
                               CreateWellFormedBeaconFileContents(
                                   /*exited_cleanly=*/false,
                                   /*crash_streak=*/last_session_num_crashes)
                                   .data()));

  const int updated_num_crashes = last_session_num_crashes + 1;
  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", true, 1);
  EXPECT_FALSE(clean_exit_beacon.exited_cleanly());
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes",
                                       updated_num_crashes, 1);
}
#endif  // !defined(OS_ANDROID)

// The below CleanExitBeaconTest.BeaconState*ExtendedSafeMode tests verify that
// the logic for recording UMA.CleanExitBeacon.BeaconFileConsistency is correct
// for clients in the SignalAndWriteViaFileUtil group.
INSTANTIATE_TEST_SUITE_P(
    All,
    BeaconFileConsistencyTest,
    ::testing::Values(
        BeaconConsistencyTestParams{
            .test_name = "MissingMissing",
            .expected_consistency =
                CleanExitBeaconConsistency::kMissingMissing},
        BeaconConsistencyTestParams{
            .test_name = "MissingClean",
            .local_state_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kMissingClean},
        BeaconConsistencyTestParams{
            .test_name = "MissingDirty",
            .local_state_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kMissingDirty},
        BeaconConsistencyTestParams{
            .test_name = "CleanMissing",
            .beacon_file_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kCleanMissing},
        BeaconConsistencyTestParams{
            .test_name = "DirtyMissing",
            .beacon_file_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyMissing},
        BeaconConsistencyTestParams{
            .test_name = "CleanClean",
            .beacon_file_beacon_value = true,
            .local_state_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kCleanClean},
        BeaconConsistencyTestParams{
            .test_name = "CleanDirty",
            .beacon_file_beacon_value = true,
            .local_state_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kCleanDirty},
        BeaconConsistencyTestParams{
            .test_name = "DirtyClean",
            .beacon_file_beacon_value = false,
            .local_state_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyClean},
        BeaconConsistencyTestParams{
            .test_name = "DirtyDirty",
            .beacon_file_beacon_value = false,
            .local_state_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyDirty}),
    [](const ::testing::TestParamInfo<BeaconConsistencyTestParams>& params) {
      return params.param.test_name;
    });

TEST_P(BeaconFileConsistencyTest, BeaconConsistency) {
  // Verify that the beacon file is not present. Unless set below, this beacon
  // is considered missing.
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  ASSERT_FALSE(base::PathExists(temp_beacon_file_path));
  // Clear the Local State beacon. Unless set below, it is also considered
  // missing.
  prefs_.ClearPref(prefs::kStabilityExitedCleanly);

  BeaconConsistencyTestParams params = GetParam();
  if (params.beacon_file_beacon_value) {
    ASSERT_LT(
        0, base::WriteFile(
               temp_beacon_file_path,
               CreateWellFormedBeaconFileContents(
                   /*exited_cleanly=*/params.beacon_file_beacon_value.value(),
                   /*crash_streak=*/0)
                   .data()));
  }
  if (params.local_state_beacon_value) {
    prefs_.SetBoolean(prefs::kStabilityExitedCleanly,
                      params.local_state_beacon_value.value());
  }

  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);
  histogram_tester_.ExpectUniqueSample(
      "UMA.CleanExitBeacon.BeaconFileConsistency", params.expected_consistency,
      1);
}

#if defined(OS_ANDROID)
// TODO(crbug/1248239): Remove this test once the Extended Variations Safe Mode
// experiment is fully enabled on Android Chrome.

// Verify that the beacon file, if any, is ignored on Android.
TEST_F(CleanExitBeaconTest, BeaconFileIgnoredOnAndroid) {
  // Set up the beacon file such that the previous session did not exit cleanly
  // and the running crash streak is 2. The file (and thus these values) are
  // expected to be ignored.
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  const int last_session_num_crashes = 2;
  ASSERT_LT(0, base::WriteFile(temp_beacon_file_path,
                               CreateWellFormedBeaconFileContents(
                                   /*exited_cleanly=*/false,
                                   /*crash_streak=*/last_session_num_crashes)
                                   .data()));

  // Set up the PrefService such that the previous session exited cleanly and
  // the running crash streak is 0. The PrefService (and thus these values) are
  // expected to be used.
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, true);
  const int expected_num_crashes = 0;
  prefs_.SetInteger(variations::prefs::kVariationsCrashStreak,
                    expected_num_crashes);

  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);

  // Verify that the Local State beacon was used (not the beacon file beacon).
  EXPECT_TRUE(clean_exit_beacon.exited_cleanly());
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes",
                                       expected_num_crashes, 1);
}
#endif  // defined(OS_ANDROID)

// Verify that attempting to write synchronously DCHECKs for clients that do not
// belong to the SignalAndWriteViaFileUtil experiment group.
TEST_F(CleanExitBeaconTest, WriteBeaconValue_SynchronousWriteDcheck) {
  SetUpExtendedSafeModeExperiment(variations::kControlGroup);
  ASSERT_EQ(variations::kControlGroup, base::FieldTrialList::FindFullName(
                                           variations::kExtendedSafeModeTrial));

  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_.GetPath());
  EXPECT_DCHECK_DEATH(
      clean_exit_beacon.WriteBeaconValue(/*exited_cleanly=*/false,
                                         /*write_synchronously=*/true));
  histogram_tester_.ExpectTotalCount(
      "Variations.ExtendedSafeMode.WritePrefsTime", 0);
}

// The below CleanExitBeaconTest.BeaconState_* tests verify that the logic for
// recording UMA.CleanExitBeaconConsistency2 is correct.
#if defined(OS_IOS)
INSTANTIATE_TEST_SUITE_P(
    All,
    BackupBeaconConsistencyTest,
    ::testing::Values(
        BeaconConsistencyTestParams{
            .test_name = "MissingMissing",
            .expected_consistency =
                CleanExitBeaconConsistency::kMissingMissing},
        BeaconConsistencyTestParams{
            .test_name = "MissingClean",
            .local_state_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kMissingClean},
        BeaconConsistencyTestParams{
            .test_name = "MissingDirty",
            .local_state_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kMissingDirty},
        BeaconConsistencyTestParams{
            .test_name = "CleanMissing",
            .platform_specific_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kCleanMissing},
        BeaconConsistencyTestParams{
            .test_name = "DirtyMissing",
            .platform_specific_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyMissing},
        BeaconConsistencyTestParams{
            .test_name = "CleanClean",
            .platform_specific_beacon_value = true,
            .local_state_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kCleanClean},
        BeaconConsistencyTestParams{
            .test_name = "CleanDirty",
            .platform_specific_beacon_value = true,
            .local_state_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kCleanDirty},
        BeaconConsistencyTestParams{
            .test_name = "DirtyClean",
            .platform_specific_beacon_value = false,
            .local_state_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyClean},
        BeaconConsistencyTestParams{
            .test_name = "DirtyDirty",
            .platform_specific_beacon_value = false,
            .local_state_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyDirty}),
    [](const ::testing::TestParamInfo<BeaconConsistencyTestParams>& params) {
      return params.param.test_name;
    });

TEST_P(BackupBeaconConsistencyTest, BeaconConsistency) {
  // Clear the platform-specific and Local State beacons. Unless set below, the
  // beacons are considered missing.
  CleanExitBeacon::ResetStabilityExitedCleanlyForTesting(&prefs_);

  BeaconConsistencyTestParams params = GetParam();
  if (params.platform_specific_beacon_value) {
    CleanExitBeacon::SetUserDefaultsBeacon(
        /*exited_cleanly=*/params.platform_specific_beacon_value.value());
  }
  if (params.local_state_beacon_value) {
    prefs_.SetBoolean(prefs::kStabilityExitedCleanly,
                      params.local_state_beacon_value.value());
  }

  TestCleanExitBeacon clean_exit_beacon(&prefs_);
  histogram_tester_.ExpectUniqueSample("UMA.CleanExitBeaconConsistency2",
                                       params.expected_consistency, 1);
}
#endif  // defined(OS_IOS)

}  // namespace metrics
