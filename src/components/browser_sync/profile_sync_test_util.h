// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
#define COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/sync/device_info/device_info_sync_service_impl.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/driver/sync_client_mock.h"
#include "components/sync/model/test_model_type_store_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/test/test_url_loader_factory.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace browser_sync {

// Call this to register preferences needed for ProfileSyncService creation.
void RegisterPrefsForProfileSyncService(
    user_prefs::PrefRegistrySyncable* registry);

// Aggregate this class to get all necessary support for creating a
// ProfileSyncService in tests. The test still needs to have its own
// MessageLoop, though.
class ProfileSyncServiceBundle {
 public:
  ProfileSyncServiceBundle();

  ~ProfileSyncServiceBundle();

  // Creates a mock sync client that leverages the dependencies in this bundle.
  std::unique_ptr<syncer::SyncClientMock> CreateSyncClientMock();

  // Creates an InitParams instance with the specified |start_behavior| and
  // |sync_client|, and fills the rest with dummy values and objects owned by
  // the bundle.
  ProfileSyncService::InitParams CreateBasicInitParams(
      ProfileSyncService::StartBehavior start_behavior,
      std::unique_ptr<syncer::SyncClient> sync_client);

  // Accessors

  network::TestURLLoaderFactory* url_loader_factory() {
    return &test_url_loader_factory_;
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &pref_service_;
  }

  identity::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  identity::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  syncer::SyncApiComponentFactoryMock* component_factory() {
    return &component_factory_;
  }

  invalidation::ProfileIdentityProvider* identity_provider() {
    return identity_provider_.get();
  }

  invalidation::FakeInvalidationService* fake_invalidation_service() {
    return &fake_invalidation_service_;
  }

  syncer::DeviceInfoSyncService* device_info_sync_service() {
    return &device_info_sync_service_;
  }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  syncer::TestModelTypeStoreService model_type_store_service_;
  syncer::DeviceInfoSyncServiceImpl device_info_sync_service_;
  identity::IdentityTestEnvironment identity_test_env_;
  testing::NiceMock<syncer::SyncApiComponentFactoryMock> component_factory_;
  std::unique_ptr<invalidation::ProfileIdentityProvider> identity_provider_;
  invalidation::FakeInvalidationService fake_invalidation_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceBundle);
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
