// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_FACTORY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

class Profile;
class ProfileStatistics;

// Singleton that owns all ProfileStatistics and associates them with Profiles.
class ProfileStatisticsFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ProfileStatistics* GetForProfile(Profile* profile);

  static ProfileStatisticsFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ProfileStatisticsFactory>;

  ProfileStatisticsFactory();

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ProfileStatisticsFactory);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_STATISTICS_FACTORY_H_
