// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_id_provider_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/federated_learning/floc_event_logger.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/federated_learning/features/features.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom.h"

namespace federated_learning {

namespace {

using ComputeFlocResult = FlocIdProviderImpl::ComputeFlocResult;
using ComputeFlocCompletedCallback =
    FlocIdProviderImpl::ComputeFlocCompletedCallback;
using CanComputeFlocCallback = FlocIdProviderImpl::CanComputeFlocCallback;

class MockFlocSortingLshService : public FlocSortingLshClustersService {
 public:
  using FlocSortingLshClustersService::FlocSortingLshClustersService;

  void ConfigureSortingLsh(
      const std::unordered_map<uint64_t, base::Optional<uint64_t>>&
          sorting_lsh_map,
      base::Version version) {
    sorting_lsh_map_ = sorting_lsh_map;
    version_ = version;
  }

  void ApplySortingLsh(uint64_t sim_hash,
                       ApplySortingLshCallback callback) override {
    if (sorting_lsh_map_.count(sim_hash)) {
      std::move(callback).Run(sorting_lsh_map_.at(sim_hash), version_);
      return;
    }

    std::move(callback).Run(base::nullopt, version_);
  }

 private:
  std::unordered_map<uint64_t, base::Optional<uint64_t>> sorting_lsh_map_;
  base::Version version_;
};

class FakeCookieSettings : public content_settings::CookieSettings {
 public:
  using content_settings::CookieSettings::CookieSettings;

  void GetCookieSettingInternal(const GURL& url,
                                const GURL& first_party_url,
                                bool is_third_party_request,
                                content_settings::SettingSource* source,
                                ContentSetting* cookie_setting) const override {
    *cookie_setting =
        allow_cookies_internal_ ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  }

  bool ShouldBlockThirdPartyCookies() const override {
    return should_block_third_party_cookies_;
  }

  void set_should_block_third_party_cookies(
      bool should_block_third_party_cookies) {
    should_block_third_party_cookies_ = should_block_third_party_cookies;
  }

  void set_allow_cookies_internal(bool allow_cookies_internal) {
    allow_cookies_internal_ = allow_cookies_internal;
  }

 private:
  ~FakeCookieSettings() override = default;

  bool should_block_third_party_cookies_ = false;
  bool allow_cookies_internal_ = true;
};

class MockFlocIdProvider : public FlocIdProviderImpl {
 public:
  using FlocIdProviderImpl::FlocIdProviderImpl;

  void OnComputeFlocCompleted(ComputeFlocResult result) override {
    if (should_pause_before_compute_floc_completed_) {
      DCHECK(!paused_);
      paused_ = true;
      paused_result_ = result;
      return;
    }

    ++compute_floc_completed_count_;
    FlocIdProviderImpl::OnComputeFlocCompleted(result);
  }

  void ContinueLastOnComputeFlocCompleted() {
    DCHECK(paused_);
    paused_ = false;
    ++compute_floc_completed_count_;
    FlocIdProviderImpl::OnComputeFlocCompleted(paused_result_);
  }

  void LogFlocComputedEvent(const ComputeFlocResult& result) override {
    ++log_event_count_;
    last_log_event_result_ = result;
  }

  size_t compute_floc_completed_count() const {
    return compute_floc_completed_count_;
  }

  void set_should_pause_before_compute_floc_completed(bool should_pause) {
    should_pause_before_compute_floc_completed_ = should_pause;
  }

  ComputeFlocResult paused_result() const {
    DCHECK(paused_);
    return paused_result_;
  }

  size_t log_event_count() const { return log_event_count_; }

  ComputeFlocResult last_log_event_result() const {
    DCHECK_LT(0u, log_event_count_);
    return last_log_event_result_;
  }

 private:
  base::OnceCallback<void()> callback_before_compute_floc_completed_;

  // Add the support to be able to pause on the OnComputeFlocCompleted
  // execution and let it yield to other tasks posted to the same task runner.
  bool should_pause_before_compute_floc_completed_ = false;
  bool paused_ = false;
  ComputeFlocResult paused_result_;

  size_t compute_floc_completed_count_ = 0u;
  size_t log_event_count_ = 0u;
  ComputeFlocResult last_log_event_result_;
};

}  // namespace

// Used to create a floc id with non-current finch_config_version or
// compute_time.
class FlocIdTester {
 public:
  static FlocId Create(base::Optional<uint64_t> id,
                       base::Time history_begin_time,
                       base::Time history_end_time,
                       uint32_t finch_config_version,
                       uint32_t sorting_lsh_version,
                       base::Time compute_time) {
    return FlocId(id, history_begin_time, history_end_time,
                  finch_config_version, sorting_lsh_version, compute_time);
  }
};

class FlocIdProviderUnitTest : public testing::Test {
 public:
  FlocIdProviderUnitTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~FlocIdProviderUnitTest() override = default;

  void SetUp() override {
    FlocId::RegisterPrefs(prefs_.registry());
    privacy_sandbox::RegisterProfilePrefs(prefs_.registry());

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    content_settings::ContentSettingsRegistry::GetInstance()->ResetForTest();
    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = new HostContentSettingsMap(
        &prefs_, /*is_off_the_record=*/false, /*store_last_modified=*/false,
        /*restore_session=*/false);

    auto sorting_lsh_service = std::make_unique<MockFlocSortingLshService>();
    sorting_lsh_service_ = sorting_lsh_service.get();
    TestingBrowserProcess::GetGlobal()->SetFlocSortingLshClustersService(
        std::move(sorting_lsh_service));

    history_service_ = std::make_unique<history::HistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));

    fake_cookie_settings_ = base::MakeRefCounted<FakeCookieSettings>(
        settings_map_.get(), &prefs_, false, "chrome-extension");

    privacy_sandbox_settings_ = std::make_unique<PrivacySandboxSettings>(
        settings_map_.get(), fake_cookie_settings_.get(), &prefs_,
        &mock_policy_service_,
        /*sync_service=*/nullptr, /*identity_manager=*/nullptr);

    task_environment_.RunUntilIdle();
  }

  void InitializeFlocIdProvider() {
    floc_id_provider_ = std::make_unique<MockFlocIdProvider>(
        &prefs_, privacy_sandbox_settings_.get(), history_service_.get(),
        nullptr);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    settings_map_->ShutdownOnUIThread();
    history_service_->RemoveObserver(floc_id_provider_.get());
  }

  void ApplySortingLshPostProcessing(ComputeFlocCompletedCallback callback,
                                     uint64_t sim_hash,
                                     base::Time history_begin_time,
                                     base::Time history_end_time) {
    floc_id_provider_->ApplySortingLshPostProcessing(
        std::move(callback), sim_hash, history_begin_time, history_end_time);
  }

  void CheckCanComputeFloc(CanComputeFlocCallback callback) {
    floc_id_provider_->CheckCanComputeFloc(std::move(callback));
  }

  void OnFlocDataAccessibleSinceUpdated() {
    floc_id_provider_->OnFlocDataAccessibleSinceUpdated();
  }

  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) {
    floc_id_provider_->OnURLsDeleted(history_service, deletion_info);
  }

  void OnGetRecentlyVisitedURLsCompleted(history::QueryResults results) {
    auto compute_floc_completed_callback =
        base::BindOnce(&FlocIdProviderImpl::OnComputeFlocCompleted,
                       base::Unretained(floc_id_provider_.get()));

    floc_id_provider_->OnGetRecentlyVisitedURLsCompleted(
        std::move(compute_floc_completed_callback), std::move(results));
  }

  void AddHistoryEntriesForDomains(const std::vector<std::string>& domains,
                                   base::Time time) {
    history::HistoryAddPageArgs add_page_args;
    add_page_args.time = time;
    add_page_args.floc_allowed = true;

    for (const std::string& domain : domains) {
      add_page_args.url = GURL(base::StrCat({"https://www.", domain}));
      history_service_->AddPage(add_page_args);
    }
  }

  void ExpireHistoryBeforeUninclusive(base::Time end_time) {
    base::CancelableTaskTracker tracker;
    base::RunLoop run_loop;
    history_service_->ExpireHistoryBetween(
        /*restrict_urls=*/{}, /*begin_time=*/base::Time(), end_time,
        /*user_initiated=*/true, run_loop.QuitClosure(), &tracker);
    run_loop.Run();
  }

  FlocId floc_id() const { return floc_id_provider_->floc_id_; }

  void set_floc_id(const FlocId& floc_id) const {
    floc_id_provider_->floc_id_ = floc_id;
  }

  bool floc_computation_in_progress() const {
    return floc_id_provider_->floc_computation_in_progress_;
  }

  void set_floc_computation_in_progress(bool floc_computation_in_progress) {
    floc_id_provider_->floc_computation_in_progress_ =
        floc_computation_in_progress;
  }

  bool floc_computation_scheduled() const {
    return floc_id_provider_->compute_floc_timer_.IsRunning();
  }

  void set_floc_id(const FlocId& floc_id) {
    floc_id_provider_->floc_id_ = floc_id;
  }

  bool need_recompute() { return floc_id_provider_->need_recompute_; }

 protected:
  base::test::ScopedFeatureList feature_list_;

  content::BrowserTaskEnvironment task_environment_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;

  std::unique_ptr<history::HistoryService> history_service_;
  scoped_refptr<FakeCookieSettings> fake_cookie_settings_;
  testing::NiceMock<policy::MockPolicyService> mock_policy_service_;
  std::unique_ptr<PrivacySandboxSettings> privacy_sandbox_settings_;
  std::unique_ptr<MockFlocIdProvider> floc_id_provider_;

  MockFlocSortingLshService* sorting_lsh_service_;

  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(FlocIdProviderUnitTest);
};

TEST_F(FlocIdProviderUnitTest, DefaultSetup_BelowMinimumHistoryDomainSize) {
  const base::Time kSevenDaysBeforeStart =
      base::Time::Now() - base::TimeDelta::FromDays(7);

  AddHistoryEntriesForDomains({"foo.com", "bar.com"}, kSevenDaysBeforeStart);

  // Initializing the floc provider should trigger an immediate computation.
  InitializeFlocIdProvider();
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed with an invalid floc, due
  // to insufficient history domains.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderUnitTest, DefaultSetup_MinimumHistoryDomainSize) {
  const base::Time kSevenDaysBeforeStart =
      base::Time::Now() - base::TimeDelta::FromDays(7);

  AddHistoryEntriesForDomains({"foo.com", "bar.com", "baz.com"},
                              kSevenDaysBeforeStart);

  // Initializing the floc provider should trigger an immediate computation.
  InitializeFlocIdProvider();
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed with the expected floc.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId(FlocId::SimHashHistory({"foo.com", "bar.com", "baz.com"}),
                   kSevenDaysBeforeStart, kSevenDaysBeforeStart, 0),
            floc_id());
}

TEST_F(FlocIdProviderUnitTest, DefaultSetup_ScheduledUpdateInterval) {
  const base::Time kSevenDaysBeforeStart =
      base::Time::Now() - base::TimeDelta::FromDays(7);

  AddHistoryEntriesForDomains({"foo.com", "bar.com", "baz.com"},
                              kSevenDaysBeforeStart);

  // Initializing the floc provider should trigger an immediate computation.
  InitializeFlocIdProvider();
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId(FlocId::SimHashHistory({"foo.com", "bar.com", "baz.com"}),
                   kSevenDaysBeforeStart, kSevenDaysBeforeStart, 0),
            floc_id());
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_computation_scheduled());

  // Advance the clock by 6 days.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(6));

  // Add 3 history entries with a new set of domains.
  const base::Time kSixDaysAfterStart = base::Time::Now();
  AddHistoryEntriesForDomains({"bar.com", "baz.com", "qux.com"},
                              kSixDaysAfterStart);

  task_environment_.RunUntilIdle();

  // Advance the clock by 23 hours. Expect no more computation, as the floc id
  // refresh interval is 7 days.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(23));

  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());

  // Advance the clock by 1 hour. Expect one more computation.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));

  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());

  EXPECT_EQ(FlocId(FlocId::SimHashHistory({"bar.com", "baz.com", "qux.com"}),
                   kSixDaysAfterStart, kSixDaysAfterStart, 0),
            floc_id());
}

class FlocIdProviderSimpleFeatureParamUnitTest : public FlocIdProviderUnitTest {
 public:
  FlocIdProviderSimpleFeatureParamUnitTest() {
    feature_list_.Reset();
    feature_list_.InitAndEnableFeatureWithParameters(
        kFederatedLearningOfCohorts,
        {{"update_interval", "24h"},
         {"minimum_history_domain_size_required", "1"}});
  }
};

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest, QualifiedInitialHistory) {
  const base::Time kTime = base::Time::Now() - base::TimeDelta::FromDays(7);

  AddHistoryEntriesForDomains({"foo.com"}, kTime);

  // Initializing the floc provider should trigger an immediate computation.
  InitializeFlocIdProvider();
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId(FlocId::SimHashHistory({"foo.com"}), kTime, kTime, 0),
            floc_id());
  EXPECT_TRUE(floc_id_provider_->last_log_event_result().sim_hash_computed);
  EXPECT_EQ(FlocId::SimHashHistory({"foo.com"}),
            floc_id_provider_->last_log_event_result().sim_hash);
  EXPECT_EQ(base::Time::Now(),
            floc_id_provider_->last_log_event_result().floc_id.compute_time());
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_computation_scheduled());

  // Advance the clock by 1 day. Expect a computation, as there's no history in
  // the last 7 days so the id has been reset to empty.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());
  EXPECT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest, UnqualifiedInitialHistory) {
  AddHistoryEntriesForDomains({"foo.com"},
                              base::Time::Now() - base::TimeDelta::FromDays(8));

  // Initializing the floc provider should trigger an immediate computation.
  InitializeFlocIdProvider();
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_computation_scheduled());

  const base::Time kTime = base::Time::Now() - base::TimeDelta::FromDays(6);
  AddHistoryEntriesForDomains({"foo.com"}, kTime);

  // Advance the clock by 23 hours. Expect no more computation, as the id
  // refresh interval is 24 hours.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(23));

  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());

  // Advance the clock by 1 hour. Expect one more computation, as the refresh
  // time is reached and there's a valid history entry in the last 7 days.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));

  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());

  EXPECT_EQ(FlocId(FlocId::SimHashHistory({"foo.com"}), kTime, kTime, 0),
            floc_id());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       HistoryQueryBoundedByFlocAccessibleSince) {
  const base::Time kStartTime = base::Time::Now();
  const base::Time kSevenDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(7);
  const base::Time kSixDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(6);

  prefs_.SetTime(prefs::kPrivacySandboxFlocDataAccessibleSince,
                 kSixDaysBeforeStart);

  AddHistoryEntriesForDomains({"foo.com"}, kSevenDaysBeforeStart);
  AddHistoryEntriesForDomains({"bar.com"}, kSixDaysBeforeStart);

  // Initializing the floc provider should trigger an immediate computation.
  InitializeFlocIdProvider();
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_computation_scheduled());

  // Expected that floc is calculated from only "bar.com".
  EXPECT_EQ(FlocId(FlocId::SimHashHistory({"bar.com"}), kSixDaysBeforeStart,
                   kSixDaysBeforeStart, 0),
            floc_id());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       FlocAccessibleSinceViolationOnStartup) {
  const base::Time kStartTime = base::Time::Now();
  const base::Time kSevenDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(7);
  const base::Time kSixDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(6);
  const base::Time kTwelveHoursBeforeStart =
      kStartTime - base::TimeDelta::FromHours(12);

  FlocId floc_id_in_prefs_before_start =
      FlocIdTester::Create(123, kSevenDaysBeforeStart, kSixDaysBeforeStart, 1,
                           0, kTwelveHoursBeforeStart);
  floc_id_in_prefs_before_start.SaveToPrefs(&prefs_);

  prefs_.SetTime(prefs::kPrivacySandboxFlocDataAccessibleSince,
                 kSixDaysBeforeStart);

  AddHistoryEntriesForDomains({"foo.com"}, kSevenDaysBeforeStart);
  AddHistoryEntriesForDomains({"bar.com"}, kSixDaysBeforeStart);

  // Initializing the floc provider should invalidate the previous floc but
  // should not trigger an immediate computation.
  InitializeFlocIdProvider();
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_computation_scheduled());

  EXPECT_FALSE(floc_id().IsValid());
  EXPECT_FALSE(FlocId::ReadFromPrefs(&prefs_).IsValid());

  // Fast forward by 12 hours. This should trigger a scheduled update.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(12));

  // Expect a completed computation and an update to the local prefs.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_computation_scheduled());

  EXPECT_EQ(floc_id(), FlocId(FlocId::SimHashHistory({"bar.com"}),
                              kSixDaysBeforeStart, kSixDaysBeforeStart, 0));
  EXPECT_EQ(floc_id(), FlocId::ReadFromPrefs(&prefs_));
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       HistoryDeleteAndScheduledUpdate) {
  const base::Time kStartTime = base::Time::Now();
  const base::Time kSevenDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(7);
  const base::Time kSixDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(6);

  AddHistoryEntriesForDomains({"foo.com"}, kSevenDaysBeforeStart);
  AddHistoryEntriesForDomains({"bar.com"}, kSixDaysBeforeStart);

  // Initializing the floc provider should trigger an immediate computation.
  InitializeFlocIdProvider();
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId(FlocId::SimHashHistory({"foo.com", "bar.com"}),
                   kSevenDaysBeforeStart, kSixDaysBeforeStart, 0),
            floc_id());

  // Advance the clock by 12 hours. Expect no more computation.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(12));
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());

  // Expire the oldest history entry.
  ExpireHistoryBeforeUninclusive(kSixDaysBeforeStart);
  task_environment_.RunUntilIdle();

  // Expect that the floc has been invalidated. Expect no more floc computation,
  // but one more logging.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());
  EXPECT_FALSE(floc_id().IsValid());

  // Advance the clock by 12 hours. Expect one more computation, which implies
  // the timer didn't get reset due to the history invalidation. Expect that
  // the floc is derived from "bar.com".
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(12));
  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(3u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId(FlocId::SimHashHistory({"bar.com"}), kSixDaysBeforeStart,
                   kSixDaysBeforeStart, 0),
            floc_id());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest, ScheduledUpdateSameFloc) {
  const base::Time kTime = base::Time::Now() - base::TimeDelta::FromDays(2);

  AddHistoryEntriesForDomains({"foo.com"}, kTime);

  // Initializing the floc provider should trigger an immediate computation.
  InitializeFlocIdProvider();
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId(FlocId::SimHashHistory({"foo.com"}), kTime, kTime, 0),
            floc_id());

  // Advance the clock by 1 day. Expect one more computation, but the floc
  // didn't change.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId(FlocId::SimHashHistory({"foo.com"}), kTime, kTime, 0),
            floc_id());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       CheckCanComputeFloc_Default_Success) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  base::OnceCallback<void(bool)> cb = base::BindOnce(
      [](bool can_compute_floc) { EXPECT_TRUE(can_compute_floc); });

  CheckCanComputeFloc(std::move(cb));
  task_environment_.RunUntilIdle();
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       CheckCanComputeFloc_Failure_BlockThirdPartyCookies) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  fake_cookie_settings_->set_should_block_third_party_cookies(true);

  base::OnceCallback<void(bool)> cb = base::BindOnce(
      [](bool can_compute_floc) { EXPECT_FALSE(can_compute_floc); });

  CheckCanComputeFloc(std::move(cb));
  task_environment_.RunUntilIdle();
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       OnFlocDataAccessibleSinceUpdated_TimeRangeNotFullyCovered) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  const base::Time kTime1 = base::Time::FromTimeT(1);
  const base::Time kTime2 = base::Time::FromTimeT(2);

  set_floc_id(FlocId(123, kTime1, kTime2, 0));

  prefs_.SetTime(prefs::kPrivacySandboxFlocDataAccessibleSince, kTime2);
  OnFlocDataAccessibleSinceUpdated();

  EXPECT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       OnFlocDataAccessibleSinceUpdated_TimeRangeFullyCovered) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  const base::Time kTime1 = base::Time::FromTimeT(1);
  const base::Time kTime2 = base::Time::FromTimeT(2);

  set_floc_id(FlocId(123, kTime1, kTime2, 0));

  prefs_.SetTime(prefs::kPrivacySandboxFlocDataAccessibleSince, kTime1);
  OnFlocDataAccessibleSinceUpdated();

  EXPECT_TRUE(floc_id().IsValid());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest, HistoryDelete_AllHistory) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  const base::Time kTime1 = base::Time::FromTimeT(1);
  const base::Time kTime2 = base::Time::FromTimeT(2);

  set_floc_id(FlocId(123, kTime1, kTime2, 0));

  OnURLsDeleted(history_service_.get(), history::DeletionInfo::ForAllHistory());

  EXPECT_FALSE(floc_id().IsValid());

  // Check the logged event for history-delete.
  EXPECT_FALSE(floc_id_provider_->last_log_event_result().sim_hash_computed);
  EXPECT_EQ(0u, floc_id_provider_->last_log_event_result().sim_hash);
  EXPECT_EQ(base::Time::Now(),
            floc_id_provider_->last_log_event_result().floc_id.compute_time());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       HistoryDelete_InvalidTimeRange) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  const base::Time kTime1 = base::Time::FromTimeT(1);
  const base::Time kTime2 = base::Time::FromTimeT(2);

  GURL url_a = GURL("https://a.test");

  history::URLResult url_result(url_a, kTime1);
  url_result.set_floc_allowed(true);

  history::QueryResults query_results;
  query_results.SetURLResults({url_result});

  const FlocId expected_floc =
      FlocId(FlocId::SimHashHistory({"a.test"}), kTime1, kTime2, 0);

  set_floc_id(expected_floc);

  OnURLsDeleted(history_service_.get(),
                history::DeletionInfo::ForUrls(
                    {history::URLResult(url_a, kTime1)}, /*favicon_urls=*/{}));

  EXPECT_EQ(expected_floc, floc_id());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       HistoryDelete_TimeRangeNoOverlap) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  const base::Time kTime1 = base::Time::FromTimeT(1);
  const base::Time kTime2 = base::Time::FromTimeT(2);
  const base::Time kTime3 = base::Time::FromTimeT(3);
  const base::Time kTime4 = base::Time::FromTimeT(4);

  const FlocId expected_floc =
      FlocId(FlocId::SimHashHistory({"a.test"}), kTime1, kTime2, 0);

  set_floc_id(expected_floc);

  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(kTime3, kTime4),
      /*is_from_expiration=*/false, /*deleted_rows=*/{}, /*favicon_urls=*/{},
      /*restrict_urls=*/base::nullopt);
  OnURLsDeleted(history_service_.get(), deletion_info);

  EXPECT_EQ(expected_floc, floc_id());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       HistoryDelete_TimeRangePartialOverlap) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  const base::Time kTime1 = base::Time::FromTimeT(1);
  const base::Time kTime2 = base::Time::FromTimeT(2);
  const base::Time kTime3 = base::Time::FromTimeT(3);

  const FlocId expected_floc =
      FlocId(FlocId::SimHashHistory({"a.test"}), kTime1, kTime2, 0);

  set_floc_id(expected_floc);

  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(kTime2, kTime3),
      /*is_from_expiration=*/false, /*deleted_rows=*/{}, /*favicon_urls=*/{},
      /*restrict_urls=*/base::nullopt);
  OnURLsDeleted(history_service_.get(), deletion_info);

  EXPECT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       HistoryDelete_TimeRangeFullOverlap) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  const base::Time kTime1 = base::Time::FromTimeT(1);
  const base::Time kTime2 = base::Time::FromTimeT(2);

  const FlocId expected_floc =
      FlocId(FlocId::SimHashHistory({"a.test"}), kTime1, kTime2, 0);

  set_floc_id(expected_floc);

  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(kTime1, kTime2),
      /*is_from_expiration=*/false, /*deleted_rows=*/{}, /*favicon_urls=*/{},
      /*restrict_urls=*/base::nullopt);
  OnURLsDeleted(history_service_.get(), deletion_info);

  EXPECT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest, FlocIneligibleHistoryEntries) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  history::QueryResults query_results;
  query_results.SetURLResults(
      {history::URLResult(GURL("https://a.test"),
                          base::Time::Now() - base::TimeDelta::FromDays(1))});

  set_floc_computation_in_progress(true);

  OnGetRecentlyVisitedURLsCompleted(std::move(query_results));

  EXPECT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest, MultipleHistoryEntries) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  const base::Time kTime1 = base::Time::FromTimeT(1);
  const base::Time kTime2 = base::Time::FromTimeT(2);
  const base::Time kTime3 = base::Time::FromTimeT(3);

  history::URLResult url_result_a(GURL("https://a.test"), kTime1);
  url_result_a.set_floc_allowed(true);

  history::URLResult url_result_b(GURL("https://b.test"), kTime2);
  url_result_b.set_floc_allowed(true);

  history::URLResult url_result_c(GURL("https://c.test"), kTime3);

  std::vector<history::URLResult> url_results{url_result_a, url_result_b,
                                              url_result_c};

  history::QueryResults query_results;
  query_results.SetURLResults(std::move(url_results));

  set_floc_computation_in_progress(true);

  OnGetRecentlyVisitedURLsCompleted(std::move(query_results));

  EXPECT_EQ(
      FlocId(FlocId::SimHashHistory({"a.test", "b.test"}), kTime1, kTime2, 0),
      floc_id());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       MaybeRecordFlocToUkmMethod_FilteredRecording) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  InitializeFlocIdProvider();

  floc_id_provider_->MaybeRecordFlocToUkm(1);

  // Initially the |need_ukm_recording_| is false. The recording attempt should
  // have been filtered. Expect no events.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::FlocPageLoad::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       MaybeRecordFlocToUkmMethod_RecordInvalidFloc) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  InitializeFlocIdProvider();

  floc_id_provider_->OnComputeFlocCompleted(ComputeFlocResult());
  floc_id_provider_->MaybeRecordFlocToUkm(1);

  // Expect an event with a missing metric, meaning the floc id is invalid.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::FlocPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entries.back(), ukm::builders::FlocPageLoad::kFlocIdName));
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       MaybeRecordFlocToUkmMethod_RecordValidFloc) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  InitializeFlocIdProvider();

  floc_id_provider_->OnComputeFlocCompleted(ComputeFlocResult(
      /*sim_hash=*/123, FlocIdTester::Create(123, base::Time::FromTimeT(4),
                                             base::Time::FromTimeT(5), 6, 7,
                                             base::Time::FromTimeT(8))));
  floc_id_provider_->MaybeRecordFlocToUkm(1);

  // Expect an event with a metric having the expected floc value.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::FlocPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(entries.back(),
                                 ukm::builders::FlocPageLoad::kFlocIdName,
                                 /*expected_value=*/123);
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       MaybeRecordFlocToUkmMethod_MultipleRecordings) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  InitializeFlocIdProvider();

  floc_id_provider_->OnComputeFlocCompleted(ComputeFlocResult(
      /*sim_hash=*/123, FlocIdTester::Create(123, base::Time::FromTimeT(4),
                                             base::Time::FromTimeT(5), 6, 7,
                                             base::Time::FromTimeT(8))));
  floc_id_provider_->MaybeRecordFlocToUkm(1);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::FlocPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  // Subsequent recoding attempt will be filtered as the last recording has set
  // |need_ukm_recording_| to false.
  floc_id_provider_->MaybeRecordFlocToUkm(1);
  entries =
      ukm_recorder.GetEntriesByName(ukm::builders::FlocPageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  // Trigger a new floc computation completion.
  set_floc_computation_in_progress(true);
  floc_id_provider_->OnComputeFlocCompleted(ComputeFlocResult(
      /*sim_hash=*/456, FlocIdTester::Create(456, base::Time::FromTimeT(4),
                                             base::Time::FromTimeT(5), 6, 7,
                                             base::Time::FromTimeT(8))));
  floc_id_provider_->MaybeRecordFlocToUkm(1);

  // The new recording attempt should have succeeded.
  entries =
      ukm_recorder.GetEntriesByName(ukm::builders::FlocPageLoad::kEntryName);
  EXPECT_EQ(2u, entries.size());
  ukm_recorder.ExpectEntryMetric(entries.back(),
                                 ukm::builders::FlocPageLoad::kFlocIdName,
                                 /*expected_value=*/456);
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       GetInterestCohortForJsApiMethod) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  const base::Time kTime = base::Time::Now() - base::TimeDelta::FromDays(1);
  const FlocId expected_floc = FlocId(123, kTime, kTime, 999);

  set_floc_id(expected_floc);

  EXPECT_EQ(expected_floc.ToInterestCohortForJsApi(),
            floc_id_provider_->GetInterestCohortForJsApi(
                /*requesting_origin=*/{}, /*site_for_cookies=*/{}));
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       GetInterestCohortForJsApiMethod_ThirdPartyCookiesDisabled) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  fake_cookie_settings_->set_should_block_third_party_cookies(true);

  const base::Time kTime = base::Time::Now() - base::TimeDelta::FromDays(1);

  set_floc_id(FlocId(123, kTime, kTime, 999));

  EXPECT_EQ(blink::mojom::InterestCohort::New(),
            floc_id_provider_->GetInterestCohortForJsApi(
                /*requesting_origin=*/{}, /*site_for_cookies=*/{}));
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       GetInterestCohortForJsApiMethod_CookiesContentSettingsDisallowed) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  fake_cookie_settings_->set_allow_cookies_internal(false);

  const base::Time kTime = base::Time::Now() - base::TimeDelta::FromDays(1);

  set_floc_id(FlocId(123, kTime, kTime, 999));

  EXPECT_EQ(blink::mojom::InterestCohort::New(),
            floc_id_provider_->GetInterestCohortForJsApi(
                /*requesting_origin=*/{}, /*site_for_cookies=*/{}));
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       GetInterestCohortForJsApiMethod_FlocUnavailable) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(blink::mojom::InterestCohort::New(),
            floc_id_provider_->GetInterestCohortForJsApi(
                /*requesting_origin=*/{}, /*site_for_cookies=*/{}));
}

class FlocIdProviderUnitTestSortingLshEnabled
    : public FlocIdProviderSimpleFeatureParamUnitTest {
 public:
  FlocIdProviderUnitTestSortingLshEnabled() {
    feature_list_.Reset();
    feature_list_.InitWithFeaturesAndParameters(
        {{kFederatedLearningOfCohorts,
          {{"update_interval", "24h"},
           {"minimum_history_domain_size_required", "1"}}},
         {kFlocIdSortingLshBasedComputation, {}}},
        {});
  }
};

TEST_F(FlocIdProviderUnitTestSortingLshEnabled,
       HistoryDeleteDuringInProgressComputation) {
  const base::Time kStartTime = base::Time::Now();
  const base::Time kSevenDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(7);
  const base::Time kSixDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(6);
  const base::Time kFiveDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(5);

  AddHistoryEntriesForDomains({"foo.com"}, kSevenDaysBeforeStart);
  AddHistoryEntriesForDomains({"bar.com"}, kSixDaysBeforeStart);
  AddHistoryEntriesForDomains({"baz.com"}, kFiveDaysBeforeStart);

  // Initializing the floc provider should not trigger an immediate computation,
  // as the sorting-lsh file is not ready.
  InitializeFlocIdProvider();
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());

  // Map SimHashHistory({"foo.com", "bar.com", "baz.com"}) to 123.
  // Map SimHashHistory({"bar.com", "baz.com"}) to 456.
  // Map SimHashHistory({"baz.com"}) to 789.
  sorting_lsh_service_->ConfigureSortingLsh(
      {{FlocId::SimHashHistory({"foo.com", "bar.com", "baz.com"}), 123},
       {FlocId::SimHashHistory({"bar.com", "baz.com"}), 456},
       {FlocId::SimHashHistory({"baz.com"}), 789}},
      base::Version("999.0.0"));

  // Trigger the 1st floc computation.
  sorting_lsh_service_->OnSortingLshClustersFileReady(base::FilePath(),
                                                      base::Version());
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_TRUE(floc_id().IsValid());
  EXPECT_EQ(FlocId(123, kSevenDaysBeforeStart, kFiveDaysBeforeStart, 999),
            floc_id());

  base::Time time_before_advancing = base::Time::Now();

  // Advance the clock by 1 day. The "foo.com" should expire. However, we pause
  // before the computation completes.
  floc_id_provider_->set_should_pause_before_compute_floc_completed(true);
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(need_recompute());
  EXPECT_EQ(
      FlocIdTester::Create(123, kSevenDaysBeforeStart, kFiveDaysBeforeStart, 1,
                           999, time_before_advancing),
      floc_id());
  EXPECT_EQ(FlocId(456, kSixDaysBeforeStart, kFiveDaysBeforeStart, 999),
            floc_id_provider_->paused_result().floc_id);

  // Expire the "bar.com" history entry right before the floc computation
  // completes. Since the computation is still considered to be in-progress, we
  // will recompute right after this computation completes.
  ExpireHistoryBeforeUninclusive(kFiveDaysBeforeStart);

  EXPECT_TRUE(need_recompute());

  floc_id_provider_->set_should_pause_before_compute_floc_completed(false);
  floc_id_provider_->ContinueLastOnComputeFlocCompleted();
  task_environment_.RunUntilIdle();

  // Expect 2 more compute completion events and 1 more log event. This is
  // because we won't send log event if there's a recompute event scheduled.
  // The compute trigger should be the original trigger (i.e. kScheduledUpdate),
  // rather than kHistoryDelete.
  EXPECT_EQ(3u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());
  EXPECT_FALSE(need_recompute());

  // The final floc should be derived from "baz.com".
  EXPECT_TRUE(floc_id().IsValid());
  EXPECT_EQ(FlocId(789, kFiveDaysBeforeStart, kFiveDaysBeforeStart, 999),
            floc_id());
}

TEST_F(FlocIdProviderUnitTestSortingLshEnabled,
       ApplyAdditionalFiltering_SortingLsh) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  const base::Time kTime1 = base::Time::FromTimeT(1);
  const base::Time kTime2 = base::Time::FromTimeT(2);

  bool callback_called = false;
  auto callback = base::BindLambdaForTesting([&](ComputeFlocResult result) {
    EXPECT_FALSE(callback_called);
    EXPECT_EQ(result.sim_hash, 3u);
    EXPECT_EQ(result.floc_id, FlocId(2, kTime1, kTime2, 99));
    callback_called = true;
  });

  // Map 3 to 2
  sorting_lsh_service_->OnSortingLshClustersFileReady(base::FilePath(),
                                                      base::Version());
  sorting_lsh_service_->ConfigureSortingLsh({{3, 2}}, base::Version("99.0"));

  ApplySortingLshPostProcessing(std::move(callback), /*sim_hash=*/3, kTime1,
                                kTime2);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(FlocIdProviderUnitTestSortingLshEnabled,
       ApplySortingLshPostProcessing_FileCorrupted) {
  InitializeFlocIdProvider();
  task_environment_.RunUntilIdle();

  const base::Time kTime1 = base::Time::FromTimeT(1);
  const base::Time kTime2 = base::Time::FromTimeT(2);

  bool callback_called = false;
  auto callback = base::BindLambdaForTesting([&](ComputeFlocResult result) {
    EXPECT_FALSE(callback_called);
    EXPECT_EQ(result.sim_hash, 3u);
    EXPECT_EQ(result.floc_id, FlocId());
    callback_called = true;
  });

  sorting_lsh_service_->OnSortingLshClustersFileReady(base::FilePath(),
                                                      base::Version());
  sorting_lsh_service_->ConfigureSortingLsh({}, base::Version("3.4.5"));

  ApplySortingLshPostProcessing(std::move(callback), /*sim_hash=*/3, kTime1,
                                kTime2);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(FlocIdProviderUnitTestSortingLshEnabled, SortingLshPostProcessing) {
  const base::Time kTime = base::Time::Now() - base::TimeDelta::FromDays(1);

  AddHistoryEntriesForDomains({"foo.com"}, kTime);

  uint64_t sim_hash = FlocId::SimHashHistory({"foo.com"});

  // Initializing the floc provider should not trigger an immediate computation,
  // as the sorting-lsh file is not ready.
  InitializeFlocIdProvider();
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());

  // Configure the |sorting_lsh_service_| to map |sim_hash| to 12345.
  sorting_lsh_service_->ConfigureSortingLsh({{sim_hash, 12345}},
                                            base::Version("99.0"));

  // Trigger the sorting-lsh ready event to trigger the 1st floc computation.
  sorting_lsh_service_->OnSortingLshClustersFileReady(base::FilePath(),
                                                      base::Version());
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_computation_scheduled());

  // Expect a computation. The floc should be equal to 12345.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId(12345, kTime, kTime, 99), floc_id());

  // Configure the |sorting_lsh_service_| to block |sim_hash|.
  sorting_lsh_service_->ConfigureSortingLsh({{sim_hash, base::nullopt}},
                                            base::Version("3.4.5"));

  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  // Expect one more computation, where the result contains a valid sim_hash and
  // an invalid floc_id, as it was blocked. The internal floc is set to the
  // invalid one.
  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());
  EXPECT_TRUE(floc_id_provider_->last_log_event_result().sim_hash_computed);
  EXPECT_EQ(floc_id_provider_->last_log_event_result().sim_hash, sim_hash);
  EXPECT_FALSE(floc_id_provider_->last_log_event_result().floc_id.IsValid());
  EXPECT_FALSE(floc_id().IsValid());

  // Configure the |sorting_lsh_service_| to map |sim_hash| to 6789.
  sorting_lsh_service_->ConfigureSortingLsh({{sim_hash, 6789}},
                                            base::Version("999.0"));

  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  // Expect one more computation. The floc should be equal to 6789.
  EXPECT_EQ(3u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(3u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId(6789, kTime, kTime, 999), floc_id());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       LastFlocUnexpired_NextScheduledUpdate) {
  // Setups before session start.
  const base::Time kStartTime = base::Time::Now();
  const base::Time kFourDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(4);
  const base::Time kThreeDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(3);
  const base::Time kLastComputeTime =
      kStartTime - base::TimeDelta::FromHours(12);

  FlocId floc_id_in_prefs_before_start =
      FlocIdTester::Create(123, kFourDaysBeforeStart, kThreeDaysBeforeStart, 1,
                           999, kLastComputeTime);
  floc_id_in_prefs_before_start.SaveToPrefs(&prefs_);

  AddHistoryEntriesForDomains({"domain1.com"}, kFourDaysBeforeStart);
  AddHistoryEntriesForDomains({"domain2.com"}, kThreeDaysBeforeStart);

  // Start of session.
  InitializeFlocIdProvider();

  // Initially the floc is set to the entry from the prefs. No computation has
  // occurred for this session.
  EXPECT_EQ(floc_id(), floc_id_in_prefs_before_start);
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_computation_scheduled());
  EXPECT_EQ(0u, floc_id_provider_->compute_floc_completed_count());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();

  // Expect that the floc prefs hasn't changed at this stage.
  EXPECT_EQ(floc_id(), FlocId::ReadFromPrefs(&prefs_));

  // Fast forward by 12 hours. This should trigger a scheduled update.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(12));

  // Expect a completed computation and an update to the local prefs.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(floc_id(),
            FlocId(FlocId::SimHashHistory({"domain1.com", "domain2.com"}),
                   kFourDaysBeforeStart, kThreeDaysBeforeStart, 0));

  EXPECT_EQ(floc_id(), FlocId::ReadFromPrefs(&prefs_));
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       LastFlocUnexpired_HistoryDelete) {
  // Setups before session start.
  const base::Time kStartTime = base::Time::Now();
  const base::Time kFourDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(4);
  const base::Time kThreeDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(3);
  const base::Time kLastComputeTime =
      kStartTime - base::TimeDelta::FromHours(12);

  FlocId floc_id_in_prefs_before_start =
      FlocIdTester::Create(123, kFourDaysBeforeStart, kThreeDaysBeforeStart, 1,
                           999, kLastComputeTime);
  floc_id_in_prefs_before_start.SaveToPrefs(&prefs_);

  AddHistoryEntriesForDomains({"domain1.com"}, kFourDaysBeforeStart);
  AddHistoryEntriesForDomains({"domain2.com"}, kThreeDaysBeforeStart);

  // Start of session.
  InitializeFlocIdProvider();

  // Initially the floc is set to the entry from the prefs. No computation has
  // occurred for this session.
  EXPECT_EQ(floc_id(), floc_id_in_prefs_before_start);
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_computation_scheduled());
  EXPECT_EQ(0u, floc_id_provider_->compute_floc_completed_count());

  // Expire all previous history.
  ExpireHistoryBeforeUninclusive(base::Time::Now());

  // Expect no explicit recompute, but the floc has been invalidated and is
  // written to the local prefs. The last floc compute time in the pref hasn't
  // changed.
  EXPECT_EQ(0u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_FALSE(floc_id().IsValid());

  FlocId floc_id_in_prefs = FlocId::ReadFromPrefs(&prefs_);
  EXPECT_FALSE(floc_id_in_prefs.IsValid());
  EXPECT_EQ(kLastComputeTime, floc_id_in_prefs.compute_time());
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       LastFlocExpired_ImmediateCompute) {
  // Setups before session start.
  const base::Time kStartTime = base::Time::Now();
  const base::Time kTwentyDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(20);
  const base::Time kNineteenDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(19);
  const base::Time kTwoDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(2);
  const base::Time kLastComputeTime =
      kStartTime - base::TimeDelta::FromHours(25);

  FlocId floc_id_in_prefs_before_start =
      FlocIdTester::Create(123, kTwentyDaysBeforeStart,
                           kNineteenDaysBeforeStart, 1, 888, kLastComputeTime);
  floc_id_in_prefs_before_start.SaveToPrefs(&prefs_);

  AddHistoryEntriesForDomains({"domain1.com"}, kTwentyDaysBeforeStart);
  AddHistoryEntriesForDomains({"domain2.com"}, kNineteenDaysBeforeStart);
  AddHistoryEntriesForDomains({"foo.com"}, kTwoDaysBeforeStart);

  // Start of session.
  InitializeFlocIdProvider();

  FlocId initial_invalid_floc_id =
      FlocIdTester::Create(base::nullopt, kTwentyDaysBeforeStart,
                           kNineteenDaysBeforeStart, 1, 888, kLastComputeTime);

  // Initially the floc is invalidated as the last floc has expired, but other
  // fields remains unchanged. The invalidation is also written to the prefs.
  // Expect an immediate computation.
  EXPECT_EQ(floc_id(), initial_invalid_floc_id);
  EXPECT_EQ(FlocId::ReadFromPrefs(&prefs_), initial_invalid_floc_id);
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());
  EXPECT_EQ(0u, floc_id_provider_->compute_floc_completed_count());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_computation_scheduled());

  // Expect a completed computation and an update to the local prefs.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(floc_id(), FlocId(FlocId::SimHashHistory({"foo.com"}),
                              kTwoDaysBeforeStart, kTwoDaysBeforeStart, 0));
  EXPECT_EQ(floc_id(), FlocId::ReadFromPrefs(&prefs_));
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       NextComputeDelayTooBig_ImmediateCompute) {
  // Setups before session start.
  const base::Time kStartTime = base::Time::Now();
  const base::Time kFourDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(4);
  const base::Time kThreeDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(3);
  const base::Time kTwoDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(2);
  const base::Time kLastComputeTime = kStartTime + base::TimeDelta::FromDays(1);

  // Configure the last compute time to be 1 day after the start time, that
  // emulates the situation when the machine time has changed.
  FlocId floc_id_in_prefs_before_start =
      FlocIdTester::Create(123, kFourDaysBeforeStart, kThreeDaysBeforeStart, 1,
                           999, kLastComputeTime);
  floc_id_in_prefs_before_start.SaveToPrefs(&prefs_);

  AddHistoryEntriesForDomains({"foo.com"}, kTwoDaysBeforeStart);

  // Start of session.
  InitializeFlocIdProvider();

  FlocId initial_invalid_floc_id =
      FlocIdTester::Create(base::nullopt, kFourDaysBeforeStart,
                           kThreeDaysBeforeStart, 1, 999, kLastComputeTime);

  // Initially the floc is invalidated as the "presumed next computation delay"
  // >= "2 x the scheduled update interval", implying the machine time has
  // changed. Other fields should remain unchanged. The invalidation is also
  // written to the prefs. Expect an immediate computation.
  EXPECT_EQ(floc_id(), initial_invalid_floc_id);
  EXPECT_EQ(FlocId::ReadFromPrefs(&prefs_), initial_invalid_floc_id);
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());
  EXPECT_EQ(0u, floc_id_provider_->compute_floc_completed_count());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_computation_scheduled());

  // Expect a completed computation and an update to the local prefs.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(floc_id(), FlocId(FlocId::SimHashHistory({"foo.com"}),
                              kTwoDaysBeforeStart, kTwoDaysBeforeStart, 0));
  EXPECT_EQ(floc_id(), FlocId::ReadFromPrefs(&prefs_));
}

TEST_F(FlocIdProviderSimpleFeatureParamUnitTest,
       LastFlocVersionMismatch_ImmediateCompute) {
  // Setups before session start.
  const base::Time kStartTime = base::Time::Now();
  const base::Time kFourDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(4);
  const base::Time kThreeDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(3);
  const base::Time kTwoDaysBeforeStart =
      kStartTime - base::TimeDelta::FromDays(2);
  const base::Time kLastComputeTime =
      kStartTime - base::TimeDelta::FromHours(12);

  // Configure a floc with version finch_config_version 0, that is different
  // from the current version 1.
  FlocId floc_id_in_prefs_before_start =
      FlocIdTester::Create(123, kFourDaysBeforeStart, kThreeDaysBeforeStart, 0,
                           999, kLastComputeTime);
  floc_id_in_prefs_before_start.SaveToPrefs(&prefs_);

  AddHistoryEntriesForDomains({"foo.com"}, kTwoDaysBeforeStart);

  // Start of session.
  InitializeFlocIdProvider();

  FlocId initial_invalid_floc_id =
      FlocIdTester::Create(base::nullopt, kFourDaysBeforeStart,
                           kThreeDaysBeforeStart, 0, 999, kLastComputeTime);

  // Initially the floc is invalidated as the version mismatches, but other
  // fields remains unchanged. The invalidation is also written to the prefs.
  // Expect an immediate computation.
  EXPECT_EQ(floc_id(), initial_invalid_floc_id);
  EXPECT_EQ(FlocId::ReadFromPrefs(&prefs_), initial_invalid_floc_id);
  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(floc_computation_scheduled());
  EXPECT_EQ(0u, floc_id_provider_->compute_floc_completed_count());

  // Finish any outstanding history queries.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_computation_scheduled());

  // Expect a completed computation and an update to the local prefs.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(floc_id(), FlocId(FlocId::SimHashHistory({"foo.com"}),
                              kTwoDaysBeforeStart, kTwoDaysBeforeStart, 0));
  EXPECT_EQ(floc_id(), FlocId::ReadFromPrefs(&prefs_));
}

}  // namespace federated_learning
