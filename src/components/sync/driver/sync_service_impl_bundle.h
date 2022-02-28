// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_IMPL_BUNDLE_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_IMPL_BUNDLE_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/driver/fake_sync_api_component_factory.h"
#include "components/sync/driver/mock_trusted_vault_client.h"
#include "components/sync/driver/sync_client_mock.h"
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/invalidations/mock_sync_invalidations_service.h"
#include "components/sync/invalidations/switches.h"
#include "services/network/test/test_url_loader_factory.h"

namespace syncer {

// Aggregate this class to get all necessary support for creating a
// SyncServiceImpl in tests. The test still needs to have its own
// MessageLoop, though.
class SyncServiceImplBundle {
 public:
  SyncServiceImplBundle();

  ~SyncServiceImplBundle();

  SyncServiceImplBundle(const SyncServiceImplBundle&) = delete;
  SyncServiceImplBundle& operator=(const SyncServiceImplBundle&) = delete;

  // Creates a mock sync client that leverages the dependencies in this bundle.
  std::unique_ptr<SyncClientMock> CreateSyncClientMock();

  // Creates an InitParams instance with the specified |start_behavior| and
  // |sync_client|, and fills the rest with dummy values and objects owned by
  // the bundle.
  SyncServiceImpl::InitParams CreateBasicInitParams(
      SyncServiceImpl::StartBehavior start_behavior,
      std::unique_ptr<SyncClient> sync_client);

  // Accessors

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  FakeSyncApiComponentFactory* component_factory() {
    return &component_factory_;
  }

  MockSyncInvalidationsService* sync_invalidations_service() {
    if (base::FeatureList::IsEnabled(switches::kSyncSendInterestedDataTypes)) {
      return &sync_invalidations_service_;
    } else {
      return nullptr;
    }
  }

  MockTrustedVaultClient* trusted_vault_client() {
    return &trusted_vault_client_;
  }

 private:
  TestingPrefServiceSimple pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  FakeSyncApiComponentFactory component_factory_;
  testing::NiceMock<MockSyncInvalidationsService> sync_invalidations_service_;
  testing::NiceMock<MockTrustedVaultClient> trusted_vault_client_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_IMPL_BUNDLE_H_
