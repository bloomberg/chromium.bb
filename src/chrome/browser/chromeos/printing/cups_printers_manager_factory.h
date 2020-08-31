// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINTERS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINTERS_MANAGER_FACTORY_H_

#include <memory>
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace chromeos {

class CupsPrintersManager;
class CupsPrintersManagerProxy;

class CupsPrintersManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CupsPrintersManagerFactory* GetInstance();
  static CupsPrintersManager* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns the CupsPrintersManagerProxy object which is always attached to the
  // primary profile.
  CupsPrintersManagerProxy* GetProxy();

 private:
  friend struct base::DefaultSingletonTraits<CupsPrintersManagerFactory>;

  CupsPrintersManagerFactory();
  ~CupsPrintersManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void BrowserContextShutdown(content::BrowserContext* context) override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  // Proxy object always attached to the primary profile.
  std::unique_ptr<CupsPrintersManagerProxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintersManagerFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINTERS_MANAGER_FACTORY_H_
