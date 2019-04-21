// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GCM_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_GCM_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace instance_id {

class InstanceIDProfileService;

// Singleton that owns all InstanceIDProfileService and associates them with
// profiles.
class InstanceIDProfileServiceFactory :
    public BrowserContextKeyedServiceFactory {
 public:
  static InstanceIDProfileService* GetForProfile(
      content::BrowserContext* profile);
  static InstanceIDProfileServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<InstanceIDProfileServiceFactory>;

  InstanceIDProfileServiceFactory();
  ~InstanceIDProfileServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(InstanceIDProfileServiceFactory);
};

}  // namespace instance_id

#endif  // CHROME_BROWSER_GCM_INSTANCE_ID_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
