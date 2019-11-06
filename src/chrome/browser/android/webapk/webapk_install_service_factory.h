// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class WebApkInstallService;

// Factory for creating WebApkInstallService. Installing WebAPKs from incognito
// is unsupported.
class WebApkInstallServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static WebApkInstallServiceFactory* GetInstance();
  static WebApkInstallService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<WebApkInstallServiceFactory>;

  WebApkInstallServiceFactory();
  ~WebApkInstallServiceFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebApkInstallServiceFactory);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_FACTORY_H_
