// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_delegate_impl_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_ui_delegate_impl.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace web_app {

// static
WebAppUiDelegateImpl* WebAppUiDelegateImplFactory::GetForProfile(
    Profile* profile) {
  return static_cast<WebAppUiDelegateImpl*>(
      WebAppUiDelegateImplFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
WebAppUiDelegateImplFactory* WebAppUiDelegateImplFactory::GetInstance() {
  return base::Singleton<WebAppUiDelegateImplFactory>::get();
}

WebAppUiDelegateImplFactory::WebAppUiDelegateImplFactory()
    : BrowserContextKeyedServiceFactory(
          "WebAppUiDelegate",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(WebAppProviderFactory::GetInstance());
}

WebAppUiDelegateImplFactory::~WebAppUiDelegateImplFactory() = default;

KeyedService* WebAppUiDelegateImplFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new WebAppUiDelegateImpl(Profile::FromBrowserContext(context));
}

bool WebAppUiDelegateImplFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* WebAppUiDelegateImplFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextForWebApps(context);
}

}  // namespace web_app
