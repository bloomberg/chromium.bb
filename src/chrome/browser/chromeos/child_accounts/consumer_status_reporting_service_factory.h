// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CONSUMER_STATUS_REPORTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CONSUMER_STATUS_REPORTING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace chromeos {
class ConsumerStatusReportingService;

// Singleton that owns all ConsumerStatusReportingService objects and associates
// them with corresponding BrowserContexts. Listens for the BrowserContext's
// destruction notification and cleans up the associated
// ConsumerStatusReportingService.
class ConsumerStatusReportingServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ConsumerStatusReportingService* GetForBrowserContext(
      content::BrowserContext* context);

  static ConsumerStatusReportingServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ConsumerStatusReportingServiceFactory>;

  ConsumerStatusReportingServiceFactory();
  ~ConsumerStatusReportingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ConsumerStatusReportingServiceFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CONSUMER_STATUS_REPORTING_SERVICE_FACTORY_H_
