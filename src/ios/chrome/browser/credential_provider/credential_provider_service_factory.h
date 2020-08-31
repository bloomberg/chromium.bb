// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class CredentialProviderService;

// Singleton that owns all CredentialProviderServices and associates them with
// ChromeBrowserState.
class CredentialProviderServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static CredentialProviderService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static CredentialProviderServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<CredentialProviderServiceFactory>;

  CredentialProviderServiceFactory();
  ~CredentialProviderServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CredentialProviderServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_FACTORY_H_
