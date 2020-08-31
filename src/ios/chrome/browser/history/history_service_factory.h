// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_HISTORY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_HISTORY_HISTORY_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
enum class ServiceAccessType;

namespace history {
class HistoryService;
}

namespace ios {
// Singleton that owns all HistoryServices and associates them with
// ChromeBrowserState.
class HistoryServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static history::HistoryService* GetForBrowserState(
      ChromeBrowserState* browser_state,
      ServiceAccessType access_type);
  static history::HistoryService* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state,
      ServiceAccessType access_type);
  static HistoryServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<HistoryServiceFactory>;

  HistoryServiceFactory();
  ~HistoryServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(HistoryServiceFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_HISTORY_HISTORY_SERVICE_FACTORY_H_
