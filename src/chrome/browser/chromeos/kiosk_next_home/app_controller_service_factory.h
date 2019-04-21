// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_APP_CONTROLLER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_APP_CONTROLLER_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace chromeos {
namespace kiosk_next_home {

class AppControllerService;

// Singleton that owns and provides AppControllerService instances.
class AppControllerServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppControllerService* GetForBrowserContext(
      content::BrowserContext* context);

  static AppControllerServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<AppControllerServiceFactory>;

  AppControllerServiceFactory();
  ~AppControllerServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(AppControllerServiceFactory);
};

}  // namespace kiosk_next_home
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_APP_CONTROLLER_SERVICE_FACTORY_H_
