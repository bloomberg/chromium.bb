// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_

#include <vector>

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace syncer {
class DeviceInfoSyncService;
class DeviceInfoTracker;
}  // namespace syncer

class DeviceInfoSyncServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static syncer::DeviceInfoSyncService* GetForProfile(Profile* profile);
  static DeviceInfoSyncServiceFactory* GetInstance();

  // Iterates over all of the profiles that have been loaded so far, and
  // extracts their tracker if present. If some profiles don't have trackers, no
  // indication is given in the passed vector.
  static void GetAllDeviceInfoTrackers(
      std::vector<const syncer::DeviceInfoTracker*>* trackers);

 private:
  friend struct base::DefaultSingletonTraits<DeviceInfoSyncServiceFactory>;

  DeviceInfoSyncServiceFactory();
  ~DeviceInfoSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfoSyncServiceFactory);
};

#endif  // CHROME_BROWSER_SYNC_DEVICE_INFO_SYNC_SERVICE_FACTORY_H_
