// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_LEGACY_STRIKE_DATABASE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_LEGACY_STRIKE_DATABASE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace autofill {
class LegacyStrikeDatabase;
}

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all LegacyStrikeDatabases and associates them with
// ios_web_view::WebViewBrowserState.
class WebViewLegacyStrikeDatabaseFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static autofill::LegacyStrikeDatabase* GetForBrowserState(
      WebViewBrowserState* browser_state);
  static WebViewLegacyStrikeDatabaseFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      WebViewLegacyStrikeDatabaseFactory>;

  WebViewLegacyStrikeDatabaseFactory();
  ~WebViewLegacyStrikeDatabaseFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebViewLegacyStrikeDatabaseFactory);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_LEGACY_STRIKE_DATABASE_FACTORY_H_
