// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_TEST_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_TEST_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/signin/core/browser/account_info.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

namespace syncer {

class TestSyncService : public SyncService {
 public:
  TestSyncService();
  ~TestSyncService() override;

  void SetDisableReasons(int disable_reasons);
  void SetTransportState(TransportState transport_state);
  void SetLocalSyncEnabled(bool local_sync_enabled);
  void SetAuthError(const GoogleServiceAuthError& auth_error);
  void SetPreferredDataTypes(const ModelTypeSet& types);
  void SetActiveDataTypes(const ModelTypeSet& types);
  void SetCustomPassphraseEnabled(bool enabled);
  void SetLastCycleSnapshot(const SyncCycleSnapshot& snapshot);

  // SyncService implementation.
  int GetDisableReasons() const override;
  TransportState GetTransportState() const override;
  bool IsLocalSyncEnabled() const override;
  AccountInfo GetAuthenticatedAccountInfo() const override;
  bool IsAuthenticatedAccountPrimary() const override;
  const GoogleServiceAuthError& GetAuthError() const override;

  bool IsFirstSetupComplete() const override;
  void SetFirstSetupComplete() override;

  std::unique_ptr<SyncSetupInProgressHandle> GetSetupInProgressHandle()
      override;
  bool IsSetupInProgress() const override;

  ModelTypeSet GetPreferredDataTypes() const override;
  ModelTypeSet GetActiveDataTypes() const override;

  void RequestStart() override;
  void RequestStop(SyncStopDataFate data_fate) override;
  void OnDataTypeRequestsSyncStartup(ModelType type) override;
  void OnUserChoseDatatypes(bool sync_everything,
                            ModelTypeSet chosen_types) override;
  void TriggerRefresh(const ModelTypeSet& types) override;
  void ReenableDatatype(ModelType type) override;
  void ReadyForStartChanged(syncer::ModelType type) override;

  void AddObserver(SyncServiceObserver* observer) override;
  void RemoveObserver(SyncServiceObserver* observer) override;
  bool HasObserver(const SyncServiceObserver* observer) const override;

  bool IsPassphraseRequiredForDecryption() const override;
  base::Time GetExplicitPassphraseTime() const override;
  bool IsUsingSecondaryPassphrase() const override;
  void EnableEncryptEverything() override;
  bool IsEncryptEverythingEnabled() const override;
  void SetEncryptionPassphrase(const std::string& passphrase,
                               PassphraseType type) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override;
  bool IsCryptographerReady(const BaseTransaction* trans) const override;

  SyncClient* GetSyncClient() const override;
  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate() override;
  UserShare* GetUserShare() const override;

  SyncTokenStatus GetSyncTokenStatus() const override;
  bool QueryDetailedSyncStatus(SyncStatus* result) const override;
  base::Time GetLastSyncedTime() const override;
  SyncCycleSnapshot GetLastCycleSnapshot() const override;
  std::unique_ptr<base::Value> GetTypeStatusMap() override;
  const GURL& sync_service_url() const override;
  std::string unrecoverable_error_message() const override;
  base::Location unrecoverable_error_location() const override;
  void AddProtocolEventObserver(ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(ProtocolEventObserver* observer) override;
  void AddTypeDebugInfoObserver(TypeDebugInfoObserver* observer) override;
  void RemoveTypeDebugInfoObserver(TypeDebugInfoObserver* observer) override;
  base::WeakPtr<JsController> GetJsController() override;
  void GetAllNodes(const base::Callback<void(std::unique_ptr<base::ListValue>)>&
                       callback) override;

  // DataTypeEncryptionHandler implementation.
  bool IsPassphraseRequired() const override;
  ModelTypeSet GetEncryptedDataTypes() const override;

  // KeyedService implementation.
  void Shutdown() override;

 private:
  int disable_reasons_ = DISABLE_REASON_NONE;
  TransportState transport_state_ = TransportState::ACTIVE;
  bool local_sync_enabled_ = false;
  AccountInfo account_info_;
  GoogleServiceAuthError auth_error_;

  ModelTypeSet preferred_data_types_;
  ModelTypeSet active_data_types_;

  bool custom_passphrase_enabled_ = false;

  SyncCycleSnapshot last_cycle_snapshot_;

  GURL sync_service_url_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_TEST_SYNC_SERVICE_H_
