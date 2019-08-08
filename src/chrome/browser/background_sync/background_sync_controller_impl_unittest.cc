// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_controller_impl.h"

#include <stdint.h>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/background_sync_parameters.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/background_sync_launcher_android.h"
#endif

namespace {

using content::BackgroundSyncController;

const char kFieldTrialGroup[] = "GroupA";
const char kExampleUrl[] = "https://www.example.com/foo/";

// Default min time gap between two periodic sync events for a given
// Periodic Background Sync registration.
constexpr base::TimeDelta kMinGapBetweenPeriodicSyncEvents =
    base::TimeDelta::FromHours(12);
constexpr base::TimeDelta kSmallerThanMinGap = base::TimeDelta::FromHours(11);
constexpr base::TimeDelta kLargerThanMinGap = base::TimeDelta::FromHours(13);

std::unique_ptr<KeyedService> BuildTestHistoryService(
    const base::FilePath& file_path,
    content::BrowserContext* context) {
  auto service = std::make_unique<history::HistoryService>();
  service->Init(history::TestHistoryDatabaseParamsForPath(file_path));
  return service;
}

class BackgroundSyncControllerImplTest : public testing::Test {
 protected:
  BackgroundSyncControllerImplTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
    ResetFieldTrialList();
#if defined(OS_ANDROID)
    BackgroundSyncLauncherAndroid::SetPlayServicesVersionCheckDisabledForTests(
        true);
#endif
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(
                       &BuildTestHistoryService,
                       temp_dir_.GetPath().AppendASCII("BackgroundSyncTest")));
    controller_ = std::make_unique<BackgroundSyncControllerImpl>(&profile_);
  }

  void ResetFieldTrialList() {
    field_trial_list_ =
        std::make_unique<base::FieldTrialList>(nullptr /* entropy provider */);
    variations::testing::ClearAllVariationParams();
    base::FieldTrialList::CreateFieldTrial(
        BackgroundSyncControllerImpl::kFieldTrialName, kFieldTrialGroup);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<BackgroundSyncControllerImpl> controller_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncControllerImplTest);
};

TEST_F(BackgroundSyncControllerImplTest, NoFieldTrial) {
  content::BackgroundSyncParameters original;
  content::BackgroundSyncParameters overrides;
  controller_->GetParameterOverrides(&overrides);
  EXPECT_EQ(original, overrides);
}

TEST_F(BackgroundSyncControllerImplTest, SomeParamsSet) {
  std::map<std::string, std::string> field_parameters;
  field_parameters[BackgroundSyncControllerImpl::kDisabledParameterName] =
      "TrUe";
  field_parameters[BackgroundSyncControllerImpl::kInitialRetryParameterName] =
      "100";
  ASSERT_TRUE(variations::AssociateVariationParams(
      BackgroundSyncControllerImpl::kFieldTrialName, kFieldTrialGroup,
      field_parameters));

  content::BackgroundSyncParameters original;
  content::BackgroundSyncParameters sync_parameters;
  controller_->GetParameterOverrides(&sync_parameters);
  EXPECT_TRUE(sync_parameters.disable);
  EXPECT_EQ(base::TimeDelta::FromSeconds(100),
            sync_parameters.initial_retry_delay);

  EXPECT_EQ(original.max_sync_attempts, sync_parameters.max_sync_attempts);
  EXPECT_EQ(original.retry_delay_factor, sync_parameters.retry_delay_factor);
  EXPECT_EQ(original.min_sync_recovery_time,
            sync_parameters.min_sync_recovery_time);
  EXPECT_EQ(original.max_sync_event_duration,
            sync_parameters.max_sync_event_duration);
}

TEST_F(BackgroundSyncControllerImplTest, AllParamsSet) {
  std::map<std::string, std::string> field_parameters;
  field_parameters[BackgroundSyncControllerImpl::kDisabledParameterName] =
      "FALSE";
  field_parameters[BackgroundSyncControllerImpl::kInitialRetryParameterName] =
      "100";
  field_parameters[BackgroundSyncControllerImpl::kMaxAttemptsParameterName] =
      "200";
  field_parameters[BackgroundSyncControllerImpl::
                       kMaxAttemptsWithNotificationPermissionParameterName] =
      "250";
  field_parameters
      [BackgroundSyncControllerImpl::kRetryDelayFactorParameterName] = "300";
  field_parameters[BackgroundSyncControllerImpl::kMinSyncRecoveryTimeName] =
      "400";
  field_parameters[BackgroundSyncControllerImpl::kMaxSyncEventDurationName] =
      "500";
  ASSERT_TRUE(variations::AssociateVariationParams(
      BackgroundSyncControllerImpl::kFieldTrialName, kFieldTrialGroup,
      field_parameters));

  content::BackgroundSyncParameters sync_parameters;
  controller_->GetParameterOverrides(&sync_parameters);

  EXPECT_FALSE(sync_parameters.disable);
  EXPECT_EQ(base::TimeDelta::FromSeconds(100),
            sync_parameters.initial_retry_delay);
  EXPECT_EQ(200, sync_parameters.max_sync_attempts);
  EXPECT_EQ(250,
            sync_parameters.max_sync_attempts_with_notification_permission);
  EXPECT_EQ(300, sync_parameters.retry_delay_factor);
  EXPECT_EQ(base::TimeDelta::FromSeconds(400),
            sync_parameters.min_sync_recovery_time);
  EXPECT_EQ(base::TimeDelta::FromSeconds(500),
            sync_parameters.max_sync_event_duration);
}

TEST_F(BackgroundSyncControllerImplTest, GetNextEventDelay) {
  controller_ = std::make_unique<BackgroundSyncControllerImpl>(
      profile_.GetOffTheRecordProfile());
  content::BackgroundSyncParameters sync_parameters;
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  SiteEngagementScore::SetParamValuesForTesting();
  SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(
      GURL(kExampleUrl), SiteEngagementScore::GetHighEngagementBoundary());

  // Periodic Sync: zero attempts.
  // min_interval < kMinGapBetweenPeriodicSyncEvents.
  base::TimeDelta delay = controller_->GetNextEventDelay(
      origin,
      /* min_interval= */ kSmallerThanMinGap.InMilliseconds(),
      /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC,
      &sync_parameters);
  EXPECT_EQ(delay, kMinGapBetweenPeriodicSyncEvents);

  // Periodic Sync: zero attempts.
  // |min_interval| > kMinGapBetweenPeriodicSyncEvents.
  delay = controller_->GetNextEventDelay(
      origin,
      /* min_interval= */ kLargerThanMinGap.InMilliseconds(),
      /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC,
      &sync_parameters);
  EXPECT_EQ(delay, kLargerThanMinGap);

  // One-shot Sync.
  delay = controller_->GetNextEventDelay(
      origin,
      /* min_interval= */ -1,
      /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::ONE_SHOT,
      &sync_parameters);
  EXPECT_EQ(delay, base::TimeDelta());

  base::TimeDelta delay_after_attempt1 = controller_->GetNextEventDelay(
      origin,
      /* min_interval= */ -1,
      /* num_attempts= */ 1, blink::mojom::BackgroundSyncType::ONE_SHOT,
      &sync_parameters);
  EXPECT_EQ(delay_after_attempt1, sync_parameters.initial_retry_delay);

  base::TimeDelta delay_after_attempt2 = controller_->GetNextEventDelay(
      origin,
      /* min_interval= */ -1,
      /* num_attempts= */ 2, blink::mojom::BackgroundSyncType::ONE_SHOT,
      &sync_parameters);
  EXPECT_LT(delay_after_attempt1, delay_after_attempt2);
}

TEST_F(BackgroundSyncControllerImplTest,
       GetNextEventDelayWithSiteEngagementPenalty) {
  controller_ = std::make_unique<BackgroundSyncControllerImpl>(
      profile_.GetOffTheRecordProfile());
  content::BackgroundSyncParameters sync_parameters;
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  SiteEngagementScore::SetParamValuesForTesting();
  SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(
      GURL(kExampleUrl), SiteEngagementScore::GetMediumEngagementBoundary());

  // Medium engagement.
  base::TimeDelta delay = controller_->GetNextEventDelay(
      origin,
      /* min_interval= */ kMinGapBetweenPeriodicSyncEvents.InMilliseconds(),
      /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC,
      &sync_parameters);
  EXPECT_EQ(delay,
            base::TimeDelta::FromMilliseconds(
                kMinGapBetweenPeriodicSyncEvents.InMilliseconds() *
                BackgroundSyncControllerImpl::kEngagementLevelMediumPenalty));

  // Low engagement.
  SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(
      GURL(kExampleUrl),
      SiteEngagementScore::GetMediumEngagementBoundary() - 1);
  delay = controller_->GetNextEventDelay(
      origin,
      /* min_interval= */ kMinGapBetweenPeriodicSyncEvents.InMilliseconds(),
      /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC,
      &sync_parameters);
  EXPECT_EQ(delay,
            base::TimeDelta::FromMilliseconds(
                kMinGapBetweenPeriodicSyncEvents.InMilliseconds() *
                BackgroundSyncControllerImpl::kEngagementLevelLowPenalty));

  // Minimal engagement.
  SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(GURL(kExampleUrl),
                                                              0.5);
  delay = controller_->GetNextEventDelay(
      origin,
      /* min_interval= */ kMinGapBetweenPeriodicSyncEvents.InMilliseconds(),
      /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC,
      &sync_parameters);
  EXPECT_EQ(delay,
            base::TimeDelta::FromMilliseconds(
                kMinGapBetweenPeriodicSyncEvents.InMilliseconds() *
                BackgroundSyncControllerImpl::kEngagementLevelMinimalPenalty));

  // No engagement.
  SiteEngagementService::Get(&profile_)->ResetBaseScoreForURL(GURL(kExampleUrl),
                                                              0);
  delay = controller_->GetNextEventDelay(
      origin,
      /* min_interval= */ kMinGapBetweenPeriodicSyncEvents.InMilliseconds(),
      /* num_attempts= */ 0, blink::mojom::BackgroundSyncType::PERIODIC,
      &sync_parameters);
  EXPECT_EQ(delay, base::TimeDelta::Max());
}

}  // namespace
