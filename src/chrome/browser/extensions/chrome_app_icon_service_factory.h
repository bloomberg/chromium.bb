// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class ChromeAppIconService;

// Factory to create ChromeAppIconService. Use helper
// ChromeAppIconService::Get(context) to access the service.
class ChromeAppIconServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ChromeAppIconService* GetForBrowserContext(
      content::BrowserContext* context);

  static ChromeAppIconServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ChromeAppIconServiceFactory>;

  ChromeAppIconServiceFactory();
  ~ChromeAppIconServiceFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppIconServiceFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_SERVICE_FACTORY_H_
