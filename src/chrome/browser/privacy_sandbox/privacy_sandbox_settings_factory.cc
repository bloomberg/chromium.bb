// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"

PrivacySandboxSettingsFactory* PrivacySandboxSettingsFactory::GetInstance() {
  return base::Singleton<PrivacySandboxSettingsFactory>::get();
}

PrivacySandboxSettings* PrivacySandboxSettingsFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PrivacySandboxSettings*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacySandboxSettingsFactory::PrivacySandboxSettingsFactory()
    : BrowserContextKeyedServiceFactory(
          "PrivacySandboxSettings",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* PrivacySandboxSettingsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return new PrivacySandboxSettings(
      HostContentSettingsMapFactory::GetForProfile(profile),
      CookieSettingsFactory::GetForProfile(profile).get(), profile->GetPrefs());
}

content::BrowserContext* PrivacySandboxSettingsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
