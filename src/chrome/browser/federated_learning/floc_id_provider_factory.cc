// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_id_provider_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/federated_learning/floc_id_provider_impl.h"
#include "chrome/browser/federated_learning/floc_remote_permission_service.h"
#include "chrome/browser/federated_learning/floc_remote_permission_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_user_events/user_event_service.h"

namespace federated_learning {

// static
FlocIdProvider* FlocIdProviderFactory::GetForProfile(Profile* profile) {
  return static_cast<FlocIdProvider*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FlocIdProviderFactory* FlocIdProviderFactory::GetInstance() {
  return base::Singleton<FlocIdProviderFactory>::get();
}

FlocIdProviderFactory::FlocIdProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "FlocIdProvider",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(CookieSettingsFactory::GetInstance());
  DependsOn(FlocRemotePermissionServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(browser_sync::UserEventServiceFactory::GetInstance());
}

FlocIdProviderFactory::~FlocIdProviderFactory() = default;

KeyedService* FlocIdProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (!sync_service)
    return nullptr;

  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(profile);
  if (!cookie_settings)
    return nullptr;

  FlocRemotePermissionService* floc_remote_permission_service =
      FlocRemotePermissionServiceFactory::GetForProfile(profile);
  if (!floc_remote_permission_service)
    return nullptr;

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (!history_service)
    return nullptr;

  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile);
  if (!user_event_service)
    return nullptr;

  return new FlocIdProviderImpl(sync_service, std::move(cookie_settings),
                                floc_remote_permission_service, history_service,
                                user_event_service);
}

}  // namespace federated_learning
