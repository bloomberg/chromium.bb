// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"

#include <string>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"

namespace send_tab_to_self {
// static
SendTabToSelfClientService* SendTabToSelfClientServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<send_tab_to_self::SendTabToSelfClientService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SendTabToSelfClientServiceFactory*
SendTabToSelfClientServiceFactory::GetInstance() {
  return base::Singleton<SendTabToSelfClientServiceFactory>::get();
}

SendTabToSelfClientServiceFactory::SendTabToSelfClientServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SendTabToSelfClientService",
          BrowserContextDependencyManager::GetInstance()) {
  // TODO(tgupta): Add that this depends on the DisplayNotificationService as
  // well
  DependsOn(SendTabToSelfSyncServiceFactory::GetInstance());
}

SendTabToSelfClientServiceFactory::~SendTabToSelfClientServiceFactory() {}

// BrowserStateKeyedServiceFactory implementation.
KeyedService* SendTabToSelfClientServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  SendTabToSelfSyncService* sync_service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  return new SendTabToSelfClientService(profile,
                                        sync_service->GetSendTabToSelfModel());
}

bool SendTabToSelfClientServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool SendTabToSelfClientServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace send_tab_to_self
