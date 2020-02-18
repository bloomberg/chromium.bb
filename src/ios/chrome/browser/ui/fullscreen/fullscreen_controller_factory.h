// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace ios {
class ChromeBrowserState;
}

class FullscreenController;

// FullscreenControllerFactory attaches FullscreenControllers to
// ChromeBrowserStates.
class FullscreenControllerFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Convenience getter that typecasts the value returned to a
  // FullscreenController.
  static FullscreenController* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  // Getter for singleton instance.
  static FullscreenControllerFactory* GetInstance();

 private:
  friend class base::NoDestructor<FullscreenControllerFactory>;

  FullscreenControllerFactory();

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(FullscreenControllerFactory);
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_FACTORY_H_
