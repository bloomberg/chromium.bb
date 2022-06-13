// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_offer_manager_factory.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#if !defined(OS_ANDROID)
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "chrome/browser/commerce/coupons/coupon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#endif

namespace autofill {

// static
AutofillOfferManager* AutofillOfferManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AutofillOfferManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AutofillOfferManagerFactory* AutofillOfferManagerFactory::GetInstance() {
  return base::Singleton<AutofillOfferManagerFactory>::get();
}

AutofillOfferManagerFactory::AutofillOfferManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "AutofillOfferManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(PersonalDataManagerFactory::GetInstance());
#if !defined(OS_ANDROID)
  DependsOn(CouponServiceFactory::GetInstance());
#endif
}

AutofillOfferManagerFactory::~AutofillOfferManagerFactory() = default;

KeyedService* AutofillOfferManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if !defined(OS_ANDROID)
  CouponService* service =
      CouponServiceFactory::GetForProfile(Profile::FromBrowserContext(context));
  return new AutofillOfferManager(
      PersonalDataManagerFactory::GetForBrowserContext(context),
      static_cast<CouponServiceDelegate*>(service));
#else
  return new AutofillOfferManager(
      PersonalDataManagerFactory::GetForBrowserContext(context),
      /*coupon_service_delegate=*/nullptr);
#endif
}

}  // namespace autofill
