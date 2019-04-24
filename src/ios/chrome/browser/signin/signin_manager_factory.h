// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class SigninManager;

namespace ios {

class ChromeBrowserState;

// Singleton that owns all SigninManagers and associates them with browser
// states.
class SigninManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static SigninManager* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static SigninManager* GetForBrowserStateIfExists(
      ios::ChromeBrowserState* browser_state);

  // Returns an instance of the SigninManagerFactory singleton.
  static SigninManagerFactory* GetInstance();

  // Implementation of BrowserStateKeyedServiceFactory (public so tests
  // can call it).
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

 private:
  friend class base::NoDestructor<SigninManagerFactory>;

  SigninManagerFactory();
  ~SigninManagerFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};
}

#endif  // IOS_CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FACTORY_H_
