// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace autofill {

class AutofillImageFetcher;

class AutofillImageFetcherFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the AutofillImageFetcher for |profile|, creating it if it is not
  // yet created.
  static AutofillImageFetcher* GetForProfile(Profile* profile);

  static AutofillImageFetcherFactory* GetInstance();

  static KeyedService* BuildAutofillImageFetcher(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<AutofillImageFetcherFactory>;

  AutofillImageFetcherFactory();
  ~AutofillImageFetcherFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_IMAGE_FETCHER_FACTORY_H_
