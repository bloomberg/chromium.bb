// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_METRICS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_METRICS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {
class FamilyUserMetricsService;

// Singleton that owns FamilyUserMetricsService object and associates
// them with corresponding BrowserContexts. Listens for the BrowserContext's
// destruction notification and cleans up the associated
// FamilyUserMetricsService.
class FamilyUserMetricsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static FamilyUserMetricsService* GetForBrowserContext(
      content::BrowserContext* context);

  static FamilyUserMetricsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<FamilyUserMetricsServiceFactory>;

  FamilyUserMetricsServiceFactory();
  FamilyUserMetricsServiceFactory(const FamilyUserMetricsServiceFactory&) =
      delete;
  FamilyUserMetricsServiceFactory& operator=(
      const FamilyUserMetricsServiceFactory&) = delete;

  ~FamilyUserMetricsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromOS code migration is done.
namespace chromeos {
using ::ash::FamilyUserMetricsServiceFactory;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_METRICS_SERVICE_FACTORY_H_
