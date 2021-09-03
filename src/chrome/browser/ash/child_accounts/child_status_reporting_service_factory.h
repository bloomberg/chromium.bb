// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_STATUS_REPORTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_STATUS_REPORTING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {
class ChildStatusReportingService;

// Singleton that owns all ChildStatusReportingService objects and associates
// them with corresponding BrowserContexts. Listens for the BrowserContext's
// destruction notification and cleans up the associated
// ChildStatusReportingService.
class ChildStatusReportingServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ChildStatusReportingService* GetForBrowserContext(
      content::BrowserContext* context);

  static ChildStatusReportingServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ChildStatusReportingServiceFactory>;

  ChildStatusReportingServiceFactory();
  ChildStatusReportingServiceFactory(
      const ChildStatusReportingServiceFactory&) = delete;
  ChildStatusReportingServiceFactory& operator=(
      const ChildStatusReportingServiceFactory&) = delete;

  ~ChildStatusReportingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromOS code migration is done.
namespace chromeos {
using ::ash::ChildStatusReportingServiceFactory;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_STATUS_REPORTING_SERVICE_FACTORY_H_
