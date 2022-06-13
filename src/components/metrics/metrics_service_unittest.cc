// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/metrics/client_info.h"
#include "components/metrics/environment_recorder.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_upload_scheduler.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/test/test_metrics_provider.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/metrics/unsent_log_store_metrics_impl.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/active_field_trials.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace metrics {
namespace {

const char kTestPrefName[] = "TestPref";

class TestUnsentLogStore : public UnsentLogStore {
 public:
  explicit TestUnsentLogStore(PrefService* service)
      : UnsentLogStore(std::make_unique<UnsentLogStoreMetricsImpl>(),
                       service,
                       kTestPrefName,
                       nullptr,
                       /* min_log_count= */ 3,
                       /* min_log_bytes= */ 1,
                       /* max_log_size= */ 0,
                       std::string()) {}
  ~TestUnsentLogStore() override = default;

  TestUnsentLogStore(const TestUnsentLogStore&) = delete;
  TestUnsentLogStore& operator=(const TestUnsentLogStore&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry) {
    registry->RegisterListPref(kTestPrefName);
  }
};

void YieldUntil(base::Time when) {
  while (base::Time::Now() <= when)
    base::PlatformThread::YieldCurrentThread();
}

// Returns true if |id| is present in |proto|'s collection of FieldTrials.
bool IsFieldTrialPresent(const SystemProfileProto& proto,
                         const std::string& trial_name,
                         const std::string& group_name) {
  const variations::ActiveGroupId id =
      variations::MakeActiveGroupId(trial_name, group_name);

  for (const auto& trial : proto.field_trial()) {
    if (trial.name_id() == id.name && trial.group_id() == id.group)
      return true;
  }
  return false;
}

class TestMetricsService : public MetricsService {
 public:
  TestMetricsService(MetricsStateManager* state_manager,
                     MetricsServiceClient* client,
                     PrefService* local_state)
      : MetricsService(state_manager, client, local_state) {}

  TestMetricsService(const TestMetricsService&) = delete;
  TestMetricsService& operator=(const TestMetricsService&) = delete;

  ~TestMetricsService() override = default;

  using MetricsService::INIT_TASK_SCHEDULED;
  using MetricsService::RecordCurrentEnvironmentHelper;
  using MetricsService::SENDING_LOGS;
  using MetricsService::state;

  // MetricsService:
  void SetPersistentSystemProfile(const std::string& serialized_proto,
                                  bool complete) override {
    persistent_system_profile_provided_ = true;
    persistent_system_profile_complete_ = complete;
  }

  bool persistent_system_profile_provided() const {
    return persistent_system_profile_provided_;
  }
  bool persistent_system_profile_complete() const {
    return persistent_system_profile_complete_;
  }

 private:
  bool persistent_system_profile_provided_ = false;
  bool persistent_system_profile_complete_ = false;
};

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id,
                 int session_id,
                 MetricsServiceClient* client)
      : MetricsLog(client_id, session_id, MetricsLog::ONGOING_LOG, client) {}

  TestMetricsLog(const TestMetricsLog&) = delete;
  TestMetricsLog& operator=(const TestMetricsLog&) = delete;

  ~TestMetricsLog() override {}
};

const char kOnDidCreateMetricsLogHistogramName[] = "Test.OnDidCreateMetricsLog";

class TestMetricsProviderForOnDidCreateMetricsLog : public TestMetricsProvider {
 public:
  TestMetricsProviderForOnDidCreateMetricsLog() = default;
  ~TestMetricsProviderForOnDidCreateMetricsLog() override = default;

  void OnDidCreateMetricsLog() override {
    base::UmaHistogramBoolean(kOnDidCreateMetricsLogHistogramName, true);
  }
};

class MetricsServiceTest : public testing::Test {
 public:
  MetricsServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_),
        enabled_state_provider_(new TestEnabledStateProvider(false, false)) {
    base::SetRecordActionTaskRunner(task_runner_);
    MetricsService::RegisterPrefs(testing_local_state_.registry());
  }

  MetricsServiceTest(const MetricsServiceTest&) = delete;
  MetricsServiceTest& operator=(const MetricsServiceTest&) = delete;

  ~MetricsServiceTest() override {}

  MetricsStateManager* GetMetricsStateManager(
      StartupVisibility startup_visibility = StartupVisibility::kUnknown) {
    // Lazy-initialize the metrics_state_manager so that it correctly reads the
    // stability state from prefs after tests have a chance to initialize it.
    if (!metrics_state_manager_) {
      metrics_state_manager_ = MetricsStateManager::Create(
          GetLocalState(), enabled_state_provider_.get(), std::wstring(),
          base::FilePath(), startup_visibility);
      metrics_state_manager_->InstantiateFieldTrialList();
    }
    return metrics_state_manager_.get();
  }

  std::unique_ptr<TestUnsentLogStore> InitializeTestLogStoreAndGet() {
    TestUnsentLogStore::RegisterPrefs(testing_local_state_.registry());
    return std::make_unique<TestUnsentLogStore>(GetLocalState());
  }

  PrefService* GetLocalState() { return &testing_local_state_; }

  // Sets metrics reporting as enabled for testing.
  void EnableMetricsReporting() {
    enabled_state_provider_->set_consent(true);
    enabled_state_provider_->set_enabled(true);
  }

  // Finds a histogram with the specified |name_hash| in |histograms|.
  const base::HistogramBase* FindHistogram(
      const base::StatisticsRecorder::Histograms& histograms,
      uint64_t name_hash) {
    for (const base::HistogramBase* histogram : histograms) {
      if (name_hash == base::HashMetricName(histogram->histogram_name()))
        return histogram;
    }
    return nullptr;
  }

  // Checks whether |uma_log| contains any histograms that are not flagged
  // with kUmaStabilityHistogramFlag. Stability logs should only contain such
  // histograms.
  void CheckForNonStabilityHistograms(
      const ChromeUserMetricsExtension& uma_log) {
    const int kStabilityFlags = base::HistogramBase::kUmaStabilityHistogramFlag;
    const base::StatisticsRecorder::Histograms histograms =
        base::StatisticsRecorder::GetHistograms();
    for (int i = 0; i < uma_log.histogram_event_size(); ++i) {
      const uint64_t hash = uma_log.histogram_event(i).name_hash();

      const base::HistogramBase* histogram = FindHistogram(histograms, hash);
      EXPECT_TRUE(histogram) << hash;

      EXPECT_EQ(kStabilityFlags, histogram->flags() & kStabilityFlags) << hash;
    }
  }

  // Returns the number of samples logged to the specified histogram or 0 if
  // the histogram was not found.
  int GetHistogramSampleCount(const ChromeUserMetricsExtension& uma_log,
                              base::StringPiece histogram_name) {
    const auto histogram_name_hash = base::HashMetricName(histogram_name);
    int samples = 0;
    for (int i = 0; i < uma_log.histogram_event_size(); ++i) {
      const auto& histogram = uma_log.histogram_event(i);
      if (histogram.name_hash() == histogram_name_hash) {
        for (int j = 0; j < histogram.bucket_size(); ++j) {
          const auto& bucket = histogram.bucket(j);
          // Per proto comments, count field not being set means 1 sample.
          samples += (!bucket.has_count() ? 1 : bucket.count());
        }
      }
    }
    return samples;
  }

  // Returns the sampled count of the |kOnDidCreateMetricsLogHistogramName|
  // histogram in the currently staged log in |test_log_store|.
  int GetSampleCountOfOnDidCreateLogHistogram(MetricsLogStore* test_log_store) {
    ChromeUserMetricsExtension log;
    EXPECT_TRUE(DecodeLogDataToProto(test_log_store->staged_log(), &log));
    return GetHistogramSampleCount(log, kOnDidCreateMetricsLogHistogramName);
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  base::test::ScopedFeatureList feature_list_;

 private:
  std::unique_ptr<TestEnabledStateProvider> enabled_state_provider_;
  TestingPrefServiceSimple testing_local_state_;
  std::unique_ptr<MetricsStateManager> metrics_state_manager_;
};

struct StartupVisibilityTestParams {
  const std::string test_name;
  metrics::StartupVisibility startup_visibility;
  bool expected_beacon_value;
};

class MetricsServiceTestWithStartupVisibility
    : public MetricsServiceTest,
      public ::testing::WithParamInterface<StartupVisibilityTestParams> {};

class ExperimentTestMetricsProvider : public TestMetricsProvider {
 public:
  explicit ExperimentTestMetricsProvider(
      base::FieldTrial* profile_metrics_trial,
      base::FieldTrial* session_data_trial)
      : profile_metrics_trial_(profile_metrics_trial),
        session_data_trial_(session_data_trial) {}

  ~ExperimentTestMetricsProvider() override = default;

  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile_proto) override {
    TestMetricsProvider::ProvideSystemProfileMetrics(system_profile_proto);
    profile_metrics_trial_->group();
  }

  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override {
    TestMetricsProvider::ProvideCurrentSessionData(uma_proto);
    session_data_trial_->group();
  }

 private:
  raw_ptr<base::FieldTrial> profile_metrics_trial_;
  raw_ptr<base::FieldTrial> session_data_trial_;
};

}  // namespace

TEST_F(MetricsServiceTest, InitialStabilityLogAfterCleanShutDown) {
  EnableMetricsReporting();
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(GetLocalState(), true);

  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());

  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();

  // No initial stability log should be generated.
  EXPECT_FALSE(service.has_unsent_logs());

  // Ensure that HasPreviousSessionData() is always called on providers,
  // for consistency, even if other conditions already indicate their presence.
  EXPECT_TRUE(test_provider->has_initial_stability_metrics_called());

  // The test provider should not have been called upon to provide initial
  // stability nor regular stability metrics.
  EXPECT_FALSE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_FALSE(test_provider->provide_stability_metrics_called());
}

TEST_F(MetricsServiceTest, InitialStabilityLogAtProviderRequest) {
  EnableMetricsReporting();

  // Save an existing system profile to prefs, to correspond to what would be
  // saved from a previous session.
  TestMetricsServiceClient client;
  TestMetricsLog log("client", 1, &client);
  DelegatingProvider delegating_provider;
  TestMetricsService::RecordCurrentEnvironmentHelper(&log, GetLocalState(),
                                                     &delegating_provider);

  // Record stability build time and version from previous session, so that
  // stability metrics (including exited cleanly flag) won't be cleared.
  EnvironmentRecorder(GetLocalState())
      .SetBuildtimeAndVersion(MetricsLog::GetBuildTime(),
                              client.GetVersionString());

  // Set the clean exit flag, as that will otherwise cause a stabilty
  // log to be produced, irrespective provider requests.
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(GetLocalState(), true);

  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());
  // Add a metrics provider that requests a stability log.
  TestMetricsProvider* test_provider = new TestMetricsProvider();
  test_provider->set_has_initial_stability_metrics(true);
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();

  // The initial stability log should be generated and persisted in unsent logs.
  MetricsLogStore* test_log_store = service.LogStoreForTest();
  EXPECT_TRUE(test_log_store->has_unsent_logs());
  EXPECT_FALSE(test_log_store->has_staged_log());

  // Ensure that HasPreviousSessionData() is always called on providers,
  // for consistency, even if other conditions already indicate their presence.
  EXPECT_TRUE(test_provider->has_initial_stability_metrics_called());

  // The test provider should have been called upon to provide initial
  // stability and regular stability metrics.
  EXPECT_TRUE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_TRUE(test_provider->provide_stability_metrics_called());

  // Stage the log and retrieve it.
  test_log_store->StageNextLog();
  EXPECT_TRUE(test_log_store->has_staged_log());

  ChromeUserMetricsExtension uma_log;
  EXPECT_TRUE(DecodeLogDataToProto(test_log_store->staged_log(), &uma_log));

  EXPECT_TRUE(uma_log.has_client_id());
  EXPECT_TRUE(uma_log.has_session_id());
  EXPECT_TRUE(uma_log.has_system_profile());
  EXPECT_EQ(0, uma_log.user_action_event_size());
  EXPECT_EQ(0, uma_log.omnibox_event_size());
  EXPECT_EQ(0, uma_log.perf_data_size());
  CheckForNonStabilityHistograms(uma_log);

  // As there wasn't an unclean shutdown, this log has zero crash count.
  EXPECT_EQ(0, uma_log.system_profile().stability().crash_count());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MetricsServiceTestWithStartupVisibility,
    ::testing::Values(
        StartupVisibilityTestParams{
            .test_name = "UnknownVisibility",
            .startup_visibility = StartupVisibility::kUnknown,
            .expected_beacon_value = true},
        StartupVisibilityTestParams{
            .test_name = "BackgroundVisibility",
            .startup_visibility = StartupVisibility::kBackground,
            .expected_beacon_value = true},
        StartupVisibilityTestParams{
            .test_name = "ForegroundVisibility",
            .startup_visibility = StartupVisibility::kForeground,
            .expected_beacon_value = false}),
    [](const ::testing::TestParamInfo<StartupVisibilityTestParams>& params) {
      return params.param.test_name;
    });

TEST_P(MetricsServiceTestWithStartupVisibility, InitialStabilityLogAfterCrash) {
  PrefService* local_state = GetLocalState();
  EnableMetricsReporting();
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(local_state, false);

  // Set up prefs to simulate restarting after a crash.

  // Save an existing system profile to prefs, to correspond to what would be
  // saved from a previous session.
  TestMetricsServiceClient client;
  const std::string kCrashedVersion = "4.0.321.0-64-devel";
  client.set_version_string(kCrashedVersion);
  TestMetricsLog log("client", 1, &client);
  DelegatingProvider delegating_provider;
  TestMetricsService::RecordCurrentEnvironmentHelper(&log, local_state,
                                                     &delegating_provider);

  // Record stability build time and version from previous session, so that
  // stability metrics (including exited cleanly flag) won't be cleared.
  EnvironmentRecorder(local_state)
      .SetBuildtimeAndVersion(MetricsLog::GetBuildTime(),
                              client.GetVersionString());

  const std::string kCurrentVersion = "5.0.322.0-64-devel";
  client.set_version_string(kCurrentVersion);

  StartupVisibilityTestParams params = GetParam();
  TestMetricsService service(GetMetricsStateManager(params.startup_visibility),
                             &client, local_state);
  // Add a provider.
  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));
  service.InitializeMetricsRecordingState();

  // Verify that Chrome is (or is not) watching for crashes by checking the
  // beacon value.
#if defined(OS_ANDROID)
  EXPECT_EQ(local_state->GetBoolean(prefs::kStabilityExitedCleanly),
            params.expected_beacon_value);
#else
  EXPECT_FALSE(local_state->GetBoolean(prefs::kStabilityExitedCleanly));
#endif  // defined(OS_ANDROID)

  // The initial stability log should be generated and persisted in unsent logs.
  MetricsLogStore* test_log_store = service.LogStoreForTest();
  EXPECT_TRUE(test_log_store->has_unsent_logs());
  EXPECT_FALSE(test_log_store->has_staged_log());

  // Ensure that HasPreviousSessionData() is always called on providers,
  // for consistency, even if other conditions already indicate their presence.
  EXPECT_TRUE(test_provider->has_initial_stability_metrics_called());

  // The test provider should have been called upon to provide initial
  // stability and regular stability metrics.
  EXPECT_TRUE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_TRUE(test_provider->provide_stability_metrics_called());

  // Stage the log and retrieve it.
  test_log_store->StageNextLog();
  EXPECT_TRUE(test_log_store->has_staged_log());

  ChromeUserMetricsExtension uma_log;
  EXPECT_TRUE(DecodeLogDataToProto(test_log_store->staged_log(), &uma_log));

  EXPECT_TRUE(uma_log.has_client_id());
  EXPECT_TRUE(uma_log.has_session_id());
  EXPECT_TRUE(uma_log.has_system_profile());
  EXPECT_EQ(0, uma_log.user_action_event_size());
  EXPECT_EQ(0, uma_log.omnibox_event_size());
  EXPECT_EQ(0, uma_log.perf_data_size());
  CheckForNonStabilityHistograms(uma_log);

  EXPECT_EQ(kCrashedVersion, uma_log.system_profile().app_version());
  EXPECT_EQ(kCurrentVersion,
            uma_log.system_profile().log_written_by_app_version());

  EXPECT_EQ(1, uma_log.system_profile().stability().crash_count());
}

TEST_F(MetricsServiceTest, InitialLogsHaveOnDidCreateMetricsLogHistograms) {
  EnableMetricsReporting();
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());

  // Create a provider that will log to |kOnDidCreateMetricsLogHistogramName|
  // in OnDidCreateMetricsLog()
  auto* test_provider = new TestMetricsProviderForOnDidCreateMetricsLog();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();
  // Start() will create the first ongoing log.
  service.Start();
  ASSERT_EQ(TestMetricsService::INIT_TASK_SCHEDULED, service.state());

  // Run pending tasks to finish init task and complete the first ongoing log.
  task_runner_->RunPendingTasks();
  ASSERT_EQ(TestMetricsService::SENDING_LOGS, service.state());

  MetricsLogStore* test_log_store = service.LogStoreForTest();

  // Stage the next log, which should be the first ongoing log.
  // Check that it has one sample in |kOnDidCreateMetricsLogHistogramName|.
  test_log_store->StageNextLog();
  EXPECT_EQ(1, GetSampleCountOfOnDidCreateLogHistogram(test_log_store));

  // Discard the staged log and close and stage the next log, which is the
  // second "ongoing log".
  // Check that it has one sample in |kOnDidCreateMetricsLogHistogramName|.
  test_log_store->DiscardStagedLog();
  service.StageCurrentLogForTest();
  EXPECT_EQ(1, GetSampleCountOfOnDidCreateLogHistogram(test_log_store));

  // Check one more log for good measure.
  test_log_store->DiscardStagedLog();
  service.StageCurrentLogForTest();
  EXPECT_EQ(1, GetSampleCountOfOnDidCreateLogHistogram(test_log_store));
}

TEST_F(MetricsServiceTest, FirstLogCreatedBeforeUnsentLogsSent) {
  EnableMetricsReporting();
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());

  service.InitializeMetricsRecordingState();
  // Start() will create the first ongoing log.
  service.Start();
  ASSERT_EQ(TestMetricsService::INIT_TASK_SCHEDULED, service.state());

  MetricsLogStore* test_log_store = service.LogStoreForTest();

  // Set up the log store with an existing fake log entry. The string content
  // is never deserialized to proto, so we're just passing some dummy content.
  ASSERT_EQ(0u, test_log_store->initial_log_count());
  ASSERT_EQ(0u, test_log_store->ongoing_log_count());
  test_log_store->StoreLog("blah_blah", MetricsLog::ONGOING_LOG, LogMetadata());
  // Note: |initial_log_count()| refers to initial stability logs, so the above
  // log is counted an ongoing log (per its type).
  ASSERT_EQ(0u, test_log_store->initial_log_count());
  ASSERT_EQ(1u, test_log_store->ongoing_log_count());

  // Run pending tasks to finish init task and complete the first ongoing log.
  task_runner_->RunPendingTasks();
  ASSERT_EQ(TestMetricsService::SENDING_LOGS, service.state());
  // When the init task is complete, the first ongoing log should be created
  // and added to the ongoing logs.
  EXPECT_EQ(0u, test_log_store->initial_log_count());
  EXPECT_EQ(2u, test_log_store->ongoing_log_count());
}

TEST_F(MetricsServiceTest,
       MetricsProviderOnRecordingDisabledCalledOnInitialStop) {
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());

  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();
  service.Stop();

  EXPECT_TRUE(test_provider->on_recording_disabled_called());
}

TEST_F(MetricsServiceTest, MetricsProvidersInitialized) {
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());

  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();

  EXPECT_TRUE(test_provider->init_called());
}

// Verify that FieldTrials activated by a MetricsProvider are reported by the
// FieldTrialsProvider.
TEST_F(MetricsServiceTest, ActiveFieldTrialsReported) {
  EnableMetricsReporting();
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());

  // Set up FieldTrials.
  const std::string trial_name1 = "CoffeeExperiment";
  const std::string group_name1 = "Free";
  base::FieldTrial* trial1 =
      base::FieldTrialList::CreateFieldTrial(trial_name1, group_name1);

  const std::string trial_name2 = "DonutExperiment";
  const std::string group_name2 = "MapleBacon";
  base::FieldTrial* trial2 =
      base::FieldTrialList::CreateFieldTrial(trial_name2, group_name2);

  service.RegisterMetricsProvider(
      std::make_unique<ExperimentTestMetricsProvider>(trial1, trial2));

  service.InitializeMetricsRecordingState();
  service.Start();
  service.StageCurrentLogForTest();

  MetricsLogStore* test_log_store = service.LogStoreForTest();
  ChromeUserMetricsExtension uma_log;
  EXPECT_TRUE(DecodeLogDataToProto(test_log_store->staged_log(), &uma_log));

  // Verify that the reported FieldTrial IDs are for the trial set up by this
  // test.
  EXPECT_TRUE(
      IsFieldTrialPresent(uma_log.system_profile(), trial_name1, group_name1));
  EXPECT_TRUE(
      IsFieldTrialPresent(uma_log.system_profile(), trial_name2, group_name2));
}

TEST_F(MetricsServiceTest, SystemProfileDataProvidedOnEnableRecording) {
  EnableMetricsReporting();
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());

  TestMetricsProvider* test_provider = new TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<MetricsProvider>(test_provider));

  service.InitializeMetricsRecordingState();

  // ProvideSystemProfileMetrics() shouldn't be called initially.
  EXPECT_FALSE(test_provider->provide_system_profile_metrics_called());
  EXPECT_FALSE(service.persistent_system_profile_provided());

  service.Start();

  // Start should call ProvideSystemProfileMetrics().
  EXPECT_TRUE(test_provider->provide_system_profile_metrics_called());
  EXPECT_TRUE(service.persistent_system_profile_provided());
  EXPECT_FALSE(service.persistent_system_profile_complete());
}

TEST_F(MetricsServiceTest, SplitRotation) {
  EnableMetricsReporting();
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());
  service.InitializeMetricsRecordingState();
  service.Start();
  // Rotation loop should create a log and mark state as idle.
  // Upload loop should start upload or be restarted.
  // The independent-metrics upload job will be started and always be a task.
  task_runner_->RunPendingTasks();
  // Rotation loop should terminated due to being idle.
  // Upload loop should start uploading if it isn't already.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client.uploader()->is_uploading());
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  service.OnApplicationNotIdle();
  EXPECT_TRUE(client.uploader()->is_uploading());
  EXPECT_EQ(2U, task_runner_->NumPendingTasks());
  // Log generation should be suppressed due to unsent log.
  // Idle state should not be reset.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client.uploader()->is_uploading());
  EXPECT_EQ(2U, task_runner_->NumPendingTasks());
  // Make sure idle state was not reset.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client.uploader()->is_uploading());
  EXPECT_EQ(2U, task_runner_->NumPendingTasks());
  // Upload should not be rescheduled, since there are no other logs.
  client.uploader()->CompleteUpload(200);
  EXPECT_FALSE(client.uploader()->is_uploading());
  EXPECT_EQ(2U, task_runner_->NumPendingTasks());
  // Running should generate a log, restart upload loop, and mark idle.
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(client.uploader()->is_uploading());
  EXPECT_EQ(3U, task_runner_->NumPendingTasks());
  // Upload should start, and rotation loop should idle out.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client.uploader()->is_uploading());
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
}

TEST_F(MetricsServiceTest, LastLiveTimestamp) {
  EnableMetricsReporting();
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());

  base::Time initial_last_live_time =
      GetLocalState()->GetTime(prefs::kStabilityBrowserLastLiveTimeStamp);

  service.InitializeMetricsRecordingState();
  service.Start();

  task_runner_->RunPendingTasks();
  size_t num_pending_tasks = task_runner_->NumPendingTasks();

  service.StartUpdatingLastLiveTimestamp();

  // Starting the update sequence should not write anything, but should
  // set up for a later write.
  EXPECT_EQ(
      initial_last_live_time,
      GetLocalState()->GetTime(prefs::kStabilityBrowserLastLiveTimeStamp));
  EXPECT_EQ(num_pending_tasks + 1, task_runner_->NumPendingTasks());

  // To avoid flakiness, yield until we're over a microsecond threshold.
  YieldUntil(initial_last_live_time + base::Microseconds(2));

  task_runner_->RunPendingTasks();

  // Verify that the time has updated in local state.
  base::Time updated_last_live_time =
      GetLocalState()->GetTime(prefs::kStabilityBrowserLastLiveTimeStamp);
  EXPECT_LT(initial_last_live_time, updated_last_live_time);

  // Double check that an update schedules again...
  YieldUntil(updated_last_live_time + base::Microseconds(2));

  task_runner_->RunPendingTasks();
  EXPECT_LT(
      updated_last_live_time,
      GetLocalState()->GetTime(prefs::kStabilityBrowserLastLiveTimeStamp));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(MetricsServiceTest,
       OngoingLogNotFlushedBeforeInitialLogWhenUserLogStoreSet) {
  EnableMetricsReporting();
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());

  service.InitializeMetricsRecordingState();
  // Start() will create the first ongoing log.
  service.Start();

  MetricsLogStore* test_log_store = service.LogStoreForTest();
  std::unique_ptr<TestUnsentLogStore> alternate_ongoing_log_store =
      InitializeTestLogStoreAndGet();
  TestUnsentLogStore* alternate_ongoing_log_store_ptr =
      alternate_ongoing_log_store.get();

  ASSERT_EQ(0u, test_log_store->initial_log_count());
  ASSERT_EQ(0u, test_log_store->ongoing_log_count());

  service.SetUserLogStore(std::move(alternate_ongoing_log_store));

  // Initial logs should not have been collected so the ongoing log being
  // recorded should not be flushed when a user log store is mounted.
  ASSERT_EQ(0u, test_log_store->initial_log_count());
  ASSERT_EQ(0u, test_log_store->ongoing_log_count());

  // Run pending tasks to finish init task and complete the first ongoing log.
  task_runner_->RunPendingTasks();
  ASSERT_EQ(TestMetricsService::SENDING_LOGS, service.state());
  // When the init task is complete, the first ongoing log should be created
  // in the alternate ongoing log store.
  EXPECT_EQ(0u, test_log_store->initial_log_count());
  EXPECT_EQ(0u, test_log_store->ongoing_log_count());
  EXPECT_EQ(1u, alternate_ongoing_log_store_ptr->size());
}

TEST_F(MetricsServiceTest,
       OngoingLogFlushedAfterInitialLogWhenUserLogStoreSet) {
  EnableMetricsReporting();
  TestMetricsServiceClient client;
  TestMetricsService service(GetMetricsStateManager(), &client,
                             GetLocalState());

  service.InitializeMetricsRecordingState();
  // Start() will create the first ongoing log.
  service.Start();

  MetricsLogStore* test_log_store = service.LogStoreForTest();
  std::unique_ptr<TestUnsentLogStore> alternate_ongoing_log_store =
      InitializeTestLogStoreAndGet();

  // Init state.
  ASSERT_EQ(0u, test_log_store->initial_log_count());
  ASSERT_EQ(0u, test_log_store->ongoing_log_count());

  // Run pending tasks to finish init task and complete the first ongoing log.
  task_runner_->RunPendingTasks();
  ASSERT_EQ(TestMetricsService::SENDING_LOGS, service.state());
  ASSERT_EQ(0u, test_log_store->initial_log_count());
  ASSERT_EQ(1u, test_log_store->ongoing_log_count());

  // User log store set post-init.
  service.SetUserLogStore(std::move(alternate_ongoing_log_store));

  // Another log should have been flushed from setting the user log store.
  ASSERT_EQ(0u, test_log_store->initial_log_count());
  ASSERT_EQ(2u, test_log_store->ongoing_log_count());
}
#endif

}  // namespace metrics
