// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace syncer {
class ProfileSyncService;
class SyncService;
}  // namespace syncer

// Singleton that owns all SyncServices and associates them with
// ChromeBrowserState.
class ProfileSyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static syncer::SyncService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static syncer::SyncService* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);

  static syncer::ProfileSyncService* GetAsProfileSyncServiceForBrowserState(
      ChromeBrowserState* browser_state);

  static syncer::ProfileSyncService*
  GetAsProfileSyncServiceForBrowserStateIfExists(
      ChromeBrowserState* browser_state);

  static ProfileSyncServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ProfileSyncServiceFactory>;

  ProfileSyncServiceFactory();
  ~ProfileSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_
