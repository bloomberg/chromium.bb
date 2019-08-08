// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_DELEGATE_IMPL_FACTORY_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_DELEGATE_IMPL_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace web_app {

class WebAppUiDelegateImpl;

// Singleton that owns all WebAppUiDelegateImplFactories and associated them
// with Profile.
class WebAppUiDelegateImplFactory : public BrowserContextKeyedServiceFactory {
 public:
  static WebAppUiDelegateImpl* GetForProfile(Profile* profile);

  static WebAppUiDelegateImplFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<WebAppUiDelegateImplFactory>;

  WebAppUiDelegateImplFactory();
  ~WebAppUiDelegateImplFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebAppUiDelegateImplFactory);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_DELEGATE_IMPL_FACTORY_H_
