// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"

#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

// static
MediaGalleriesPreferences*
MediaGalleriesPreferencesFactory::GetForProfile(Profile* profile) {
  return static_cast<MediaGalleriesPreferences*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MediaGalleriesPreferencesFactory*
MediaGalleriesPreferencesFactory::GetInstance() {
  return base::Singleton<MediaGalleriesPreferencesFactory>::get();
}

MediaGalleriesPreferencesFactory::MediaGalleriesPreferencesFactory()
    : BrowserContextKeyedServiceFactory(
        "MediaGalleriesPreferences",
        BrowserContextDependencyManager::GetInstance()) {}

MediaGalleriesPreferencesFactory::~MediaGalleriesPreferencesFactory() {}

KeyedService* MediaGalleriesPreferencesFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new MediaGalleriesPreferences(static_cast<Profile*>(profile));
}

void MediaGalleriesPreferencesFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  MediaGalleriesPreferences::RegisterProfilePrefs(prefs);
}

content::BrowserContext*
MediaGalleriesPreferencesFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
