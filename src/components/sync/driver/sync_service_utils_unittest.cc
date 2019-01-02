// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_utils.h"

#include <vector>
#include "components/sync/base/model_type.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/driver/sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class TestSyncService : public FakeSyncService {
 public:
  TestSyncService() = default;
  ~TestSyncService() override = default;

  void SetDisableReasons(int disable_reasons) {
    disable_reasons_ = disable_reasons;
  }
  void SetTransportState(TransportState state) { state_ = state; }
  void SetLocalSyncEnabled(bool local) { local_sync_enabled_ = local; }
  void SetPreferredDataTypes(const ModelTypeSet& types) {
    preferred_data_types_ = types;
  }
  void SetActiveDataTypes(const ModelTypeSet& types) {
    active_data_types_ = types;
  }
  void SetCustomPassphraseEnabled(bool enabled) {
    custom_passphrase_enabled_ = enabled;
  }
  void SetSyncCycleComplete(bool complete) { sync_cycle_complete_ = complete; }

  // SyncService implementation.
  int GetDisableReasons() const override { return disable_reasons_; }
  TransportState GetTransportState() const override { return state_; }
  bool IsLocalSyncEnabled() const override { return local_sync_enabled_; }
  bool IsFirstSetupComplete() const override { return true; }
  ModelTypeSet GetPreferredDataTypes() const override {
    return preferred_data_types_;
  }
  ModelTypeSet GetActiveDataTypes() const override {
    if (!IsSyncActive())
      return ModelTypeSet();
    return active_data_types_;
  }
  ModelTypeSet GetEncryptedDataTypes() const override {
    if (!custom_passphrase_enabled_) {
      // PASSWORDS are always encrypted.
      return ModelTypeSet(syncer::PASSWORDS);
    }
    // Some types can never be encrypted, e.g. DEVICE_INFO and
    // AUTOFILL_WALLET_DATA, so make sure we don't report them as encrypted.
    return syncer::Intersection(preferred_data_types_,
                                syncer::EncryptableUserTypes());
  }
  SyncCycleSnapshot GetLastCycleSnapshot() const override {
    if (sync_cycle_complete_) {
      return SyncCycleSnapshot(
          ModelNeutralState(), ProgressMarkerMap(), false, 5, 2, 7, false, 0,
          base::Time::Now(), base::Time::Now(),
          std::vector<int>(MODEL_TYPE_COUNT, 0),
          std::vector<int>(MODEL_TYPE_COUNT, 0),
          sync_pb::SyncEnums::UNKNOWN_ORIGIN,
          /*short_poll_interval=*/base::TimeDelta::FromMinutes(30),
          /*long_poll_interval=*/base::TimeDelta::FromMinutes(180),
          /*has_remaining_local_changes=*/false);
    }
    return SyncCycleSnapshot();
  }
  bool IsUsingSecondaryPassphrase() const override {
    return custom_passphrase_enabled_;
  }

 private:
  int disable_reasons_ = DISABLE_REASON_PLATFORM_OVERRIDE;
  TransportState state_ = TransportState::DISABLED;
  bool sync_cycle_complete_ = false;
  bool local_sync_enabled_ = false;
  ModelTypeSet preferred_data_types_;
  ModelTypeSet active_data_types_;
  bool custom_passphrase_enabled_ = false;
};

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledIfSyncNotAllowed) {
  TestSyncService service;

  // If sync is not allowed, uploading should never be enabled, even if all the
  // data types are enabled.
  service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);

  service.SetPreferredDataTypes(ProtocolTypes());
  service.SetActiveDataTypes(ProtocolTypes());

  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Once sync gets allowed (e.g. policy is updated), uploading should not be
  // disabled anymore (though not necessarily active yet).
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetTransportState(
      syncer::SyncService::TransportState::WAITING_FOR_START_REQUEST);

  EXPECT_NE(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest,
     UploadToGoogleInitializingUntilConfiguredAndActiveAndSyncCycleComplete) {
  TestSyncService service;
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetTransportState(
      syncer::SyncService::TransportState::WAITING_FOR_START_REQUEST);
  service.SetPreferredDataTypes(ProtocolTypes());
  service.SetActiveDataTypes(ProtocolTypes());

  // By default, if sync isn't disabled, we should be INITIALIZING.
  EXPECT_EQ(UploadState::INITIALIZING,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Finished configuration is not enough, still INITIALIZING.
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  EXPECT_EQ(UploadState::INITIALIZING,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Only after a sync cycle has been completed is upload actually ACTIVE.
  service.SetSyncCycleComplete(true);
  EXPECT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledForModelType) {
  TestSyncService service;
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  service.SetSyncCycleComplete(true);

  // Sync is enabled only for a specific model type.
  service.SetPreferredDataTypes(ModelTypeSet(syncer::BOOKMARKS));
  service.SetActiveDataTypes(ModelTypeSet(syncer::BOOKMARKS));

  // Sanity check: Upload is ACTIVE for this model type.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // ...but not for other types.
  EXPECT_EQ(
      UploadState::NOT_ACTIVE,
      GetUploadToGoogleState(&service, syncer::HISTORY_DELETE_DIRECTIVES));
  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::PREFERENCES));
}

TEST(SyncServiceUtilsTest,
     UploadToGoogleDisabledForModelTypeThatFailedToStart) {
  TestSyncService service;
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  service.SetSyncCycleComplete(true);

  // Sync is enabled for some model types.
  service.SetPreferredDataTypes(
      ModelTypeSet(syncer::BOOKMARKS, syncer::PREFERENCES));
  // But one of them fails to actually start up!
  service.SetActiveDataTypes(ModelTypeSet(syncer::BOOKMARKS));

  // Sanity check: Upload is ACTIVE for the model type that did start up.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // ...but not for the type that failed.
  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::PREFERENCES));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledIfLocalSyncEnabled) {
  TestSyncService service;
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetPreferredDataTypes(ProtocolTypes());
  service.SetActiveDataTypes(ProtocolTypes());
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  service.SetSyncCycleComplete(true);

  // Sanity check: Upload is active now.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // If we're in "local sync" mode, uploading should never be enabled, even if
  // configuration is done and all the data types are enabled.
  service.SetLocalSyncEnabled(true);

  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledOnPersistentAuthError) {
  TestSyncService service;
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetPreferredDataTypes(ProtocolTypes());
  service.SetActiveDataTypes(ProtocolTypes());
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  service.SetSyncCycleComplete(true);

  // Sanity check: Upload is active now.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // On a transient error, uploading goes back to INITIALIZING.
  GoogleServiceAuthError transient_error(
      GoogleServiceAuthError::CONNECTION_FAILED);
  ASSERT_TRUE(transient_error.IsTransientError());
  service.set_auth_error(transient_error);

  EXPECT_EQ(UploadState::INITIALIZING,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // On a persistent error, uploading is not considered active anymore (even
  // though Sync may still be considered active).
  GoogleServiceAuthError persistent_error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  ASSERT_TRUE(persistent_error.IsPersistentError());
  service.set_auth_error(persistent_error);

  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Once the auth error is resolved (e.g. user re-authenticated), uploading is
  // active again.
  service.set_auth_error(GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);

  EXPECT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledIfCustomPassphraseInUse) {
  TestSyncService service;
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetPreferredDataTypes(ProtocolTypes());
  service.SetActiveDataTypes(ProtocolTypes());
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  service.SetSyncCycleComplete(true);

  // Sanity check: Upload is ACTIVE, even for data types that are always
  // encrypted implicitly (PASSWORDS).
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::PASSWORDS));
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::DEVICE_INFO));

  // Once a custom passphrase is in use, upload should be considered disabled:
  // Even if we're technically still uploading, Google can't inspect the data.
  service.SetCustomPassphraseEnabled(true);

  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::PASSWORDS));
  // But unencryptable types like DEVICE_INFO are still active.
  EXPECT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::DEVICE_INFO));
}

}  // namespace syncer
