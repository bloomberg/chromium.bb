// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_MODEL_TYPE_WORKER_H_
#define COMPONENTS_SYNC_ENGINE_MODEL_TYPE_WORKER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/commit_contributor.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/nigori/cryptographer.h"
#include "components/sync/engine/nudge_handler.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/update_handler.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

class CancelationSignal;
class ModelTypeProcessor;

// A smart cache for sync types to communicate with the sync thread.
//
// When the sync data type wants to talk to the sync server, it will
// send a message from its thread to this object on the sync thread. This
// object ensures the appropriate sync server communication gets scheduled and
// executed. The response, if any, will be returned to the data types's thread
// eventually.
//
// This object also has a role to play in communications in the opposite
// direction. Sometimes the sync thread will receive changes from the sync
// server and deliver them here. This object will post this information back to
// the appropriate component on the model type's thread.
//
// This object does more than just pass along messages. It understands the sync
// protocol, and it can make decisions when it sees conflicting messages. For
// example, if the sync server sends down an update for a sync entity that is
// currently pending for commit, this object will detect this condition and
// cancel the pending commit.
class ModelTypeWorker : public UpdateHandler,
                        public CommitContributor,
                        public CommitQueue {
 public:
  // Public for testing.
  enum DecryptionStatus { SUCCESS, DECRYPTION_PENDING, FAILED_TO_DECRYPT };

  ModelTypeWorker(ModelType type,
                  const sync_pb::ModelTypeState& initial_state,
                  bool trigger_initial_sync,
                  std::unique_ptr<Cryptographer> cryptographer,
                  PassphraseType passphrase_type,
                  NudgeHandler* nudge_handler,
                  std::unique_ptr<ModelTypeProcessor> model_type_processor,
                  CancelationSignal* cancelation_signal);
  ~ModelTypeWorker() override;

  // Public for testing.
  // |cryptographer| can be null.
  // |response_data| must be not null.
  static DecryptionStatus PopulateUpdateResponseData(
      const Cryptographer* cryptographer,
      ModelType model_type,
      const sync_pb::SyncEntity& update_entity,
      UpdateResponseData* response_data);

  ModelType GetModelType() const;

  void UpdateCryptographer(std::unique_ptr<Cryptographer> cryptographer);
  // Causes a worker without cryptographer to record whether
  // |fallback_cryptographer_for_uma| would have been able to decrypt incoming
  // encrypted updates. |fallback_cryptographer_for_uma| must be non-null.
  void UpdateFallbackCryptographerForUma(
      std::unique_ptr<Cryptographer> fallback_cryptographer_for_uma);
  void UpdatePassphraseType(PassphraseType type);

  // UpdateHandler implementation.
  bool IsInitialSyncEnded() const override;
  const sync_pb::DataTypeProgressMarker& GetDownloadProgress() const override;
  const sync_pb::DataTypeContext& GetDataTypeContext() const override;
  SyncerError ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const sync_pb::DataTypeContext& mutated_context,
      const SyncEntityList& applicable_updates,
      StatusController* status) override;
  void ApplyUpdates(StatusController* status) override;
  void PassiveApplyUpdates(StatusController* status) override;

  // CommitQueue implementation.
  void NudgeForCommit() override;

  // CommitContributor implementation.
  std::unique_ptr<CommitContribution> GetContribution(
      size_t max_entries) override;

  // An alternative way to drive sending data to the processor, that should be
  // called when a new encryption mechanism is ready.
  void EncryptionAcceptedMaybeApplyUpdates();

  // Public for testing.
  // Returns true if this type should stop communicating because of outstanding
  // encryption issues and must wait for keys to be updated.
  bool BlockForEncryption() const;

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  base::WeakPtr<ModelTypeWorker> AsWeakPtr();

  bool HasLocalChangesForTest() const;

  void SetMinGuResponsesToIgnoreKeyForTest(int min_gu_responses_to_ignore_key) {
    min_gu_responses_to_ignore_key_ = min_gu_responses_to_ignore_key;
  }

 private:
  struct UnknownEncryptionKeyInfo {
    // Not increased if the cryptographer knows it's in a pending state
    // (cf. Cryptographer::CanEncrypt()).
    int gu_responses_while_should_have_been_known = 0;
  };

  // Attempts to decrypt the given specifics and return them in the |out|
  // parameter. The cryptographer must know the decryption key, i.e.
  // cryptographer.CanDecrypt(specifics.encrypted()) must return true.
  //
  // Returns false if the decryption failed. There are no guarantees about the
  // contents of |out| when that happens.
  //
  // In theory, this should never fail. Only corrupt or invalid entries could
  // cause this to fail, and no clients are known to create such entries. The
  // failure case is an attempt to be defensive against bad input.
  static bool DecryptSpecifics(const Cryptographer& cryptographer,
                               const sync_pb::EntitySpecifics& in,
                               sync_pb::EntitySpecifics* out);

  // Attempts to decrypt the given password specifics and return them in the
  // |out| parameter. The cryptographer must know the decryption key, i.e.
  // cryptographer.CanDecrypt(in.password().encrypted()) must return true.
  //
  // Returns false if the decryption failed. There are no guarantees about the
  // contents of |out| when that happens.
  //
  // In theory, this should never fail. Only corrupt or invalid entries could
  // cause this to fail, and no clients are known to create such entries. The
  // failure case is an attempt to be defensive against bad input.
  static bool DecryptPasswordSpecifics(const Cryptographer& cryptographer,
                                       const sync_pb::EntitySpecifics& in,
                                       sync_pb::EntitySpecifics* out);

  // Helper function to actually send |pending_updates_| to the processor.
  void ApplyPendingUpdates();

  // Returns true if this type has successfully fetched all available updates
  // from the server at least once. Our state may or may not be stale, but at
  // least we know that it was valid at some point in the past.
  bool IsTypeInitialized() const;

  // Returns true if this type is prepared to commit items. Currently, this
  // depends on having downloaded the initial data and having the encryption
  // settings in a good state.
  bool CanCommitItems() const;

  // Updates the encryption key name stored in |model_type_state_| if it differs
  // from the default encryption key name in |cryptographer_|. Returns whether
  // an update occurred.
  bool UpdateEncryptionKeyName();

  // Iterates through all elements in |entries_pending_decryption_| and tries to
  // decrypt anything that has encrypted data.
  // Should only be called during a GetUpdates cycle.
  void DecryptStoredEntities();

  // Nudges nudge_handler_ when initial sync is done, processor has local
  // changes and either encryption is disabled for the type or cryptographer is
  // ready (doesn't have pending keys).
  void NudgeIfReadyToCommit();

  // Filters our duplicate updates from |pending_updates_| based on the server
  // id. It discards all of them except the last one.
  void DeduplicatePendingUpdatesBasedOnServerId();

  // Filters our duplicate updates from |pending_updates_| based on the client
  // tag hash. It discards all of them except the last one.
  void DeduplicatePendingUpdatesBasedOnClientTagHash();

  // Filters our duplicate updates from |pending_updates_| based on the
  // originator item ID (in practice used for bookmarks only). It discards all
  // of them except the last one.
  void DeduplicatePendingUpdatesBasedOnOriginatorClientItemId();

  // Callback for when our contribution gets a response.
  void OnCommitResponse(
      const CommitResponseDataList& committed_response_list,
      const FailedCommitResponseDataList& error_response_list);

  // Callback when there is no response or server returns an error without
  // response body.
  void OnFullCommitFailure(SyncCommitError commit_error);

  // Returns true for keys that have remained unknown for so long that they are
  // not expected to arrive anytime soon. The worker ignores incoming updates
  // encrypted with them, and drops pending ones on the next GetUpdatesResponse.
  // Those keys remain in |unknown_encryption_keys_by_name_|.
  bool ShouldIgnoreUpdatesEncryptedWith(const std::string& key_name);

  // If |key_name| should be ignored (cf. ShouldIgnoreUpdatesEncryptedWith()),
  // drops elements of |entries_pending_decryption_| encrypted with it.
  void MaybeDropPendingUpdatesEncryptedWith(const std::string& key_name);

  // Removes elements of |unknown_encryption_keys_by_name_| that no longer fit
  // the definition of an unknown key, and returns their info.
  std::vector<UnknownEncryptionKeyInfo> RemoveKeysNoLongerUnknown();

  void RecordBlockedByUndecryptableUpdate();

  ModelType type_;

  // State that applies to the entire model type.
  sync_pb::ModelTypeState model_type_state_;

  // Pointer to the ModelTypeProcessor associated with this worker. Never null.
  std::unique_ptr<ModelTypeProcessor> model_type_processor_;

  // A private copy of the most recent cryptographer known to sync.
  // Initialized at construction time and updated with UpdateCryptographer().
  // null if encryption is not enabled for this type.
  std::unique_ptr<Cryptographer> cryptographer_;

  // Used to investigate an issue where the worker receives encrypted updates
  // despite not having a |cryptographer_| (|type_| is not an encrypted type).
  // In those cases, this will eventually hold the underlying cryptographer used
  // by other types. The worker then records whether that cryptographer is able
  // to decrypt the updates. See crbug.com/1178418.
  std::unique_ptr<Cryptographer> fallback_cryptographer_for_uma_;

  // A private copy of the most recent passphrase type. Initialized at
  // construction time and updated with UpdatePassphraseType().
  PassphraseType passphrase_type_;

  // Interface used to access and send nudges to the sync scheduler. Not owned.
  NudgeHandler* nudge_handler_;

  // A map of sync entities, keyed by server_id. Holds updates encrypted with
  // pending keys. Entries are stored in a map for de-duplication (applying only
  // the latest).
  // TODO(crbug.com/1109221): Use a name mentioning "updates" and "server id".
  std::map<std::string, sync_pb::SyncEntity> entries_pending_decryption_;

  // A key is said to be unknown if one of these is true:
  // a) It encrypts some updates(s) in |entries_pending_decryption_|.
  // b) (a) happened for so long that the worker dropped those updates in an
  // attempt to unblock itself (cf. ShouldIgnoreUpdatesEncryptedWith()).
  // The key is added here when the worker receives the first update entity
  // encrypted with it.
  std::map<std::string, UnknownEncryptionKeyInfo>
      unknown_encryption_keys_by_name_;

  // Accumulates all the updates from a single GetUpdates cycle in memory so
  // they can all be sent to the processor at once.
  UpdateResponseDataList pending_updates_;

  // Indicates if processor has local changes. Processor only nudges worker once
  // and worker might not be ready to commit entities at the time.
  bool has_local_changes_ = false;

  // Remains constant in production code. Can be overridden in tests.
  // |UnknownEncryptionKeyInfo::gu_responses_while_should_have_been_known| must
  // be above this value before updates encrypted with the key are ignored.
  int min_gu_responses_to_ignore_key_;

  // Cancellation signal is used to cancel blocking operation on engine
  // shutdown.
  CancelationSignal* cancelation_signal_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ModelTypeWorker> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ModelTypeWorker);
};

// GetLocalChangesRequest is a container for GetLocalChanges call response. It
// allows sync thread to block waiting for model thread to call SetResponse.
// This class supports canceling blocking call through CancelationSignal during
// sync engine shutdown.
//
// It should be used in the following manner:
// scoped_refptr<GetLocalChangesRequest> request =
//     base::MakeRefCounted<GetLocalChangesRequest>(cancelation_signal_);
// model_type_processor_->GetLocalChanges(
//     max_entries,
//     base::BindOnce(&GetLocalChangesRequest::SetResponse, request));
// request->WaitForResponseOrCancelation();
// CommitRequestDataList response;
// if (!request->WasCancelled())
//   response = request->ExtractResponse();
class GetLocalChangesRequest
    : public base::RefCountedThreadSafe<GetLocalChangesRequest>,
      public CancelationSignal::Observer {
 public:
  explicit GetLocalChangesRequest(CancelationSignal* cancelation_signal);

  // CancelationSignal::Observer implementation.
  void OnCancelationSignalReceived() override;

  // Blocks current thread until either SetResponse is called or
  // cancelation_signal_ is signaled.
  void WaitForResponseOrCancelation();

  // SetResponse takes ownership of |local_changes| and unblocks
  // WaitForResponseOrCancelation call. It is called by model type through
  // callback passed to GetLocalChanges.
  void SetResponse(CommitRequestDataList&& local_changes);

  // Checks if WaitForResponseOrCancelation was canceled through
  // CancelationSignal. When returns true calling ExtractResponse is unsafe.
  bool WasCancelled();

  // Returns response set by SetResponse().
  CommitRequestDataList&& ExtractResponse();

 private:
  friend class base::RefCountedThreadSafe<GetLocalChangesRequest>;
  ~GetLocalChangesRequest() override;

  CancelationSignal* cancelation_signal_;
  base::WaitableEvent response_accepted_;
  CommitRequestDataList response_;

  DISALLOW_COPY_AND_ASSIGN(GetLocalChangesRequest);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_MODEL_TYPE_WORKER_H_
