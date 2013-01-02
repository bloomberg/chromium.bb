// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_DEBUG_INFO_EVENT_LISTENER_H_
#define SYNC_INTERNAL_API_DEBUG_INFO_EVENT_LISTENER_H_

#include <queue>
#include <string>

#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/data_type_debug_info_listener.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/sync_encryption_handler.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/js/js_backend.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/debug_info_getter.h"

namespace syncer {

// In order to track datatype association results, we need at least as many
// entries as datatypes.
const unsigned int kMaxEntries = 25;

// Listens to events and records them in a queue. And passes the events to
// syncer when requested.
// This class is not thread safe and should only be accessed on the sync thread.
class SYNC_EXPORT_PRIVATE DebugInfoEventListener
    : public SyncManager::Observer,
      public SyncEncryptionHandler::Observer,
      public sessions::DebugInfoGetter,
      public DataTypeDebugInfoListener {
 public:
  DebugInfoEventListener();
  virtual ~DebugInfoEventListener();

  // SyncManager::Observer implementation.
  virtual void OnSyncCycleCompleted(
    const sessions::SyncSessionSnapshot& snapshot) OVERRIDE;
  virtual void OnInitializationComplete(
      const WeakHandle<JsBackend>& js_backend,
      const WeakHandle<DataTypeDebugInfoListener>& debug_listener,
      bool success, ModelTypeSet restored_types) OVERRIDE;
  virtual void OnConnectionStatusChange(
      ConnectionStatus connection_status) OVERRIDE;
  virtual void OnStopSyncingPermanently() OVERRIDE;
  virtual void OnUpdatedToken(const std::string& token) OVERRIDE;
  virtual void OnActionableError(
      const SyncProtocolError& sync_error) OVERRIDE;

  // SyncEncryptionHandler::Observer implementation.
  virtual void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) OVERRIDE;
  virtual void OnPassphraseAccepted() OVERRIDE;
  virtual void OnBootstrapTokenUpdated(
      const std::string& bootstrap_token,
      BootstrapTokenType type) OVERRIDE;
  virtual void OnEncryptedTypesChanged(
      ModelTypeSet encrypted_types,
      bool encrypt_everything) OVERRIDE;
  virtual void OnEncryptionComplete() OVERRIDE;
  virtual void OnCryptographerStateChanged(
      Cryptographer* cryptographer) OVERRIDE;
  virtual void OnPassphraseTypeChanged(
      PassphraseType type,
      base::Time explicit_passphrase_time) OVERRIDE;

  // Sync manager events.
  void OnNudgeFromDatatype(ModelType datatype);
  void OnIncomingNotification(
      const ModelTypeInvalidationMap& invalidation_map);

  // DebugInfoGetter implementation.
  virtual void GetAndClearDebugInfo(sync_pb::DebugInfo* debug_info) OVERRIDE;

  // DataTypeDebugInfoListener implementation.
  virtual void OnDataTypeAssociationComplete(
      const DataTypeAssociationStats& association_stats) OVERRIDE;
  virtual void OnConfigureComplete() OVERRIDE;

  // Returns a weak pointer to this object.
  base::WeakPtr<DataTypeDebugInfoListener> GetWeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyEventsAdded);
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyQueueSize);
  FRIEND_TEST_ALL_PREFIXES(DebugInfoEventListenerTest, VerifyGetAndClearEvents);

  void AddEventToQueue(const sync_pb::DebugEventInfo& event_info);
  void CreateAndAddEvent(sync_pb::DebugEventInfo::SingletonEventType type);
  std::queue<sync_pb::DebugEventInfo> events_;

  // True indicates we had to drop one or more events to keep our limit of
  // |kMaxEntries|.
  bool events_dropped_;

  // Cryptographer has keys that are not yet decrypted.
  bool cryptographer_has_pending_keys_;

  // Cryptographer is initialized and does not have pending keys.
  bool cryptographer_ready_;

  base::WeakPtrFactory<DebugInfoEventListener> weak_ptr_factory_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DebugInfoEventListener);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_DEBUG_INFO_EVENT_LISTENER_H_
