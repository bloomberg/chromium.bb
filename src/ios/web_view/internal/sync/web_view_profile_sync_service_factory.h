// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_PROFILE_SYNC_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_PROFILE_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace browser_sync {
class ProfileSyncService;
}  // namespace browser_sync

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all ProfileSyncService and associates them with
// WebViewBrowserState.
class WebViewProfileSyncServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static browser_sync::ProfileSyncService* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewProfileSyncServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<WebViewProfileSyncServiceFactory>;

  WebViewProfileSyncServiceFactory();
  ~WebViewProfileSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebViewProfileSyncServiceFactory);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_PROFILE_SYNC_SERVICE_FACTORY_H_
