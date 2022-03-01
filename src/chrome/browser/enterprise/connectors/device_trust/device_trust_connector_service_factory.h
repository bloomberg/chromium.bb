// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CONNECTOR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CONNECTOR_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace enterprise_connectors {

class DeviceTrustConnectorService;

// Singleton factory for Profile-keyed DeviceTrustConnectorService instances.
class DeviceTrustConnectorServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static DeviceTrustConnectorServiceFactory* GetInstance();
  static DeviceTrustConnectorService* GetForProfile(Profile* profile);

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend struct base::DefaultSingletonTraits<
      DeviceTrustConnectorServiceFactory>;

  DeviceTrustConnectorServiceFactory();
  ~DeviceTrustConnectorServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_CONNECTOR_SERVICE_FACTORY_H_
