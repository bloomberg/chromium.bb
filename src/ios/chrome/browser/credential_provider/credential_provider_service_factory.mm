// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/credential_provider/credential_provider_service_factory.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/credential_provider/credential_provider_service.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
CredentialProviderService* CredentialProviderServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<CredentialProviderService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
CredentialProviderServiceFactory*
CredentialProviderServiceFactory::GetInstance() {
  static base::NoDestructor<CredentialProviderServiceFactory> instance;
  return instance.get();
}

CredentialProviderServiceFactory::CredentialProviderServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "CredentialProviderService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromePasswordStoreFactory::GetInstance());
}

CredentialProviderServiceFactory::~CredentialProviderServiceFactory() = default;

std::unique_ptr<KeyedService>
CredentialProviderServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  scoped_refptr<password_manager::PasswordStore> password_store =
      IOSChromePasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS);
  ArchivableCredentialStore* credential_store =
      [[ArchivableCredentialStore alloc]
          initWithFileURL:CredentialProviderSharedArchivableStoreURL()];
  return std::make_unique<CredentialProviderService>(password_store,
                                                     credential_store);
}
