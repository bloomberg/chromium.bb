// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router_factory.h"

#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
AutofillPrivateEventRouter*
AutofillPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<AutofillPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AutofillPrivateEventRouterFactory*
AutofillPrivateEventRouterFactory::GetInstance() {
  return base::Singleton<AutofillPrivateEventRouterFactory>::get();
}

AutofillPrivateEventRouterFactory::AutofillPrivateEventRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "AutofillPrivateEventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

AutofillPrivateEventRouterFactory::
    ~AutofillPrivateEventRouterFactory() {
}

KeyedService* AutofillPrivateEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return AutofillPrivateEventRouter::Create(context);
}

content::BrowserContext*
AutofillPrivateEventRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
}

bool AutofillPrivateEventRouterFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
