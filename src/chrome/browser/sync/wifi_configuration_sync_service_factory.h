// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_WIFI_CONFIGURATION_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_WIFI_CONFIGURATION_SYNC_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace chromeos {
namespace sync_wifi {
class WifiConfigurationSyncService;
}  // namespace sync_wifi
}  // namespace chromeos

class WifiConfigurationSyncServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static chromeos::sync_wifi::WifiConfigurationSyncService* GetForProfile(
      Profile* profile,
      bool create);
  static WifiConfigurationSyncServiceFactory* GetInstance();

  WifiConfigurationSyncServiceFactory(
      const WifiConfigurationSyncServiceFactory&) = delete;
  WifiConfigurationSyncServiceFactory& operator=(
      const WifiConfigurationSyncServiceFactory&) = delete;

  static bool ShouldRunInProfile(const Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<
      WifiConfigurationSyncServiceFactory>;

  WifiConfigurationSyncServiceFactory();
  ~WifiConfigurationSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // CHROME_BROWSER_SYNC_WIFI_CONFIGURATION_SYNC_SERVICE_FACTORY_H_
