// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NSS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NET_NSS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class NssService;

class NssServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the NssService for `context`.
  static NssService* GetForContext(content::BrowserContext* context);
  static NssServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<NssServiceFactory>;

  NssServiceFactory();
  ~NssServiceFactory() override;

  // `BrowserContextKeyedServiceFactory` implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_NET_NSS_SERVICE_FACTORY_H_
