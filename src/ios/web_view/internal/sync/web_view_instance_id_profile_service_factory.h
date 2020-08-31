// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace instance_id {
class InstanceIDProfileService;
}

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns all InstanceIDProfileService and associates them with
// WebViewBrowserState.
class WebViewInstanceIDProfileServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static instance_id::InstanceIDProfileService* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewInstanceIDProfileServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebViewInstanceIDProfileServiceFactory>;

  WebViewInstanceIDProfileServiceFactory();
  ~WebViewInstanceIDProfileServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInstanceIDProfileServiceFactory);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
