// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_MANAGER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace file_manager {

class VolumeManager;

// Factory to create VolumeManager.
class VolumeManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns VolumeManager instance.
  static VolumeManager* Get(content::BrowserContext* context);

  static VolumeManagerFactory* GetInstance();

  VolumeManagerFactory(const VolumeManagerFactory&) = delete;
  VolumeManagerFactory& operator=(const VolumeManagerFactory&) = delete;

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

 private:
  // For Singleton.
  friend struct base::DefaultSingletonTraits<VolumeManagerFactory>;

  VolumeManagerFactory();
  ~VolumeManagerFactory() override;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_MANAGER_FACTORY_H_
