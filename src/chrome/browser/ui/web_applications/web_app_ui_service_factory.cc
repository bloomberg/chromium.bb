// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_ui_service.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace web_app {

// static
WebAppUiService* WebAppUiServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<WebAppUiService*>(
      WebAppUiServiceFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
WebAppUiServiceFactory* WebAppUiServiceFactory::GetInstance() {
  return base::Singleton<WebAppUiServiceFactory>::get();
}

WebAppUiServiceFactory::WebAppUiServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "WebAppUiDelegate",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(WebAppProviderFactory::GetInstance());
}

WebAppUiServiceFactory::~WebAppUiServiceFactory() = default;

KeyedService* WebAppUiServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new WebAppUiService(Profile::FromBrowserContext(context));
}

bool WebAppUiServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* WebAppUiServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextForWebApps(context);
}

}  // namespace web_app
