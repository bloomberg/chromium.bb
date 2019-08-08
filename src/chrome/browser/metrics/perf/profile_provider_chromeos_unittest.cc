// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/profile_provider_chromeos.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/buildflags.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/metrics/perf/heap_collector.h"
#include "chrome/browser/metrics/perf/metric_collector.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

// Returns sample PerfDataProtos with custom timestamps. The contents don't have
// to make sense. They just need to constitute a semantically valid protobuf.
// |proto| is an output parameter that will contain the created protobuf.
PerfDataProto GetExamplePerfDataProto(int tstamp_sec) {
  PerfDataProto proto;
  proto.set_timestamp_sec(tstamp_sec);  // Time since epoch in seconds.

  PerfDataProto_PerfFileAttr* file_attr = proto.add_file_attrs();
  file_attr->add_ids(61);
  file_attr->add_ids(62);
  file_attr->add_ids(63);

  PerfDataProto_PerfEventAttr* attr = file_attr->mutable_attr();
  attr->set_type(1);
  attr->set_size(2);
  attr->set_config(3);
  attr->set_sample_period(4);
  attr->set_sample_freq(5);

  PerfDataProto_PerfEventStats* stats = proto.mutable_stats();
  stats->set_num_events_read(100);
  stats->set_num_sample_events(200);
  stats->set_num_mmap_events(300);
  stats->set_num_fork_events(400);
  stats->set_num_exit_events(500);

  return proto;
}

// Custome metric collectors to register with the profile provider for testing.
template <int TSTAMP>
class TestMetricCollector : public MetricCollector {
 public:
  TestMetricCollector() {}
  explicit TestMetricCollector(const CollectionParams& collection_params)
      : MetricCollector("UMA.CWP.TestData", collection_params) {}

  void CollectProfile(
      std::unique_ptr<SampledProfile> sampled_profile) override {
    PerfDataProto perf_data_proto = GetExamplePerfDataProto(TSTAMP);
    SaveSerializedPerfProto(std::move(sampled_profile),
                            PerfProtoType::PERF_TYPE_DATA,
                            perf_data_proto.SerializeAsString());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMetricCollector);
};

// Allows access to some private methods for testing.
class TestProfileProvider : public ProfileProvider {
 public:
  TestProfileProvider() {
    // Add a couple of metric collectors. We differentiate between them using
    // different time stamps for profiles. Set sampling factors for triggers
    // to 1, so we always trigger collection.
    CollectionParams test_params;
    test_params.resume_from_suspend.sampling_factor = 1;
    test_params.restore_session.sampling_factor = 1;

    collectors_.clear();
    collectors_.push_back(
        std::make_unique<TestMetricCollector<100>>(test_params));
    collectors_.push_back(
        std::make_unique<TestMetricCollector<200>>(test_params));
  }

  using ProfileProvider::collectors_;
  using ProfileProvider::LoggedInStateChanged;
  using ProfileProvider::OnSessionRestoreDone;
  using ProfileProvider::SuspendDone;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestProfileProvider);
};

template <SampledProfile_TriggerEvent TRIGGER_TYPE>
void ExpectTwoStoredPerfProfiles(
    const std::vector<SampledProfile>& stored_profiles) {
  ASSERT_EQ(2U, stored_profiles.size());
  // Both profiles must be of the given type and include perf data.
  const SampledProfile& profile1 = stored_profiles[0];
  const SampledProfile& profile2 = stored_profiles[1];
  EXPECT_EQ(TRIGGER_TYPE, profile1.trigger_event());
  ASSERT_TRUE(profile1.has_perf_data());
  EXPECT_EQ(TRIGGER_TYPE, profile2.trigger_event());
  ASSERT_TRUE(profile2.has_perf_data());

  // We must have received a profile from each of the collectors.
  EXPECT_EQ(100u, profile1.perf_data().timestamp_sec());
  EXPECT_EQ(200u, profile2.perf_data().timestamp_sec());
}

}  // namespace

class ProfileProviderTest : public testing::Test {
 public:
  ProfileProviderTest()
      : task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        task_runner_handle_(task_runner_) {}

  void SetUp() override {
    // ProfileProvider requires chromeos::LoginState and
    // chromeos::PowerManagerClient to be initialized.
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::LoginState::Initialize();

    profile_provider_ = std::make_unique<TestProfileProvider>();
    profile_provider_->Init();
  }

  void TearDown() override {
    profile_provider_.reset();
    chromeos::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  std::unique_ptr<TestProfileProvider> profile_provider_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  DISALLOW_COPY_AND_ASSIGN(ProfileProviderTest);
};

TEST_F(ProfileProviderTest, CheckSetup) {
  EXPECT_EQ(2U, profile_provider_->collectors_.size());

  // No profiles should be collected on start.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, UserLoginLogout) {
  // No user is logged in, so no collection is scheduled to run.
  task_runner_->RunPendingTasks();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());

  // Simulate a user log in, which should activate periodic collection for all
  // collectors.
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_REGULAR);

  // Run all pending tasks. SetLoggedInState has activated timers for periodic
  // collection causing timer based pending tasks.
  task_runner_->RunPendingTasks();
  // We should find two profiles, one for each collector.
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ExpectTwoStoredPerfProfiles<SampledProfile::PERIODIC_COLLECTION>(
      stored_profiles);

  // Periodic collection is deactivated when user logs out. Simulate a user
  // logout event.
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_NONE,
      chromeos::LoginState::LOGGED_IN_USER_NONE);
  // Run all pending tasks.
  task_runner_->RunPendingTasks();
  // We should find no new profiles.
  stored_profiles.clear();
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ASSERT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, SuspendDone_NoUserLoggedIn_NoCollection) {
  // No user is logged in, so no collection is done on resume from suspend.
  profile_provider_->SuspendDone(base::TimeDelta::FromMinutes(10));
  // Run all pending tasks.
  task_runner_->RunPendingTasks();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, CanceledSuspend_NoCollection) {
  // Set user state as logged in. This activates periodic collection, but we can
  // deactivate it for each collector.
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_REGULAR);
  for (auto& collector : profile_provider_->collectors_) {
    collector->Deactivate();
  }

  // Trigger a canceled suspend (zero sleep duration).
  profile_provider_->SuspendDone(base::TimeDelta::FromSeconds(0));
  // Run all pending tasks.
  task_runner_->RunPendingTasks();

  // We should find no profiles.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ASSERT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, SuspendDone) {
  // Set user state as logged in. This activates periodic collection, but we can
  // deactivate it for each collector.
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_REGULAR);

  // Trigger a resume from suspend.
  profile_provider_->SuspendDone(base::TimeDelta::FromMinutes(10));
  // Run all pending tasks.
  task_runner_->RunPendingTasks();

  // We should find two profiles, one for each collector.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ExpectTwoStoredPerfProfiles<SampledProfile::RESUME_FROM_SUSPEND>(
      stored_profiles);
}

TEST_F(ProfileProviderTest, OnSessionRestoreDone_NoUserLoggedIn_NoCollection) {
  // No user is logged in, so no collection is done on session restore.
  profile_provider_->OnSessionRestoreDone(10);
  // Run all pending tasks.
  task_runner_->RunPendingTasks();

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(profile_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(ProfileProviderTest, OnSessionRestoreDone) {
  // Set user state as logged in. This activates periodic collection, but we can
  // deactivate it for each collector.
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_REGULAR);
  for (auto& collector : profile_provider_->collectors_) {
    collector->Deactivate();
  }

  // Trigger a session restore.
  profile_provider_->OnSessionRestoreDone(10);
  // Run all pending tasks.
  task_runner_->RunPendingTasks();

  // We should find two profiles, one for each collector.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(profile_provider_->GetSampledProfiles(&stored_profiles));
  ExpectTwoStoredPerfProfiles<SampledProfile::RESTORE_SESSION>(stored_profiles);
}

namespace {

class TestParamsProfileProvider : public ProfileProvider {
 public:
  TestParamsProfileProvider() {}

  using ProfileProvider::collectors_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestParamsProfileProvider);
};

}  // namespace

class ProfileProviderFeatureParamsTest : public testing::Test {
 public:
  ProfileProviderFeatureParamsTest()
      : task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        task_runner_handle_(task_runner_) {}

  void SetUp() override {
    // ProfileProvider requires chromeos::LoginState and
    // chromeos::PowerManagerClient to be initialized.
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::LoginState::Initialize();
  }

  void TearDown() override {
    chromeos::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  DISALLOW_COPY_AND_ASSIGN(ProfileProviderFeatureParamsTest);
};

TEST_F(ProfileProviderFeatureParamsTest, HeapCollectorDisabled) {
  std::map<std::string, std::string> params;
  params.insert(
      std::make_pair(heap_profiling::kOOPHeapProfilingFeatureMode, "non-cwp"));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      heap_profiling::kOOPHeapProfilingFeature, params);

  TestParamsProfileProvider profile_provider;
  // We should have one collector registered.
  EXPECT_EQ(1u, profile_provider.collectors_.size());

  // After initialization, we should still have a single collector, because the
  // sampling factor param is set to 0.
  profile_provider.Init();
  EXPECT_EQ(1u, profile_provider.collectors_.size());
}

TEST_F(ProfileProviderFeatureParamsTest, HeapCollectorEnabled) {
  std::map<std::string, std::string> params;
  params.insert(std::make_pair(heap_profiling::kOOPHeapProfilingFeatureMode,
                               "cwp-tcmalloc"));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      heap_profiling::kOOPHeapProfilingFeature, params);

  TestParamsProfileProvider profile_provider;
  // We should have one collector registered.
  EXPECT_EQ(1u, profile_provider.collectors_.size());

  // After initialization, if the new tcmalloc is enabled, we should have two
  // collectors, because the sampling factor param is set to 1. Otherwise, we
  // must still have one collector only.
  profile_provider.Init();
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) && BUILDFLAG(USE_NEW_TCMALLOC)
  EXPECT_EQ(2u, profile_provider.collectors_.size());
#else
  EXPECT_EQ(1u, profile_provider.collectors_.size());
#endif
}

}  // namespace metrics
