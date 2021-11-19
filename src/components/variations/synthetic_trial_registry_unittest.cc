// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/synthetic_trial_registry.h"

#include <string>

#include "base/containers/contains.h"
#include "base/metrics/field_trial.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_crash_keys.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

class SyntheticTrialRegistryTest : public ::testing::Test {
 public:
  SyntheticTrialRegistryTest() { InitCrashKeys(); }

  SyntheticTrialRegistryTest(const SyntheticTrialRegistryTest&) = delete;
  SyntheticTrialRegistryTest& operator=(const SyntheticTrialRegistryTest&) =
      delete;

  ~SyntheticTrialRegistryTest() override { ClearCrashKeysInstanceForTesting(); }

  // Returns true if there is a synthetic trial in the given vector that matches
  // the given trial name and trial group; returns false otherwise.
  bool HasSyntheticTrial(const std::vector<ActiveGroupId>& synthetic_trials,
                         const std::string& trial_name,
                         const std::string& trial_group) {
    uint32_t trial_name_hash = HashName(trial_name);
    uint32_t trial_group_hash = HashName(trial_group);
    for (const ActiveGroupId& trial : synthetic_trials) {
      if (trial.name == trial_name_hash && trial.group == trial_group_hash)
        return true;
    }
    return false;
  }

  // Waits until base::TimeTicks::Now() no longer equals |value|. This should
  // take between 1-15ms per the documented resolution of base::TimeTicks.
  void WaitUntilTimeChanges(const base::TimeTicks& value) {
    while (base::TimeTicks::Now() == value) {
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
  }

  // Gets the current synthetic trials.
  void GetSyntheticTrials(const SyntheticTrialRegistry& registry,
                          std::vector<ActiveGroupId>* synthetic_trials) {
    // Ensure that time has advanced by at least a tick before proceeding.
    WaitUntilTimeChanges(base::TimeTicks::Now());
    registry.GetSyntheticFieldTrialsOlderThan(base::TimeTicks::Now(),
                                              synthetic_trials);
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SyntheticTrialRegistryTest, RegisterSyntheticTrial) {
  SyntheticTrialRegistry registry;

  // Add two synthetic trials and confirm that they show up in the list.
  SyntheticTrialGroup trial1(HashName("TestTrial1"), HashName("Group1"));
  registry.RegisterSyntheticFieldTrial(trial1);

  SyntheticTrialGroup trial2(HashName("TestTrial2"), HashName("Group2"));
  registry.RegisterSyntheticFieldTrial(trial2);
  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(base::TimeTicks::Now());

  // Save the time when the log was started (it's okay for this to be greater
  // than the time recorded by the above call since it's used to ensure the
  // value changes).
  const base::TimeTicks begin_log_time = base::TimeTicks::Now();

  std::vector<ActiveGroupId> synthetic_trials;
  registry.GetSyntheticFieldTrialsOlderThan(base::TimeTicks::Now(),
                                            &synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "Group1"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(begin_log_time);

  // Change the group for the first trial after the log started.
  SyntheticTrialGroup trial3(HashName("TestTrial1"), HashName("Group2"));
  registry.RegisterSyntheticFieldTrial(trial3);
  registry.GetSyntheticFieldTrialsOlderThan(begin_log_time, &synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Add a new trial after the log started and confirm that it doesn't show up.
  SyntheticTrialGroup trial4(HashName("TestTrial3"), HashName("Group3"));
  registry.RegisterSyntheticFieldTrial(trial4);
  registry.GetSyntheticFieldTrialsOlderThan(begin_log_time, &synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Start a new log and ensure all three trials appear in it.
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(3U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial3", "Group3"));
}

TEST_F(SyntheticTrialRegistryTest, RegisterExternalExperiments_NoAllowlist) {
  SyntheticTrialRegistry registry(false);
  const std::string context = "TestTrial1";
  const auto mode = SyntheticTrialRegistry::kOverrideExistingIds;

  registry.RegisterExternalExperiments(context, {100, 200}, mode);
  std::vector<ActiveGroupId> synthetic_trials;
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "100"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "200"));

  // Change the group for the trial to a single group.
  registry.RegisterExternalExperiments(context, {500}, mode);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "500"));

  // Register a trial with no groups, which should effectively remove the trial.
  registry.RegisterExternalExperiments(context, {}, mode);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(0U, synthetic_trials.size());
}

TEST_F(SyntheticTrialRegistryTest, RegisterExternalExperiments_WithAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      internal::kExternalExperimentAllowlist,
      {{"100", "A"}, {"101", "A"}, {"300", "C,xyz"}});

  const std::string context = "Test";
  const auto override_mode = SyntheticTrialRegistry::kOverrideExistingIds;
  SyntheticTrialRegistry registry;
  std::vector<ActiveGroupId> synthetic_trials;

  // Register a synthetic trial TestTrial1 with groups A and B.
  registry.RegisterExternalExperiments(context, {100, 200, 300}, override_mode);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "A", "100"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "C", "300"));

  // A new call that only contains 100 will clear the other ones.
  registry.RegisterExternalExperiments(context, {101}, override_mode);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "A", "101"));

  const auto dont_override = SyntheticTrialRegistry::kDoNotOverrideExistingIds;
  // Now, register another id that doesn't exist with kDoNotOverrideExistingIds.
  registry.RegisterExternalExperiments(context, {300}, dont_override);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "A", "101"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "C", "300"));

  // Registering 100, which already has a trial A registered, shouldn't work.
  registry.RegisterExternalExperiments(context, {100}, dont_override);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "A", "101"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "C", "300"));

  // Registering an empty set should also do nothing.
  registry.RegisterExternalExperiments(context, {}, dont_override);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "A", "101"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "C", "300"));

  // Registering with an override should reset existing ones.
  registry.RegisterExternalExperiments(context, {100}, override_mode);
  GetSyntheticTrials(registry, &synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "A", "100"));
}

TEST_F(SyntheticTrialRegistryTest, GetSyntheticFieldTrialActiveGroups) {
  SyntheticTrialRegistry registry;

  // Instantiate and set up the corresponding singleton observer which tracks
  // the creation of all SyntheticTrialGroups.
  registry.AddSyntheticTrialObserver(
      SyntheticTrialsActiveGroupIdProvider::GetInstance());

  // Add two synthetic trials and confirm that they show up in the list.
  SyntheticTrialGroup trial1(HashName("TestTrial1"), HashName("Group1"));
  registry.RegisterSyntheticFieldTrial(trial1);

  SyntheticTrialGroup trial2(HashName("TestTrial2"), HashName("Group2"));
  registry.RegisterSyntheticFieldTrial(trial2);

  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(base::TimeTicks::Now());

  // Now get the list of currently active groups.
  std::vector<std::string> output;
  GetSyntheticTrialGroupIdsAsString(&output);
  EXPECT_EQ(2U, output.size());

  std::string trial1_hash =
      base::StringPrintf("%x-%x", trial1.id.name, trial1.id.group);
  EXPECT_TRUE(base::Contains(output, trial1_hash));

  std::string trial2_hash =
      base::StringPrintf("%x-%x", trial2.id.name, trial2.id.group);
  EXPECT_TRUE(base::Contains(output, trial2_hash));
}

}  // namespace variations
