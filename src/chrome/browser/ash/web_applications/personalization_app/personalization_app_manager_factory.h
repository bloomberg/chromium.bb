// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace ash {
namespace personalization_app {

class PersonalizationAppManager;

class PersonalizationAppManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static PersonalizationAppManager* GetForBrowserContext(
      content::BrowserContext* context);
  static PersonalizationAppManagerFactory* GetInstance();

  PersonalizationAppManagerFactory(const PersonalizationAppManagerFactory&) =
      delete;
  PersonalizationAppManagerFactory& operator=(
      const PersonalizationAppManagerFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<PersonalizationAppManagerFactory>;

  PersonalizationAppManagerFactory();
  ~PersonalizationAppManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace personalization_app
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_MANAGER_FACTORY_H_
