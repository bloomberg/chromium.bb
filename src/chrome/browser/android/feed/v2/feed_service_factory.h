// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_V2_FEED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_FEED_V2_FEED_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace feed {

class FeedService;

// Factory to create one FeedService per browser context. Callers need to
// watch out for nullptr when incognito, as the feed should not be used then.
// TODO(harringtond): Under development, doesn't work yet!
class FeedServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  FeedServiceFactory(const FeedServiceFactory&) = delete;
  FeedServiceFactory& operator=(const FeedServiceFactory&) = delete;

  static FeedService* GetForBrowserContext(content::BrowserContext* context);

  static FeedServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<FeedServiceFactory>;

  FeedServiceFactory();
  ~FeedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_V2_FEED_SERVICE_FACTORY_H_
