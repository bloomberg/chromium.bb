// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy/privacy_metrics_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy/privacy_metrics_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"

PrivacyMetricsServiceFactory* PrivacyMetricsServiceFactory::GetInstance() {
  return base::Singleton<PrivacyMetricsServiceFactory>::get();
}

PrivacyMetricsService* PrivacyMetricsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PrivacyMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacyMetricsServiceFactory::PrivacyMetricsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PrivacyMetricsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

KeyedService* PrivacyMetricsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // No metrics recorded for OTR profiles.
  if (context->IsOffTheRecord())
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);
  return new PrivacyMetricsService(
      profile->GetPrefs(),
      HostContentSettingsMapFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile));
}
