// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

// static
ClientSideDetectionService* ClientSideDetectionServiceFactory::GetForProfile(
    Profile* profile) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableClientSidePhishingDetection)) {
    return nullptr;
  }

  return static_cast<ClientSideDetectionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */
                                                 true));
}

// static
ClientSideDetectionServiceFactory*
ClientSideDetectionServiceFactory::GetInstance() {
  return base::Singleton<ClientSideDetectionServiceFactory>::get();
}

ClientSideDetectionServiceFactory::ClientSideDetectionServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ClientSideDetectionService",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* ClientSideDetectionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new ClientSideDetectionService(profile);
}

content::BrowserContext*
ClientSideDetectionServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace safe_browsing
