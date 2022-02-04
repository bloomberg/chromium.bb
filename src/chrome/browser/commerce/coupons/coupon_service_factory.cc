// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/coupons/coupon_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/commerce/coupons/coupon_db.h"
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/storage_partition.h"

// static
CouponServiceFactory* CouponServiceFactory::GetInstance() {
  static base::NoDestructor<CouponServiceFactory> factory;
  return factory.get();
}

// static
CouponService* CouponServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<CouponService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

CouponServiceFactory::CouponServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "CouponService",
          BrowserContextDependencyManager::GetInstance()) {}

CouponServiceFactory::~CouponServiceFactory() = default;

KeyedService* CouponServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());

  return new CouponService(std::make_unique<CouponDB>(context));
}
