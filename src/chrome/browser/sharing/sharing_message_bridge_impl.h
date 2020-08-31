// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_MESSAGE_BRIDGE_IMPL_H_
#define CHROME_BROWSER_SHARING_SHARING_MESSAGE_BRIDGE_IMPL_H_

#include <memory>

#include "base/timer/timer.h"
#include "chrome/browser/sharing/sharing_message_bridge.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"

// Class that implements sending sharing messages using Sync. This class
// implements interaction with sync service. Sharing message data type is not
// stored in any persistent storage.
class SharingMessageBridgeImpl : public SharingMessageBridge,
                                 public syncer::ModelTypeSyncBridge {
 public:
  explicit SharingMessageBridgeImpl(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
  ~SharingMessageBridgeImpl() override;
  SharingMessageBridgeImpl(const SharingMessageBridgeImpl&) = delete;
  SharingMessageBridgeImpl& operator=(const SharingMessageBridgeImpl&) = delete;

  // SharingMessageBridge implementation.
  void SendSharingMessage(
      std::unique_ptr<sync_pb::SharingMessageSpecifics> specifics,
      CommitFinishedCallback on_commit_callback) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override;

  // ModelTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void OnCommitAttemptErrors(
      const syncer::FailedCommitResponseDataList& error_response_list) override;
  void OnCommitAttemptFailed(syncer::SyncCommitError commit_error) override;
  void ApplyStopSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                metadata_change_list) override;

  size_t GetCallbacksCountForTesting() const {
    return commit_callbacks_.size();
  }

 private:
  class TimedCallback {
   public:
    // Stores the |commit_callback| and starts a timer to call the
    // |timeout_callback| if timeout happens.
    TimedCallback(CommitFinishedCallback commit_callback,
                  base::OnceClosure timeout_callback);

    ~TimedCallback();

    // Runs callback object with the given |commit_error| and stops the timer.
    void Run(const sync_pb::SharingMessageCommitError& commit_error);

   private:
    base::OneShotTimer timer_;
    CommitFinishedCallback commit_callback_;
  };

  // Process timeout which happened for a callback with associated
  // |client_tag_hash|.
  void ProcessCommitTimeout(syncer::ClientTagHash client_tag_hash);

  // Sends commit outcome via callback for |client_tag_hash| and removes it from
  // callbacks mapping.
  void ProcessCommitResponse(
      const syncer::ClientTagHash& client_tag_hash,
      const sync_pb::SharingMessageCommitError& commit_error);

  std::map<syncer::ClientTagHash, std::unique_ptr<TimedCallback>>
      commit_callbacks_;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_MESSAGE_BRIDGE_IMPL_H_
