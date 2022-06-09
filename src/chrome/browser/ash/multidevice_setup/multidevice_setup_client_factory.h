// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_CLIENT_FACTORY_H_
#define CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_CLIENT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

class ChromeOSMetricsProviderTest;
class Profile;

namespace ash {
namespace multidevice_setup {

// Singleton that owns all MultiDeviceSetupClient instances and associates them
// with Profiles.
class MultiDeviceSetupClientFactory : public BrowserContextKeyedServiceFactory {
 public:
  static MultiDeviceSetupClient* GetForProfile(Profile* profile);

  static MultiDeviceSetupClientFactory* GetInstance();

  MultiDeviceSetupClientFactory(const MultiDeviceSetupClientFactory&) = delete;
  MultiDeviceSetupClientFactory& operator=(
      const MultiDeviceSetupClientFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<MultiDeviceSetupClientFactory>;
  friend class ::ChromeOSMetricsProviderTest;

  MultiDeviceSetupClientFactory();
  ~MultiDeviceSetupClientFactory() override;

  void SetServiceIsNULLWhileTestingForTesting(
      bool service_is_null_while_testing) {
    service_is_null_while_testing_ = service_is_null_while_testing;
  }

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool service_is_null_while_testing_ = true;
};

}  // namespace multidevice_setup
}  // namespace ash

namespace chromeos {
namespace multidevice_setup {
using ::ash::multidevice_setup::MultiDeviceSetupClientFactory;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_CLIENT_FACTORY_H_
