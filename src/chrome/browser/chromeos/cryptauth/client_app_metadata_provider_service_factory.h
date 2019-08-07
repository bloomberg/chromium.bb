// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CRYPTAUTH_CLIENT_APP_METADATA_PROVIDER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_CRYPTAUTH_CLIENT_APP_METADATA_PROVIDER_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {

class ClientAppMetadataProviderService;

// Factory which creates one ClientAppMetadataProviderService per profile.
class ClientAppMetadataProviderServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ClientAppMetadataProviderService* GetForProfile(Profile* profile);
  static ClientAppMetadataProviderServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      ClientAppMetadataProviderServiceFactory>;

  ClientAppMetadataProviderServiceFactory();
  ~ClientAppMetadataProviderServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;

  DISALLOW_COPY_AND_ASSIGN(ClientAppMetadataProviderServiceFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CRYPTAUTH_CLIENT_APP_METADATA_PROVIDER_SERVICE_FACTORY_H_
