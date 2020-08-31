// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_EXTENSION_SHARE_EXTENSION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SHARE_EXTENSION_SHARE_EXTENSION_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class ShareExtensionService;

// Singleton that creates the ShareExtensionService and associates that service
// with ChromeBrowserState.
class ShareExtensionServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static ShareExtensionService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static ShareExtensionService* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);
  static ShareExtensionServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ShareExtensionServiceFactory>;

  ShareExtensionServiceFactory();
  ~ShareExtensionServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ShareExtensionServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_SHARE_EXTENSION_SHARE_EXTENSION_SERVICE_FACTORY_H_
