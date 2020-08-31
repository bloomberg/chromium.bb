// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_PRERENDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PRERENDER_PRERENDER_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class PrerenderService;

// Singleton that creates the PrerenderService and associates that service with
// ChromeBrowserState.
class PrerenderServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static PrerenderService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static PrerenderServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<PrerenderServiceFactory>;

  PrerenderServiceFactory();
  ~PrerenderServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(PrerenderServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_PRERENDER_PRERENDER_SERVICE_FACTORY_H_
