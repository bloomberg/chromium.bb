// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_DISPLAY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_DISPLAY_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace payments {

class PaymentRequestDisplayManager;

class PaymentRequestDisplayManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static PaymentRequestDisplayManagerFactory* GetInstance();
  static PaymentRequestDisplayManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  PaymentRequestDisplayManagerFactory();
  ~PaymentRequestDisplayManagerFactory() override;
  friend struct base::DefaultSingletonTraits<
      PaymentRequestDisplayManagerFactory>;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestDisplayManagerFactory);
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_DISPLAY_MANAGER_FACTORY_H_
