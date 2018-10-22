// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_TYPE_STORE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_TYPE_STORE_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace syncer {
class ModelTypeStoreService;
}  // namespace syncer

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// Singleton that owns all ModelTypeStoreService and associates them with
// ios::ChromeBrowserState.
class ModelTypeStoreServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static syncer::ModelTypeStoreService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static ModelTypeStoreServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ModelTypeStoreServiceFactory>;

  ModelTypeStoreServiceFactory();
  ~ModelTypeStoreServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_TYPE_STORE_SERVICE_FACTORY_H_
