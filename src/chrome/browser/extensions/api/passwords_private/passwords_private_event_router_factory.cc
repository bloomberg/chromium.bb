// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
PasswordsPrivateEventRouter*
PasswordsPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<PasswordsPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PasswordsPrivateEventRouterFactory*
PasswordsPrivateEventRouterFactory::GetInstance() {
  return base::Singleton<PasswordsPrivateEventRouterFactory>::get();
}

PasswordsPrivateEventRouterFactory::PasswordsPrivateEventRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordsPrivateEventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(PasswordsPrivateDelegateFactory::GetInstance());
}

PasswordsPrivateEventRouterFactory::
    ~PasswordsPrivateEventRouterFactory() {
}

KeyedService* PasswordsPrivateEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return PasswordsPrivateEventRouter::Create(context);
}

content::BrowserContext*
PasswordsPrivateEventRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

bool PasswordsPrivateEventRouterFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool PasswordsPrivateEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
