// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {

class PrintJobHistoryService;

// Singleton that owns all PrintJobHistoryServices and associates them with
// Profiles.
class PrintJobHistoryServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the PrintJobHistoryService for |context|, creating it if it is not
  // yet created.
  static PrintJobHistoryService* GetForBrowserContext(
      content::BrowserContext* context);

  static PrintJobHistoryServiceFactory* GetInstance();

  PrintJobHistoryServiceFactory(const PrintJobHistoryServiceFactory&) = delete;
  PrintJobHistoryServiceFactory& operator=(
      const PrintJobHistoryServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<PrintJobHistoryServiceFactory>;

  PrintJobHistoryServiceFactory();
  ~PrintJobHistoryServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
using ::ash::PrintJobHistoryServiceFactory;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_FACTORY_H_
