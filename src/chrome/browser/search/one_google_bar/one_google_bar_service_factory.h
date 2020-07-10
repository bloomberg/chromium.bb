// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class OneGoogleBarService;
class Profile;

class OneGoogleBarServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the OneGoogleBarService for |profile|.
  static OneGoogleBarService* GetForProfile(Profile* profile);

  static OneGoogleBarServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<OneGoogleBarServiceFactory>;

  OneGoogleBarServiceFactory();
  ~OneGoogleBarServiceFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(OneGoogleBarServiceFactory);
};

#endif  // CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_FACTORY_H_
