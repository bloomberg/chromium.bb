// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_SCREEN_TIME_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_SCREEN_TIME_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace chromeos {
class ScreenTimeController;

// Singleton that owns all ScreenTimeController and associates them with
// BrowserContexts. Listens for the BrowserContext's destruction notification
// and cleans up the associated ScreenTimeController.
class ScreenTimeControllerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ScreenTimeController* GetForBrowserContext(
      content::BrowserContext* context);

  static ScreenTimeControllerFactory* GetInstance();

 private:
  friend class base::NoDestructor<ScreenTimeControllerFactory>;

  ScreenTimeControllerFactory();
  ~ScreenTimeControllerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ScreenTimeControllerFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_SCREEN_TIME_CONTROLLER_FACTORY_H_
