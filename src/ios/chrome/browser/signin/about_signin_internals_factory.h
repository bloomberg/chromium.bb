// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_ABOUT_SIGNIN_INTERNALS_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_ABOUT_SIGNIN_INTERNALS_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

class AboutSigninInternals;

namespace ios {

class ChromeBrowserState;

// Singleton that owns all AboutSigninInternals and associates them with browser
// states.
class AboutSigninInternalsFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of AboutSigninInternals associated with this browser
  // state, creating one if none exists.
  static AboutSigninInternals* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  // Returns an instance of the AboutSigninInternalsFactory singleton.
  static AboutSigninInternalsFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AboutSigninInternalsFactory>;

  AboutSigninInternalsFactory();
  ~AboutSigninInternalsFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  DISALLOW_COPY_AND_ASSIGN(AboutSigninInternalsFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SIGNIN_ABOUT_SIGNIN_INTERNALS_FACTORY_H_
