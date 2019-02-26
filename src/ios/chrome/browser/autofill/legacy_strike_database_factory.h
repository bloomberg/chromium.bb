// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_LEGACY_STRIKE_DATABASE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_LEGACY_STRIKE_DATABASE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ios {
class ChromeBrowserState;
}

namespace autofill {

class LegacyStrikeDatabase;

// Singleton that owns all LegacyStrikeDatabases and associates them with
// ios::ChromeBrowserState.
class LegacyStrikeDatabaseFactory : public BrowserStateKeyedServiceFactory {
 public:
  static LegacyStrikeDatabase* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static LegacyStrikeDatabaseFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<LegacyStrikeDatabaseFactory>;

  LegacyStrikeDatabaseFactory();
  ~LegacyStrikeDatabaseFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(LegacyStrikeDatabaseFactory);
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_LEGACY_STRIKE_DATABASE_FACTORY_H_
