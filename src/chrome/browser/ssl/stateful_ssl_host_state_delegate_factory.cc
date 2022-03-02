// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"

#include <memory>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"

namespace {

std::unique_ptr<KeyedService> BuildStatefulSSLHostStateDelegate(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<StatefulSSLHostStateDelegate>(
      profile, profile->GetPrefs(),
      HostContentSettingsMapFactory::GetForProfile(profile));
}

}  // namespace

// static
StatefulSSLHostStateDelegate*
StatefulSSLHostStateDelegateFactory::GetForProfile(Profile* profile) {
  return static_cast<StatefulSSLHostStateDelegate*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
StatefulSSLHostStateDelegateFactory*
StatefulSSLHostStateDelegateFactory::GetInstance() {
  return base::Singleton<StatefulSSLHostStateDelegateFactory>::get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
StatefulSSLHostStateDelegateFactory::GetDefaultFactoryForTesting() {
  return base::BindRepeating(&BuildStatefulSSLHostStateDelegate);
}

StatefulSSLHostStateDelegateFactory::StatefulSSLHostStateDelegateFactory()
    : BrowserContextKeyedServiceFactory(
          "StatefulSSLHostStateDelegate",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

StatefulSSLHostStateDelegateFactory::~StatefulSSLHostStateDelegateFactory() =
    default;

KeyedService* StatefulSSLHostStateDelegateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildStatefulSSLHostStateDelegate(context).release();
}

content::BrowserContext*
StatefulSSLHostStateDelegateFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool StatefulSSLHostStateDelegateFactory::ServiceIsNULLWhileTesting() const {
  // PageInfoBubbleViewTest tests require a StatefulSSLHostDelegateFactory.
  // If this returns false, DependencyManager::CreateContextServices for
  // PageInfoBubbleViewTest tests after the first one will set an
  // EmptyTestFactory, and cause the test to check fail.
  return false;
}
