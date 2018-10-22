// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_FAKE_SYNC_ENGINE_H_
#define COMPONENTS_SYNC_ENGINE_FAKE_SYNC_ENGINE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/sync_engine.h"

namespace syncer {

// A fake of the SyncEngine.
//
// This class implements the bare minimum required for the ProfileSyncService to
// get through initialization. It often returns null pointers or nonsense
// values; it is not intended to be used in tests that depend on SyncEngine
// behavior.
class FakeSyncEngine : public SyncEngine {
 public:
  FakeSyncEngine();
  ~FakeSyncEngine() override;

  // Immediately calls params.host->OnEngineInitialized.
  void Initialize(InitParams params) override;

  void TriggerRefresh(const ModelTypeSet& types) override;

  void UpdateCredentials(const SyncCredentials& credentials) override;

  void InvalidateCredentials() override;

  void StartConfiguration() override;

  void StartSyncingWithServer() override;

  void SetEncryptionPassphrase(const std::string& passphrase,
                               bool is_explicit) override;

  void SetDecryptionPassphrase(const std::string& passphrase) override;

  void StopSyncingForShutdown() override;

  void Shutdown(ShutdownReason reason) override;

  void ConfigureDataTypes(ConfigureParams params) override;

  void RegisterDirectoryDataType(ModelType type, ModelSafeGroup group) override;

  void UnregisterDirectoryDataType(ModelType type) override;

  void EnableEncryptEverything() override;

  void ActivateDirectoryDataType(ModelType type,
                                 ModelSafeGroup group,
                                 ChangeProcessor* change_processor) override;
  void DeactivateDirectoryDataType(ModelType type) override;

  void ActivateNonBlockingDataType(
      ModelType type,
      std::unique_ptr<DataTypeActivationResponse>) override;
  void DeactivateNonBlockingDataType(ModelType type) override;

  UserShare* GetUserShare() const override;

  Status GetDetailedStatus() override;

  void HasUnsyncedItemsForTest(
      base::OnceCallback<void(bool)> cb) const override;

  bool IsCryptographerReady(const BaseTransaction* trans) const override;

  void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) const override;

  void FlushDirectory() const override;

  void RequestBufferedProtocolEventsAndEnableForwarding() override;
  void DisableProtocolEventForwarding() override;

  void EnableDirectoryTypeDebugInfoForwarding() override;
  void DisableDirectoryTypeDebugInfoForwarding() override;

  void ClearServerData(const base::Closure& callback) override;

  void OnCookieJarChanged(bool account_mismatch,
                          bool empty_jar,
                          const base::Closure& callback) override;

  void set_fail_initial_download(bool should_fail);

 private:
  bool fail_initial_download_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_FAKE_SYNC_ENGINE_H_
