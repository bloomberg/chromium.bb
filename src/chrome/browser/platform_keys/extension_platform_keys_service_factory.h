// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_KEYS_EXTENSION_PLATFORM_KEYS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PLATFORM_KEYS_EXTENSION_PLATFORM_KEYS_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace chromeos {

class ExtensionPlatformKeysService;

// Factory to create ExtensionPlatformKeysService.
class ExtensionPlatformKeysServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionPlatformKeysService* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionPlatformKeysServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      ExtensionPlatformKeysServiceFactory>;

  ExtensionPlatformKeysServiceFactory();
  ExtensionPlatformKeysServiceFactory(
      const ExtensionPlatformKeysServiceFactory&) = delete;
  auto operator=(const ExtensionPlatformKeysServiceFactory&) = delete;
  ~ExtensionPlatformKeysServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_PLATFORM_KEYS_EXTENSION_PLATFORM_KEYS_SERVICE_FACTORY_H_
