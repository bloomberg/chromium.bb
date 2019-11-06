// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace web_app {

class WebAppUiService;

// Singleton that owns all WebAppUiServices and associates them
// with Profile.
class WebAppUiServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static WebAppUiService* GetForProfile(Profile* profile);

  static WebAppUiServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<WebAppUiServiceFactory>;

  WebAppUiServiceFactory();
  ~WebAppUiServiceFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebAppUiServiceFactory);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_SERVICE_FACTORY_H_
