// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class GAIAInfoUpdateService;
class Profile;

// Singleton that owns all GAIAInfoUpdateServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated GAIAInfoUpdateService.
class GAIAInfoUpdateServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of GAIAInfoUpdateService associated with this profile
  // (creating one if none exists). Returns NULL if this profile cannot have a
  // GAIAInfoUpdateService (for example, if |profile| is incognito).
  static GAIAInfoUpdateService* GetForProfile(Profile* profile);

  // Returns an instance of the GAIAInfoUpdateServiceFactory singleton.
  static GAIAInfoUpdateServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<GAIAInfoUpdateServiceFactory>;

  GAIAInfoUpdateServiceFactory();
  ~GAIAInfoUpdateServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(GAIAInfoUpdateServiceFactory);
};

#endif  // CHROME_BROWSER_PROFILES_GAIA_INFO_UPDATE_SERVICE_FACTORY_H_
