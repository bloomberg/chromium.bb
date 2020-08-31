// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace autofill {

class PersonalDataManager;

// Singleton that owns all PersonalDataManagers and associates them with
// ChromeBrowserState.
class PersonalDataManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static PersonalDataManager* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static PersonalDataManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<PersonalDataManagerFactory>;

  PersonalDataManagerFactory();
  ~PersonalDataManagerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PersonalDataManagerFactory);
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_FACTORY_H_
