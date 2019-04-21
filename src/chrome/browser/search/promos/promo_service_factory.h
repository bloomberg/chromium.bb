// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_PROMOS_PROMO_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_PROMOS_PROMO_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class PromoService;
class Profile;

class PromoServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the PromoService for |profile|.
  static PromoService* GetForProfile(Profile* profile);

  static PromoServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PromoServiceFactory>;

  PromoServiceFactory();
  ~PromoServiceFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(PromoServiceFactory);
};

#endif  // CHROME_BROWSER_SEARCH_PROMOS_PROMO_SERVICE_FACTORY_H_
