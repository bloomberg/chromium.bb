// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/cookie_settings_factory.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"

using content_settings::CookieControlsMode;

// static
scoped_refptr<content_settings::CookieSettings>
CookieSettingsFactory::GetForProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return static_cast<content_settings::CookieSettings*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
CookieSettingsFactory* CookieSettingsFactory::GetInstance() {
  return base::Singleton<CookieSettingsFactory>::get();
}

CookieSettingsFactory::CookieSettingsFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "CookieSettings",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

CookieSettingsFactory::~CookieSettingsFactory() = default;

void CookieSettingsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  content_settings::CookieSettings::RegisterProfilePrefs(registry);
}

content::BrowserContext* CookieSettingsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // The incognito profile has its own content settings map. Therefore, it
  // should get its own CookieSettings.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

scoped_refptr<RefcountedKeyedService>
CookieSettingsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  PrefService* prefs = profile->GetPrefs();

  // Migrate BlockThirdPartyCookies to CookieControlsMode pref.
  if (prefs->IsUserModifiablePreference(prefs::kBlockThirdPartyCookies) &&
      prefs->GetBoolean(prefs::kBlockThirdPartyCookies) &&
      prefs->GetInteger(prefs::kCookieControlsMode) !=
          static_cast<int>(CookieControlsMode::kBlockThirdParty)) {
    prefs->SetInteger(prefs::kCookieControlsMode,
                      static_cast<int>(CookieControlsMode::kBlockThirdParty));
  }

  // Record cookie setting histograms.
  base::UmaHistogramBoolean("Privacy.ThirdPartyCookieBlockingSetting",
                            prefs->GetBoolean(prefs::kBlockThirdPartyCookies));
  base::UmaHistogramEnumeration(
      "Privacy.CookieControlsSetting",
      static_cast<CookieControlsMode>(
          prefs->GetInteger(prefs::kCookieControlsMode)));
  // The DNT setting is only vaguely cookie-related. However, there is currently
  // no DNT-related code that is executed once per Profile lifetime, and
  // creating a new BrowserContextKeyedService to record this metric would be
  // an overkill. Hence, we put it here.
  // TODO(msramek): Find a better place for this metric.
  base::UmaHistogramBoolean("Privacy.DoNotTrackSetting",
                            prefs->GetBoolean(prefs::kEnableDoNotTrack));
  return new content_settings::CookieSettings(
      HostContentSettingsMapFactory::GetForProfile(profile), prefs,
      profile->IsIncognitoProfile(), extensions::kExtensionScheme);
}
