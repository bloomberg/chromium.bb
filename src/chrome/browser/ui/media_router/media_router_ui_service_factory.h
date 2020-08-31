// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_SERVICE_FACTORY_H_

#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace media_router {

class MediaRouterUIService;

class MediaRouterUIServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static MediaRouterUIService* GetForBrowserContext(
      content::BrowserContext* context);

  static MediaRouterUIServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<MediaRouterUIServiceFactory>;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUIServiceFactoryUnitTest, CreateService);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUIServiceFactoryUnitTest,
                           DoNotCreateActionControllerWhenDisabled);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUIServiceFactoryUnitTest,
                           DisablingMediaRouting);

  MediaRouterUIServiceFactory();
  ~MediaRouterUIServiceFactory() override;

  // BrowserContextKeyedServiceFactory interface.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
#if !defined(OS_ANDROID)
  bool ServiceIsCreatedWithBrowserContext() const override;
#endif
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterUIServiceFactory);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_SERVICE_FACTORY_H_
