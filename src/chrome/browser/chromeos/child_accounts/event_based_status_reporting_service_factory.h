// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace chromeos {
class EventBasedStatusReportingService;

// Singleton that owns all EventBasedStatusReportingService and associates
// them with BrowserContexts. Listens for the BrowserContext's destruction
// notification and cleans up the associated EventBasedStatusReportingService.
class EventBasedStatusReportingServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static EventBasedStatusReportingService* GetForBrowserContext(
      content::BrowserContext* context);

  static EventBasedStatusReportingServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<EventBasedStatusReportingServiceFactory>;

  EventBasedStatusReportingServiceFactory();
  ~EventBasedStatusReportingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(EventBasedStatusReportingServiceFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_FACTORY_H_
