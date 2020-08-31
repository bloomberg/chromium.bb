// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class NearbySharingService;

// Factory for NearbySharingService.
class NearbySharingServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Disallow copy and assignment.
  NearbySharingServiceFactory(const NearbySharingServiceFactory&) = delete;
  NearbySharingServiceFactory& operator=(const NearbySharingServiceFactory&) =
      delete;

  // Returns singleton instance of NearbySharingServiceFactory.
  static NearbySharingServiceFactory* GetInstance();

  // Returns the NearbySharingService associated with |context|.
  static NearbySharingService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<NearbySharingServiceFactory>;

  NearbySharingServiceFactory();
  ~NearbySharingServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_FACTORY_H_
