// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_REQUEST_COORDINATOR_FACTORY_H_
#define CHROME_BROWSER_OFFLINE_PAGES_REQUEST_COORDINATOR_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace offline_pages {

class RequestCoordinator;

// A factory to create one unique RequestCoordinator.
class RequestCoordinatorFactory : public BrowserContextKeyedServiceFactory {
 public:
  static RequestCoordinatorFactory* GetInstance();
  static RequestCoordinator* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<RequestCoordinatorFactory>;

  RequestCoordinatorFactory();
  ~RequestCoordinatorFactory() override {}

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(RequestCoordinatorFactory);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_REQUEST_COORDINATOR_FACTORY_H_
