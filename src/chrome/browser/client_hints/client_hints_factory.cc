// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/client_hints/client_hints_factory.h"

#include "chrome/browser/client_hints/client_hints.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace {

base::LazyInstance<ClientHintsFactory>::DestructorAtExit
    g_client_hints_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
content::ClientHintsControllerDelegate*
ClientHintsFactory::GetForBrowserContext(content::BrowserContext* context) {
  return static_cast<client_hints::ClientHints*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ClientHintsFactory* ClientHintsFactory::GetInstance() {
  return g_client_hints_factory.Pointer();
}

ClientHintsFactory::ClientHintsFactory()
    : BrowserContextKeyedServiceFactory(
          "ClientHints",
          BrowserContextDependencyManager::GetInstance()) {}

ClientHintsFactory::~ClientHintsFactory() {}

KeyedService* ClientHintsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new client_hints::ClientHints(context);
}

content::BrowserContext* ClientHintsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool ClientHintsFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
