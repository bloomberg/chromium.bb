// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The tests in this file verify the behavior of variations safe mode. The tests
// should be kept in sync with those in ios/chrome/browser/variations/
// variations_safe_mode_egtest.mm.

#include <string>

#include "base/base_switches.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/ranges/ranges.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/metrics.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_field_trial_creator.h"
#include "components/variations/service/variations_safe_mode_constants.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

class VariationsSafeModeBrowserTest : public InProcessBrowserTest {
 public:
  VariationsSafeModeBrowserTest() { DisableTestingConfig(); }
  ~VariationsSafeModeBrowserTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_PRE_PRE_ThreeCrashesTriggerSafeMode) {
  // The PRE test mechanism is used to set prefs in the local state file before
  // the next browser test runs. No InProcessBrowserTest functions allow this
  // pref to be set early enough to be read by the variations code, which runs
  // very early during startup.
  PrefService* local_state = g_browser_process->local_state();
  WriteSeedData(local_state, kTestSeedData, kSafeSeedPrefKeys);
  SimulateCrash(local_state);
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_PRE_ThreeCrashesTriggerSafeMode) {
  SimulateCrash(g_browser_process->local_state());
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_ThreeCrashesTriggerSafeMode) {
  SimulateCrash(g_browser_process->local_state());
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       ThreeCrashesTriggerSafeMode) {
  EXPECT_EQ(g_browser_process->local_state()->GetInteger(
                prefs::kVariationsCrashStreak),
            3);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 3,
                                       1);

  // Verify that Chrome fell back to a safe seed, which happens during browser
  // test setup.
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.LoadSafeSeed.Result", LoadSeedResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample("Variations.SeedUsage",
                                       SeedUsage::kSafeSeedUsed, 1);

  // Verify that |kTestSeedData| has been applied.
  EXPECT_TRUE(FieldTrialListHasAllStudiesFrom(kTestSeedData));
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_FetchFailuresTriggerSafeMode) {
  // The PRE test mechanism is used to set prefs in the local state file before
  // the next browser test runs. No InProcessBrowserTest functions allow this
  // pref to be set early enough to be read by the variations code, which runs
  // very early during startup.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 25);
  WriteSeedData(local_state, kTestSeedData, kSafeSeedPrefKeys);
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       FetchFailuresTriggerSafeMode) {
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 25, 1);

  // Verify that Chrome fell back to a safe seed, which happens during browser
  // test setup.
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.LoadSafeSeed.Result", LoadSeedResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample("Variations.SeedUsage",
                                       SeedUsage::kSafeSeedUsed, 1);

  // Verify that |kTestSeedData| has been applied.
  EXPECT_TRUE(FieldTrialListHasAllStudiesFrom(kTestSeedData));
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_DoNotTriggerSafeMode) {
  // The PRE test mechanism is used to set prefs in the local state file before
  // the next browser test runs. No InProcessBrowserTest functions allow this
  // pref to be set early enough to be read by the variations code, which runs
  // very early during startup.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(prefs::kVariationsCrashStreak, 2);
  local_state->SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 24);
  WriteSeedData(local_state, kTestSeedData, kRegularSeedPrefKeys);
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest, DoNotTriggerSafeMode) {
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 2,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 24, 1);

  // Verify that Chrome applied the regular seed.
  histogram_tester_.ExpectUniqueSample("Variations.SeedLoadResult",
                                       LoadSeedResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample("Variations.SeedUsage",
                                       SeedUsage::kRegularSeedUsed, 1);
}

// This test code is programmatically launched by the SafeModeEndToEnd
// test below. Its primary purpose is to provide an entry-point by
// which the SafeModeEndToEnd test can cause the Field Trial Setup
// code to be exercised. For some launches, the setup code is expected
// to crash before reaching the test body; the test body simply verifies
// that the test is using the user-data-dir configured on the command-line.
//
// The MANUAL_ prefix prevents the test from running unless explicitly
// invoked.
IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest, MANUAL_SubTest) {
  // Validate that Chrome is running with the user-data-dir specified on the
  // command-line.
  base::FilePath expected_user_data_dir =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          ::switches::kUserDataDir);
  base::FilePath actual_user_data_dir;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_USER_DATA, &actual_user_data_dir));
  ASSERT_FALSE(expected_user_data_dir.empty());
  ASSERT_FALSE(actual_user_data_dir.empty());
  ASSERT_EQ(actual_user_data_dir, expected_user_data_dir);

  // If the test makes it this far, then either it's the first run of the
  // test, or the safe seed was used.
  const int crash_streak = g_browser_process->local_state()->GetInteger(
      prefs::kVariationsCrashStreak);
  const bool is_first_run = (crash_streak == 0);
  const bool safe_seed_was_used =
      FieldTrialListHasAllStudiesFrom(kTestSeedData);
  EXPECT_NE(is_first_run, safe_seed_was_used)  // ==> XOR
      << "crash_streak=" << crash_streak;
}

namespace {

class FieldTrialTest : public ::testing::Test {
 public:
  void SetUp() override {
    ::testing::Test::SetUp();
    metrics::CleanExitBeacon::SkipCleanShutdownStepsForTesting();

    pref_registry_ = base::MakeRefCounted<PrefRegistrySimple>();
    metrics::MetricsService::RegisterPrefs(pref_registry_.get());
    variations::VariationsService::RegisterPrefs(pref_registry_.get());

    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    user_data_dir_ = temp_dir_.GetPath().AppendASCII("user-data-dir");
    local_state_file_ = user_data_dir_.AppendASCII("Local State");
  }

 protected:
  const base::FilePath& user_data_dir() const { return user_data_dir_; }
  const base::FilePath& local_state_file() const { return local_state_file_; }

  const base::FilePath CopyOfLocalStateFile(int suffix) const {
    base::FilePath copy_of_local_state_file = temp_dir_.GetPath().AppendASCII(
        base::StringPrintf("local-state-copy-%d.json", suffix));
    base::CopyFile(local_state_file(), copy_of_local_state_file);
    return copy_of_local_state_file;
  }

  bool IsSuccessfulSubTestOutput(const std::string& output) {
    static const char* const kSubTestSuccessStrings[] = {
        "Running 1 test from 1 test suite",
        "OK ] VariationsSafeModeBrowserTest.MANUAL_SubTest",
        "1 test from VariationsSafeModeBrowserTest",
        "1 test from 1 test suite ran",
    };
    auto is_in_output = [&](const char* s) {
      return base::Contains(output, s);
    };
    return base::ranges::all_of(kSubTestSuccessStrings, is_in_output);
  }

  bool IsCrashingSubTestOutput(const std::string& output) {
    const char* const kSubTestCrashStrings[] = {
        "Running 1 test from 1 test suite",
        "VariationsSafeModeBrowserTest.MANUAL_SubTest",
        "crash_for_testing",
    };
    auto is_in_output = [&](const char* s) {
      return base::Contains(output, s);
    };
    return base::ranges::all_of(kSubTestCrashStrings, is_in_output);
  }

  void RunAndExpectSuccessfulSubTest(
      const base::CommandLine& sub_test_command) {
    std::string output;
    base::GetAppOutputAndError(sub_test_command, &output);
    EXPECT_TRUE(IsSuccessfulSubTestOutput(output))
        << "Did not find success signals in output:\n"
        << output;
  }

  void RunAndExpectCrashingSubTest(const base::CommandLine& sub_test_command) {
    std::string output;
    base::GetAppOutputAndError(sub_test_command, &output);
    EXPECT_FALSE(IsSuccessfulSubTestOutput(output))
        << "Expected crash but found success signals in output:\n"
        << output;
    EXPECT_TRUE(IsCrashingSubTestOutput(output))
        << "Did not find crash signals in output:\n"
        << output;
  }

  std::unique_ptr<PrefService> LoadLocalState(const base::FilePath& path) {
    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_async(false);
    pref_service_factory.set_user_prefs(
        base::MakeRefCounted<JsonPrefStore>(path));
    return pref_service_factory.Create(pref_registry_);
  }

  std::unique_ptr<metrics::CleanExitBeacon> LoadCleanExitBeacon(
      PrefService* pref_service) {
    static constexpr wchar_t kDummyWindowsRegistryKey[] = L"";
    auto clean_exit_beacon = std::make_unique<metrics::CleanExitBeacon>(
        kDummyWindowsRegistryKey, user_data_dir(), pref_service,
        version_info::Channel::UNKNOWN);
    clean_exit_beacon->Initialize();
    return clean_exit_beacon;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<PrefRegistrySimple> pref_registry_;
  base::ScopedTempDir temp_dir_;
  base::FilePath user_data_dir_;
  base::FilePath local_state_file_;
};

}  // namespace

TEST_F(FieldTrialTest, ExtendedSafeModeEndToEnd) {
  // Reuse the browser_tests binary (i.e., that this test code is in), to
  // manually run the sub-test.
  base::CommandLine sub_test =
      base::CommandLine(base::CommandLine::ForCurrentProcess()->GetProgram());

  // Run the manual sub-test in the |user_data_dir()| allocated for this test.
  sub_test.AppendSwitchASCII(base::kGTestFilterFlag,
                             "VariationsSafeModeBrowserTest.MANUAL_SubTest");
  sub_test.AppendSwitch(::switches::kRunManualTestsFlag);
  sub_test.AppendSwitch(::switches::kSingleProcessTests);
  sub_test.AppendSwitchPath(::switches::kUserDataDir, user_data_dir());

  const std::string group_name = kSignalAndWriteViaFileUtilGroup;
  // Select the extended variations safe mode field trial group. The "*"
  // prefix forces the experiment/trial state to "active" at startup.
  sub_test.AppendSwitchASCII(
      ::switches::kForceFieldTrials,
      base::StrCat({"*", kExtendedSafeModeTrial, "/", group_name, "/"}));

  // Assign the test environment to be on the "Dev" channel. This ensures
  // compatibility with both the extended safe mode trial and the crashing
  // study in the seed.
  sub_test.AppendSwitchASCII(switches::kFakeVariationsChannel, "dev");

  // Explicitly avoid any terminal control characters in the output.
  sub_test.AppendSwitchASCII("gtest_color", "no");

  // Initial sub-test run should be successful.
  RunAndExpectSuccessfulSubTest(sub_test);

  // Inject the safe and crashing seeds into the Local State of |sub_test|.
  {
    auto local_state = LoadLocalState(local_state_file());
    WriteSeedData(local_state.get(), kTestSeedData, kSafeSeedPrefKeys);
    WriteSeedData(local_state.get(), kCrashingSeedData, kRegularSeedPrefKeys);
  }

  SetUpExtendedSafeModeExperiment(group_name);

  // The next |kCrashStreakThreshold| runs of the sub-test should crash...
  for (int run_count = 1; run_count <= kCrashStreakThreshold; ++run_count) {
    SCOPED_TRACE(base::StringPrintf("Run #%d with crashing seed", run_count));
    RunAndExpectCrashingSubTest(sub_test);
    auto local_state = LoadLocalState(CopyOfLocalStateFile(run_count));
    auto clean_exit_beacon = LoadCleanExitBeacon(local_state.get());
    ASSERT_TRUE(clean_exit_beacon != nullptr);
    ASSERT_FALSE(clean_exit_beacon->exited_cleanly());
    ASSERT_EQ(run_count,
              local_state->GetInteger(prefs::kVariationsCrashStreak));
  }

  // Do another run and verify that safe mode kicks in, preventing the crash.
  RunAndExpectSuccessfulSubTest(sub_test);
}

}  // namespace variations
