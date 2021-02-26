// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCANNING_SCAN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_SCANNING_SCAN_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace chromeos {

class ScanService;

// Factory for ScanService.
class ScanServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ScanService* GetForBrowserContext(content::BrowserContext* context);
  static ScanServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ScanServiceFactory>;

  ScanServiceFactory();
  ~ScanServiceFactory() override;

  ScanServiceFactory(const ScanServiceFactory&) = delete;
  ScanServiceFactory& operator=(const ScanServiceFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SCANNING_SCAN_SERVICE_FACTORY_H_
