// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_SYNC_MANAGER_H_
#define SYNC_INTERNAL_API_SYNC_MANAGER_H_

#include <string>
#include <vector>

#include "net/base/network_change_notifier.h"
#include "sync/engine/all_status.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/engine/sync_engine_event.h"
#include "sync/engine/throttled_data_type_tracker.h"
#include "sync/engine/traffic_recorder.h"
#include "sync/internal_api/change_reorder_buffer.h"
#include "sync/internal_api/debug_info_event_listener.h"
#include "sync/internal_api/js_mutation_event_observer.h"
#include "sync/internal_api/js_sync_manager_observer.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/js/js_backend.h"
#include "sync/notifier/notifications_disabled_reason.h"
#include "sync/notifier/sync_notifier_observer.h"
#include "sync/syncable/directory_change_delegate.h"
#include "sync/util/cryptographer.h"
#include "sync/util/time.h"

namespace syncer {

class SyncAPIServerConnectionManager;
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
class SyncManagerImpl : public SyncManager,
                        public net::NetworkChangeNotifier::IPAddressObserver,
                        public Cryptographer::Observer,
                        public SyncNotifierObserver,
                        public JsBackend,
                        public SyncEngineEventListener,
                        public ServerConnectionEventListener,
                        public syncable::DirectoryChangeDelegate {
 public:
  // Create an uninitialized SyncManager.  Callers must Init() before using.
  explicit SyncManagerImpl(const std::string& name);
  virtual ~SyncManagerImpl();

  // SyncManager implementation.
  virtual void Init(
      const FilePath& database_location,
      const WeakHandle<JsEventHandler>& event_handler,
      const std::string& sync_server_and_path,
      int sync_server_port,
      bool use_ssl,
      const scoped_refptr<base::TaskRunner>& blocking_task_runner,
      scoped_ptr<HttpPostProviderFactory> post_factory,
      const std::vector<ModelSafeWorker*>& workers,
      ExtensionsActivityMonitor* extensions_activity_monitor,
      SyncManager::ChangeDelegate* change_delegate,
      const SyncCredentials& credentials,
      scoped_ptr<SyncNotifier> sync_notifier,
      const std::string& restored_key_for_bootstrapping,
      const std::string& restored_keystore_key_for_bootstrapping,
      scoped_ptr<InternalComponentsFactory> internal_components_factory,
      Encryptor* encryptor,
      UnrecoverableErrorHandler* unrecoverable_error_handler,
      ReportUnrecoverableErrorFunction
          report_unrecoverable_error_function) OVERRIDE;
  virtual void ThrowUnrecoverableError() OVERRIDE;
  virtual ModelTypeSet InitialSyncEndedTypes() OVERRIDE;
  virtual ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet types) OVERRIDE;
  virtual bool PurgePartiallySyncedTypes() OVERRIDE;
  virtual void UpdateCredentials(const SyncCredentials& credentials) OVERRIDE;
  virtual void UpdateEnabledTypes(
      const ModelTypeSet& enabled_types) OVERRIDE;
  virtual void RegisterInvalidationHandler(
      SyncNotifierObserver* handler) OVERRIDE;
  virtual void UpdateRegisteredInvalidationIds(
      SyncNotifierObserver* handler,
      const ObjectIdSet& ids) OVERRIDE;
  virtual void UnregisterInvalidationHandler(
      SyncNotifierObserver* handler) OVERRIDE;
  virtual void StartSyncingNormally(
      const ModelSafeRoutingInfo& routing_info) OVERRIDE;
  virtual void SetEncryptionPassphrase(const std::string& passphrase,
                                       bool is_explicit) OVERRIDE;
  virtual void SetDecryptionPassphrase(const std::string& passphrase) OVERRIDE;
  virtual void ConfigureSyncer(
      ConfigureReason reason,
      const ModelTypeSet& types_to_config,
      const ModelSafeRoutingInfo& new_routing_info,
      const base::Closure& ready_task,
      const base::Closure& retry_task) OVERRIDE;
  virtual void AddObserver(SyncManager::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(SyncManager::Observer* observer) OVERRIDE;
  virtual SyncStatus GetDetailedStatus() const OVERRIDE;
  virtual bool IsUsingExplicitPassphrase() OVERRIDE;
  virtual bool GetKeystoreKeyBootstrapToken(std::string* token) OVERRIDE;
  virtual void SaveChanges() OVERRIDE;
  virtual void StopSyncingForShutdown(const base::Closure& callback) OVERRIDE;
  virtual void ShutdownOnSyncThread() OVERRIDE;
  virtual UserShare* GetUserShare() OVERRIDE;

  // Update the Cryptographer from the current nigori node and write back any
  // necessary changes to the nigori node. We also detect missing encryption
  // keys and write them into the nigori node.
  // Also updates or adds the device information into the nigori node.
  // Note: opens a transaction and can trigger an ON_PASSPHRASE_REQUIRED, so
  // should only be called after syncapi is fully initialized.
  // Calls the callback argument with true if cryptographer is ready, false
  // otherwise.
  virtual void RefreshNigori(const std::string& chrome_version,
                             const base::Closure& done_callback) OVERRIDE;

  virtual void EnableEncryptEverything() OVERRIDE;
  virtual bool ReceivedExperiment(Experiments* experiments) OVERRIDE;
  virtual bool HasUnsyncedItems() OVERRIDE;

  // Return the currently active (validated) username for use with syncable
  // types.
  const std::string& username_for_share() const;

  static int GetDefaultNudgeDelay();
  static int GetPreferencesNudgeDelay();

  // SyncEngineEventListener implementation.
  virtual void OnSyncEngineEvent(const SyncEngineEvent& event) OVERRIDE;

  // ServerConnectionEventListener implementation.
  virtual void OnServerConnectionEvent(
      const ServerConnectionEvent& event) OVERRIDE;

  // JsBackend implementation.
  virtual void SetJsEventHandler(
      const WeakHandle<JsEventHandler>& event_handler) OVERRIDE;
  virtual void ProcessJsMessage(
      const std::string& name, const JsArgList& args,
      const WeakHandle<JsReplyHandler>& reply_handler) OVERRIDE;

  // DirectoryChangeDelegate implementation.
  // This listener is called upon completion of a syncable transaction, and
  // builds the list of sync-engine initiated changes that will be forwarded to
  // the SyncManager's Observers.
  virtual void HandleTransactionCompleteChangeEvent(
      ModelTypeSet models_with_changes) OVERRIDE;
  virtual ModelTypeSet HandleTransactionEndingChangeEvent(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) OVERRIDE;
  virtual void HandleCalculateChangesChangeEventFromSyncApi(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) OVERRIDE;
  virtual void HandleCalculateChangesChangeEventFromSyncer(
      const syncable::ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) OVERRIDE;

  // Cryptographer::Observer implementation.
  virtual void OnEncryptedTypesChanged(
      ModelTypeSet encrypted_types,
      bool encrypt_everything) OVERRIDE;

  // SyncNotifierObserver implementation.
  virtual void OnNotificationsEnabled() OVERRIDE;
  virtual void OnNotificationsDisabled(
      NotificationsDisabledReason reason) OVERRIDE;
  virtual void OnIncomingNotification(
      const ObjectIdPayloadMap& id_payloads,
      IncomingNotificationSource source) OVERRIDE;

  // Called only by our NetworkChangeNotifier.
  virtual void OnIPAddressChanged() OVERRIDE;

  const SyncScheduler* scheduler() const;

 private:
  friend class SyncManagerTest;
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, NudgeDelayTest);
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, OnNotificationStateChange);
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, OnIncomingNotification);
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, PurgeDisabledTypes);

  struct NotificationInfo {
    NotificationInfo();
    ~NotificationInfo();

    int total_count;
    std::string payload;

    // Returned pointer owned by the caller.
    DictionaryValue* ToValue() const;
  };

  base::TimeDelta GetNudgeDelayTimeDelta(const ModelType& model_type);

  typedef std::map<ModelType, NotificationInfo> NotificationInfoMap;
  typedef JsArgList (SyncManagerImpl::*UnboundJsMessageHandler)(
      const JsArgList&);
  typedef base::Callback<JsArgList(const JsArgList&)> JsMessageHandler;
  typedef std::map<std::string, JsMessageHandler> JsMessageHandlerMap;

  // Determine if the parents or predecessors differ between the old and new
  // versions of an entry stored in |a| and |b|.  Note that a node's index may
  // change without its NEXT_ID changing if the node at NEXT_ID also moved (but
  // the relative order is unchanged).  To handle such cases, we rely on the
  // caller to treat a position update on any sibling as updating the positions
  // of all siblings.
  bool VisiblePositionsDiffer(
      const syncable::EntryKernelMutation& mutation) const;

  // Determine if any of the fields made visible to clients of the Sync API
  // differ between the versions of an entry stored in |a| and |b|. A return
  // value of false means that it should be OK to ignore this change.
  bool VisiblePropertiesDiffer(
      const syncable::EntryKernelMutation& mutation,
      Cryptographer* cryptographer) const;

  bool ChangeBuffersAreEmpty();

  // Open the directory named with username_for_share
  bool OpenDirectory();

  // Purge those types from |previously_enabled_types| that are no longer
  // enabled in |currently_enabled_types|.
  bool PurgeDisabledTypes(ModelTypeSet previously_enabled_types,
                          ModelTypeSet currently_enabled_types);

  void RequestNudgeForDataTypes(
      const tracked_objects::Location& nudge_location,
      ModelTypeSet type);

  void NotifyCryptographerState(Cryptographer* cryptographer);

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

  // Stores the current set of encryption keys (if the cryptographer is ready)
  // and encrypted types into the nigori node.
  void UpdateNigoriEncryptionState(Cryptographer* cryptographer,
                                   WriteNode* nigori_node);

  // Internal callback of UpdateCryptographerAndNigoriCallback.
  void UpdateCryptographerAndNigoriCallback(
      const std::string& chrome_version,
      const base::Closure& done_callback,
      const std::string& session_name);

  // Updates the nigori node with any new encrypted types and then
  // encrypts the nodes for those new data types as well as other
  // nodes that should be encrypted but aren't.  Triggers
  // OnPassphraseRequired if the cryptographer isn't ready.
  void RefreshEncryption();

  void ReEncryptEverything(WriteTransaction* trans);

  // The final step of SetEncryptionPassphrase and SetDecryptionPassphrase that
  // notifies observers of the result of the set passphrase operation, updates
  // the nigori node, and does re-encryption.
  // |success|: true if the operation was successful and false otherwise. If
  //            success == false, we send an OnPassphraseRequired notification.
  // |bootstrap_token|: used to inform observers if the cryptographer's
  //                    bootstrap token was updated.
  // |is_explicit|: used to differentiate between a custom passphrase (true) and
  //                a GAIA passphrase that is implicitly used for encryption
  //                (false).
  // |trans| and |nigori_node|: used to access data in the cryptographer.
  void FinishSetPassphrase(
      bool success,
      const std::string& bootstrap_token,
      bool is_explicit,
      WriteTransaction* trans,
      WriteNode* nigori_node);

  // Called for every notification. This updates the notification statistics
  // to be displayed in about:sync.
  void UpdateNotificationInfo(
      const ModelTypePayloadMap& type_payloads);

  // Checks for server reachabilty and requests a nudge.
  void OnIPAddressChangedImpl();

  // Helper function used only by the constructor.
  void BindJsMessageHandler(
    const std::string& name, UnboundJsMessageHandler unbound_message_handler);

  // Helper function used by OnNotifications{Enabled,Disabled}().
  void OnNotificationStateChange(NotificationsDisabledReason reason);

  // Returned pointer is owned by the caller.
  static DictionaryValue* NotificationInfoToValue(
      const NotificationInfoMap& notification_info);

  static std::string NotificationInfoToString(
      const NotificationInfoMap& notification_info);

  // JS message handlers.
  JsArgList GetNotificationState(const JsArgList& args);
  JsArgList GetNotificationInfo(const JsArgList& args);
  JsArgList GetRootNodeDetails(const JsArgList& args);
  JsArgList GetAllNodes(const JsArgList& args);
  JsArgList GetNodeSummariesById(const JsArgList& args);
  JsArgList GetNodeDetailsById(const JsArgList& args);
  JsArgList GetChildNodeIds(const JsArgList& args);
  JsArgList GetClientServerTraffic(const JsArgList& args);

  syncable::Directory* directory();

  FilePath database_path_;

  const std::string name_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SyncManagerImpl> weak_ptr_factory_;

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

  // |blocking_task_runner| is a TaskRunner to be used for tasks that
  // may block on disk I/O.
  scoped_refptr<base::TaskRunner> blocking_task_runner_;

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

  // A container of various bits of information used by the SyncScheduler to
  // create SyncSessions.  Must outlive the SyncScheduler.
  scoped_ptr<sessions::SyncSessionContext> session_context_;

  // The scheduler that runs the Syncer. Needs to be explicitly
  // Start()ed.
  scoped_ptr<SyncScheduler> scheduler_;

  // The SyncNotifier which notifies us when updates need to be downloaded.
  scoped_ptr<SyncNotifier> sync_notifier_;

  // A multi-purpose status watch object that aggregates stats from various
  // sync components.
  AllStatus allstatus_;

  // Each element of this array is a store of change records produced by
  // HandleChangeEvent during the CALCULATE_CHANGES step.  The changes are
  // segregated by model type, and are stored here to be processed and
  // forwarded to the observer slightly later, at the TRANSACTION_ENDING
  // step by HandleTransactionEndingChangeEvent. The list is cleared in the
  // TRANSACTION_COMPLETE step by HandleTransactionCompleteChangeEvent.
  ChangeReorderBuffer change_buffers_[MODEL_TYPE_COUNT];

  SyncManager::ChangeDelegate* change_delegate_;

  // Set to true once Init has been called.
  bool initialized_;

  bool observing_ip_address_changes_;

  NotificationsDisabledReason notifications_disabled_reason_;

  // Map used to store the notification info to be displayed in
  // about:sync page.
  NotificationInfoMap notification_info_map_;

  // These are for interacting with chrome://sync-internals.
  JsMessageHandlerMap js_message_handlers_;
  WeakHandle<JsEventHandler> js_event_handler_;
  JsSyncManagerObserver js_sync_manager_observer_;
  JsMutationEventObserver js_mutation_event_observer_;

  ThrottledDataTypeTracker throttled_data_type_tracker_;

  // This is for keeping track of client events to send to the server.
  DebugInfoEventListener debug_info_event_listener_;

  TrafficRecorder traffic_recorder_;

  Encryptor* encryptor_;
  UnrecoverableErrorHandler* unrecoverable_error_handler_;
  ReportUnrecoverableErrorFunction report_unrecoverable_error_function_;

  // The number of times we've automatically (i.e. not via SetPassphrase or
  // conflict resolver) updated the nigori's encryption keys in this chrome
  // instantiation.
  int nigori_overwrite_count_;

  DISALLOW_COPY_AND_ASSIGN(SyncManagerImpl);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_SYNC_MANAGER_H_
