// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_FACTORY_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
class ThemeService;

namespace extensions {
class Extension;
}

// Singleton that owns all ThemeServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated ThemeService.
class ThemeServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the ThemeService that provides theming resources for
  // |profile|. Note that even if a Profile doesn't have a theme installed, it
  // still needs a ThemeService to hand back the default theme images.
  static ThemeService* GetForProfile(Profile* profile);

  // Returns the Extension that implements the theme associated with
  // |profile|. Returns NULL if the theme is no longer installed, if there is
  // no installed theme, or the theme was cleared.
  static const extensions::Extension* GetThemeForProfile(Profile* profile);

  static ThemeServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ThemeServiceFactory>;

  ThemeServiceFactory();
  ~ThemeServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(ThemeServiceFactory);
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_FACTORY_H_
