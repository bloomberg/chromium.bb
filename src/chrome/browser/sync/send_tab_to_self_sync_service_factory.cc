// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/model/model_type_store_service.h"

// static
send_tab_to_self::SendTabToSelfSyncService*
SendTabToSelfSyncServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<send_tab_to_self::SendTabToSelfSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SendTabToSelfSyncServiceFactory*
SendTabToSelfSyncServiceFactory::GetInstance() {
  return base::Singleton<SendTabToSelfSyncServiceFactory>::get();
}

SendTabToSelfSyncServiceFactory::SendTabToSelfSyncServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SendTabToSelfSyncService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
}

SendTabToSelfSyncServiceFactory::~SendTabToSelfSyncServiceFactory() {}

KeyedService* SendTabToSelfSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(send_tab_to_self::IsReceivingEnabled());

  Profile* profile = Profile::FromBrowserContext(context);

  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  return new send_tab_to_self::SendTabToSelfSyncService(
      chrome::GetChannel(), std::move(store_factory), history_service);
}
