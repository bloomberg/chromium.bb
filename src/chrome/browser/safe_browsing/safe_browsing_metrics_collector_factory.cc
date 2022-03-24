// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
SafeBrowsingMetricsCollector*
SafeBrowsingMetricsCollectorFactory::GetForProfile(Profile* profile) {
  return static_cast<SafeBrowsingMetricsCollector*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
SafeBrowsingMetricsCollectorFactory*
SafeBrowsingMetricsCollectorFactory::GetInstance() {
  return base::Singleton<SafeBrowsingMetricsCollectorFactory>::get();
}

SafeBrowsingMetricsCollectorFactory::SafeBrowsingMetricsCollectorFactory()
    : BrowserContextKeyedServiceFactory(
          "SafeBrowsingMetricsCollector",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* SafeBrowsingMetricsCollectorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new SafeBrowsingMetricsCollector(profile->GetPrefs());
}

}  // namespace safe_browsing
