// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_SYNC_MANAGER_H_
#define SYNC_INTERNAL_API_SYNC_MANAGER_H_

#include <string>
#include <vector>

#include "net/base/network_change_notifier.h"
#include "sync/base/sync_export.h"
#include "sync/engine/all_status.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/engine/nudge_handler.h"
#include "sync/engine/sync_engine_event_listener.h"
#include "sync/internal_api/change_reorder_buffer.h"
#include "sync/internal_api/debug_info_event_listener.h"
#include "sync/internal_api/js_mutation_event_observer.h"
#include "sync/internal_api/js_sync_encryption_handler_observer.h"
#include "sync/internal_api/js_sync_manager_observer.h"
#include "sync/internal_api/protocol_event_buffer.h"
#include "sync/internal_api/public/sync_context_proxy.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/internal_api/sync_encryption_handler_impl.h"
#include "sync/js/js_backend.h"
#include "sync/syncable/directory_change_delegate.h"
#include "sync/util/cryptographer.h"
#include "sync/util/time.h"

class GURL;

namespace syncer {

class ModelTypeRegistry;
class SyncAPIServerConnectionManager;
class SyncContext;
class TypeDebugInfoObserver;
class WriteNode;
class WriteTransaction;

namespace sessions {
class SyncSessionContext;
}

// SyncManager encapsulates syncable::Directory and serves as the parent of all
// other objects in the sync API.  If multiple threads interact with the same
// local sync repository (i.e. the same sqlite database), they should share a
// single SyncManager instance.  The caller should typically create one
// SyncManager for the lifetime of a user session.
//
// Unless stated otherwise, all methods of SyncManager should be called on the
// same thread.
class SYNC_EXPORT_PRIVATE SyncManagerImpl
    : public SyncManager,
      public net::NetworkChangeNotifier::IPAddressObserver,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public JsBackend,
      public SyncEngineEventListener,
      public ServerConnectionEventListener,
      public syncable::DirectoryChangeDelegate,
      public SyncEncryptionHandler::Observer,
      public NudgeHandler {
 public:
  // Create an uninitialized SyncManager.  Callers must Init() before using.
  explicit SyncManagerImpl(const std::string& name);
  ~SyncManagerImpl() override;

  // SyncManager implementation.
  void Init(InitArgs* args) override;
  ModelTypeSet InitialSyncEndedTypes() override;
  ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) override;
  bool PurgePartiallySyncedTypes() override;
  void UpdateCredentials(const SyncCredentials& credentials) override;
  void StartSyncingNormally(const ModelSafeRoutingInfo& routing_info) override;
  void ConfigureSyncer(ConfigureReason reason,
                       ModelTypeSet to_download,
                       ModelTypeSet to_purge,
                       ModelTypeSet to_journal,
                       ModelTypeSet to_unapply,
                       const ModelSafeRoutingInfo& new_routing_info,
                       const base::Closure& ready_task,
                       const base::Closure& retry_task) override;
  void SetInvalidatorEnabled(bool invalidator_enabled) override;
  void OnIncomingInvalidation(
      syncer::ModelType type,
      scoped_ptr<InvalidationInterface> invalidation) override;
  void AddObserver(SyncManager::Observer* observer) override;
  void RemoveObserver(SyncManager::Observer* observer) override;
  SyncStatus GetDetailedStatus() const override;
  void SaveChanges() override;
  void ShutdownOnSyncThread(ShutdownReason reason) override;
  UserShare* GetUserShare() override;
  syncer::SyncContextProxy* GetSyncContextProxy() override;
  const std::string cache_guid() override;
  bool ReceivedExperiment(Experiments* experiments) override;
  bool HasUnsyncedItems() override;
  SyncEncryptionHandler* GetEncryptionHandler() override;
  ScopedVector<syncer::ProtocolEvent> GetBufferedProtocolEvents() override;
  scoped_ptr<base::ListValue> GetAllNodesForType(
      syncer::ModelType type) override;
  void RegisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  void UnregisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  bool HasDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  void RequestEmitDebugInfo() override;

  // SyncEncryptionHandler::Observer implementation.
  void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               BootstrapTokenType type) override;
  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time explicit_passphrase_time) override;

  // SyncEngineEventListener implementation.
  void OnSyncCycleEvent(const SyncCycleEvent& event) override;
  void OnActionableError(const SyncProtocolError& error) override;
  void OnRetryTimeChanged(base::Time retry_time) override;
  void OnThrottledTypesChanged(ModelTypeSet throttled_types) override;
  void OnMigrationRequested(ModelTypeSet types) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;

  // ServerConnectionEventListener implementation.
  void OnServerConnectionEvent(const ServerConnectionEvent& event) override;

  // JsBackend implementation.
  void SetJsEventHandler(
      const WeakHandle<JsEventHandler>& event_handler) override;

  // DirectoryChangeDelegate implementation.
  // This listener is called upon completion of a syncable transaction, and
  // builds the list of sync-engine initiated changes that will be forwarded to
  // the SyncManager's Observers.
  void HandleTransactionCompleteChangeEvent(
      ModelTypeSet models_with_changes) override;
  ModelTypeSet HandleTransactionEndingChangeEvent(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) override;
  void HandleCalculateChangesChangeEventFromSyncApi(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans,
      std::vector<int64>* entries_changed) override;
  void HandleCalculateChangesChangeEventFromSyncer(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans,
      std::vector<int64>* entries_changed) override;

  // Handle explicit requests to fetch updates for the given types.
  void RefreshTypes(ModelTypeSet types) override;

  // These OnYYYChanged() methods are only called by our NetworkChangeNotifier.
  // Called when IP address of primary interface changes.
  void OnIPAddressChanged() override;
  // Called when the connection type of the system has changed.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType) override;

  // NudgeHandler implementation.
  void NudgeForInitialDownload(syncer::ModelType type) override;
  void NudgeForCommit(syncer::ModelType type) override;
  void NudgeForRefresh(syncer::ModelType type) override;

  const SyncScheduler* scheduler() const;

  bool GetHasInvalidAuthTokenForTest() const;

 protected:
  // Helper functions.  Virtual for testing.
  virtual void NotifyInitializationSuccess();
  virtual void NotifyInitializationFailure();

 private:
  friend class SyncManagerTest;
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, NudgeDelayTest);
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, PurgeDisabledTypes);
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, PurgeUnappliedTypes);

  struct NotificationInfo {
    NotificationInfo();
    ~NotificationInfo();

    int total_count;
    std::string payload;

    // Returned pointer owned by the caller.
    base::DictionaryValue* ToValue() const;
  };

  base::TimeDelta GetNudgeDelayTimeDelta(const ModelType& model_type);

  typedef std::map<ModelType, NotificationInfo> NotificationInfoMap;

  // Determine if the parents or predecessors differ between the old and new
  // versions of an entry.  Note that a node's index may change without its
  // UNIQUE_POSITION changing if its sibling nodes were changed.  To handle such
  // cases, we rely on the caller to treat a position update on any sibling as
  // updating the positions of all siblings.
  bool VisiblePositionsDiffer(
      const syncable::EntryKernelMutation& mutation) const;

  // Determine if any of the fields made visible to clients of the Sync API
  // differ between the versions of an entry stored in |a| and |b|. A return
  // value of false means that it should be OK to ignore this change.
  bool VisiblePropertiesDiffer(
      const syncable::EntryKernelMutation& mutation,
      Cryptographer* cryptographer) const;

  // Open the directory named with |username|.
  bool OpenDirectory(const std::string& username);

  // Purge those disabled types as specified by |to_purge|. |to_journal| and
  // |to_unapply| specify subsets that require special handling. |to_journal|
  // types are saved into the delete journal, while |to_unapply| have only
  // their local data deleted, while their server data is preserved.
  bool PurgeDisabledTypes(ModelTypeSet to_purge,
                          ModelTypeSet to_journal,
                          ModelTypeSet to_unapply);

  void RequestNudgeForDataTypes(
      const tracked_objects::Location& nudge_location,
      ModelTypeSet type);

  // If this is a deletion for a password, sets the legacy
  // ExtraPasswordChangeRecordData field of |buffer|. Otherwise sets
  // |buffer|'s specifics field to contain the unencrypted data.
  void SetExtraChangeRecordData(int64 id,
                                ModelType type,
                                ChangeReorderBuffer* buffer,
                                Cryptographer* cryptographer,
                                const syncable::EntryKernel& original,
                                bool existed_before,
                                bool exists_now);

  // Checks for server reachabilty and requests a nudge.
  void OnNetworkConnectivityChangedImpl();

  syncable::Directory* directory();

  base::FilePath database_path_;

  const std::string name_;

  base::ThreadChecker thread_checker_;

  // Thread-safe handle used by
  // HandleCalculateChangesChangeEventFromSyncApi(), which can be
  // called from any thread.  Valid only between between calls to
  // Init() and Shutdown().
  //
  // TODO(akalin): Ideally, we wouldn't need to store this; instead,
  // we'd have another worker class which implements
  // HandleCalculateChangesChangeEventFromSyncApi() and we'd pass it a
  // WeakHandle when we construct it.
  WeakHandle<SyncManagerImpl> weak_handle_this_;

  // We give a handle to share_ to clients of the API for use when constructing
  // any transaction type.
  UserShare share_;

  // This can be called from any thread, but only between calls to
  // OpenDirectory() and ShutdownOnSyncThread().
  WeakHandle<SyncManager::ChangeObserver> change_observer_;

  ObserverList<SyncManager::Observer> observers_;

  // The ServerConnectionManager used to abstract communication between the
  // client (the Syncer) and the sync server.
  scoped_ptr<SyncAPIServerConnectionManager> connection_manager_;

  // Maintains state that affects the way we interact with different sync types.
  // This state changes when entering or exiting a configuration cycle.
  scoped_ptr<ModelTypeRegistry> model_type_registry_;

  // The main interface for non-blocking sync types and a thread-safe wrapper.
  scoped_ptr<SyncContext> sync_context_;
  scoped_ptr<SyncContextProxy> sync_context_proxy_;

  // A container of various bits of information used by the SyncScheduler to
  // create SyncSessions.  Must outlive the SyncScheduler.
  scoped_ptr<sessions::SyncSessionContext> session_context_;

  // The scheduler that runs the Syncer. Needs to be explicitly
  // Start()ed.
  scoped_ptr<SyncScheduler> scheduler_;

  // A multi-purpose status watch object that aggregates stats from various
  // sync components.
  AllStatus allstatus_;

  // Each element of this map is a store of change records produced by
  // HandleChangeEventFromSyncer during the CALCULATE_CHANGES step. The changes
  // are grouped by model type, and are stored here in tree order to be
  // forwarded to the observer slightly later, at the TRANSACTION_ENDING step
  // by HandleTransactionEndingChangeEvent. The list is cleared after observer
  // finishes processing.
  typedef std::map<int, ImmutableChangeRecordList> ChangeRecordMap;
  ChangeRecordMap change_records_;

  SyncManager::ChangeDelegate* change_delegate_;

  // Set to true once Init has been called.
  bool initialized_;

  bool observing_network_connectivity_changes_;

  // Map used to store the notification info to be displayed in
  // about:sync page.
  NotificationInfoMap notification_info_map_;

  // These are for interacting with chrome://sync-internals.
  JsSyncManagerObserver js_sync_manager_observer_;
  JsMutationEventObserver js_mutation_event_observer_;
  JsSyncEncryptionHandlerObserver js_sync_encryption_handler_observer_;

  // This is for keeping track of client events to send to the server.
  DebugInfoEventListener debug_info_event_listener_;

  ProtocolEventBuffer protocol_event_buffer_;

  scoped_ptr<UnrecoverableErrorHandler> unrecoverable_error_handler_;
  ReportUnrecoverableErrorFunction report_unrecoverable_error_function_;

  // Sync's encryption handler. It tracks the set of encrypted types, manages
  // changing passphrases, and in general handles sync-specific interactions
  // with the cryptographer.
  scoped_ptr<SyncEncryptionHandlerImpl> sync_encryption_handler_;

  base::WeakPtrFactory<SyncManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncManagerImpl);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_SYNC_MANAGER_H_
