// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace web_app {

// static
WebAppProvider* WebAppProviderFactory::GetForProfile(Profile* profile) {
  return static_cast<WebAppProvider*>(
      WebAppProviderFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
WebAppProviderFactory* WebAppProviderFactory::GetInstance() {
  return base::Singleton<WebAppProviderFactory>::get();
}

WebAppProviderFactory::WebAppProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "WebAppProvider",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

WebAppProviderFactory::~WebAppProviderFactory() = default;

KeyedService* WebAppProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new WebAppProvider(profile);
}

bool WebAppProviderFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  //  namespace web_app
