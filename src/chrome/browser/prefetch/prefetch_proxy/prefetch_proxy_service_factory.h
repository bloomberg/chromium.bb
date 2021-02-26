// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
class PrefetchProxyService;

class PrefetchProxyServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the PrefetchProxy for |profile|.
  static PrefetchProxyService* GetForProfile(Profile* profile);

  static PrefetchProxyServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PrefetchProxyServiceFactory>;

  PrefetchProxyServiceFactory();
  ~PrefetchProxyServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  PrefetchProxyServiceFactory(const PrefetchProxyServiceFactory&) = delete;
  PrefetchProxyServiceFactory& operator=(const PrefetchProxyServiceFactory&) =
      delete;
};

#endif  // CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_SERVICE_FACTORY_H_
