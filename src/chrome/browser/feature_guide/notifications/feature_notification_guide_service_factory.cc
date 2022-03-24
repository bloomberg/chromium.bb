// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service_factory.h"

#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/feature_guide/notifications/config.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/feature_guide/notifications/feature_type.h"
#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_service_impl.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_ANDROID)
#include "chrome/browser/feature_guide/notifications/android/feature_notification_guide_bridge.h"
#endif

namespace feature_guide {
namespace {

// Default notification time interval in days.
constexpr int kDefaultNotificationTimeIntervalDays = 7;

std::vector<FeatureType> GetEnabledFeaturesFromVariations() {
  std::vector<FeatureType> enabled_features;
  // TODO(shaktisahu): Find these from finch.
  enabled_features.emplace_back(FeatureType::kIncognitoTab);
  enabled_features.emplace_back(FeatureType::kVoiceSearch);
  return enabled_features;
}

base::TimeDelta GetNotificationStartTimeDeltaFromVariations() {
  int notification_interval_days = base::GetFieldTrialParamByFeatureAsInt(
      features::kFeatureNotificationGuide, "notification_interval_days",
      kDefaultNotificationTimeIntervalDays);
  return base::Days(notification_interval_days);
}

}  // namespace

// static
FeatureNotificationGuideServiceFactory*
FeatureNotificationGuideServiceFactory::GetInstance() {
  return base::Singleton<FeatureNotificationGuideServiceFactory>::get();
}

// static
FeatureNotificationGuideService*
FeatureNotificationGuideServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<FeatureNotificationGuideService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

FeatureNotificationGuideServiceFactory::FeatureNotificationGuideServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FeatureNotificationGuideService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NotificationScheduleServiceFactory::GetInstance());
}

KeyedService* FeatureNotificationGuideServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* notification_scheduler =
      NotificationScheduleServiceFactory::GetForKey(profile->GetProfileKey());
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  Config config;
  config.enabled_features = GetEnabledFeaturesFromVariations();
  config.notification_deliver_time_delta =
      GetNotificationStartTimeDeltaFromVariations();
  std::unique_ptr<FeatureNotificationGuideService::Delegate> delegate;
#if defined(OS_ANDROID)
  delegate.reset(new FeatureNotificationGuideBridge());
#endif
  return new FeatureNotificationGuideServiceImpl(
      std::move(delegate), config, notification_scheduler, tracker,
      base::DefaultClock::GetInstance());
}

bool FeatureNotificationGuideServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace feature_guide
