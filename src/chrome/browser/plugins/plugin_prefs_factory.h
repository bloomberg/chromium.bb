// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_PREFS_FACTORY_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_PREFS_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

class PluginPrefs;
class Profile;

class PluginPrefsFactory : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  static scoped_refptr<PluginPrefs> GetPrefsForProfile(Profile* profile);

  static PluginPrefsFactory* GetInstance();

 private:
  friend class PluginPrefs;
  friend struct base::DefaultSingletonTraits<PluginPrefsFactory>;

  // Helper method for PluginPrefs::GetForTestingProfile.
  static scoped_refptr<RefcountedKeyedService> CreateForTestingProfile(
      content::BrowserContext* profile);

  PluginPrefsFactory();
  ~PluginPrefsFactory() override;

  // RefcountedBrowserContextKeyedServiceFactory methods:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  // BrowserContextKeyedServiceFactory methods:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_PREFS_FACTORY_H_
