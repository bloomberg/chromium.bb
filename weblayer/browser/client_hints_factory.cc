// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/client_hints_factory.h"

#include "components/client_hints/browser/client_hints.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/content_browser_client_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

class PrefService;

namespace weblayer {

// static
client_hints::ClientHints* ClientHintsFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<client_hints::ClientHints*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
ClientHintsFactory* ClientHintsFactory::GetInstance() {
  static base::NoDestructor<ClientHintsFactory> factory;
  return factory.get();
}

ClientHintsFactory::ClientHintsFactory()
    : BrowserContextKeyedServiceFactory(
          "ClientHints",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

ClientHintsFactory::~ClientHintsFactory() = default;

KeyedService* ClientHintsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  PrefService* local_state = BrowserProcess::GetInstance()->GetLocalState();
  return new client_hints::ClientHints(
      context, BrowserProcess::GetInstance()->GetNetworkQualityTracker(),
      HostContentSettingsMapFactory::GetForBrowserContext(context),
      GetUserAgentMetadata(), local_state);
}

content::BrowserContext* ClientHintsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
