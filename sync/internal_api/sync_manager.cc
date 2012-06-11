// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_manager.h"

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/observer_list.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "net/base/network_change_notifier.h"
#include "sync/engine/net/server_connection_manager.h"
#include "sync/engine/nigori_util.h"
#include "sync/engine/sync_scheduler.h"
#include "sync/engine/syncer_types.h"
#include "sync/internal_api/all_status.h"
#include "sync/internal_api/base_node.h"
#include "sync/internal_api/change_reorder_buffer.h"
#include "sync/internal_api/configure_reason.h"
#include "sync/internal_api/debug_info_event_listener.h"
#include "sync/internal_api/js_mutation_event_observer.h"
#include "sync/internal_api/js_sync_manager_observer.h"
#include "sync/internal_api/public/engine/polling_constants.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/internal_api/public/syncable/model_type_payload_map.h"
#include "sync/internal_api/read_node.h"
#include "sync/internal_api/read_transaction.h"
#include "sync/internal_api/syncapi_internal.h"
#include "sync/internal_api/syncapi_server_connection_manager.h"
#include "sync/internal_api/user_share.h"
#include "sync/internal_api/write_node.h"
#include "sync/internal_api/write_transaction.h"
#include "sync/js/js_arg_list.h"
#include "sync/js/js_backend.h"
#include "sync/js/js_event_details.h"
#include "sync/js/js_event_handler.h"
#include "sync/js/js_reply_handler.h"
#include "sync/notifier/sync_notifier.h"
#include "sync/notifier/sync_notifier_observer.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/protocol/proto_value_conversions.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/directory_change_delegate.h"
#include "sync/syncable/syncable.h"
#include "sync/util/cryptographer.h"
#include "sync/util/experiments.h"
#include "sync/util/get_session_name.h"
#include "sync/util/time.h"

using base::TimeDelta;
using browser_sync::AllStatus;
using browser_sync::Cryptographer;
using browser_sync::Encryptor;
using browser_sync::JsArgList;
using browser_sync::JsBackend;
using browser_sync::JsEventDetails;
using browser_sync::JsEventHandler;
using browser_sync::JsEventHandler;
using browser_sync::JsReplyHandler;
using browser_sync::JsMutationEventObserver;
using browser_sync::JsSyncManagerObserver;
using browser_sync::kNigoriTag;
using browser_sync::KeyParams;
using browser_sync::ModelSafeRoutingInfo;
using browser_sync::ReportUnrecoverableErrorFunction;
using browser_sync::ServerConnectionEvent;
using browser_sync::ServerConnectionEventListener;
using browser_sync::SyncEngineEvent;
using browser_sync::SyncEngineEventListener;
using browser_sync::SyncScheduler;
using browser_sync::Syncer;
using browser_sync::UnrecoverableErrorHandler;
using browser_sync::WeakHandle;
using browser_sync::sessions::SyncSessionContext;
using syncable::ImmutableWriteTransactionInfo;
using syncable::ModelType;
using syncable::ModelTypeSet;
using syncable::SPECIFICS;
using sync_pb::GetUpdatesCallerInfo;

namespace {

// Delays for syncer nudges.
static const int kSyncRefreshDelayMsec = 500;
static const int kSyncSchedulerDelayMsec = 250;

GetUpdatesCallerInfo::GetUpdatesSource GetSourceFromReason(
    sync_api::ConfigureReason reason) {
  switch (reason) {
    case sync_api::CONFIGURE_REASON_RECONFIGURATION:
      return GetUpdatesCallerInfo::RECONFIGURATION;
    case sync_api::CONFIGURE_REASON_MIGRATION:
      return GetUpdatesCallerInfo::MIGRATION;
    case sync_api::CONFIGURE_REASON_NEW_CLIENT:
      return GetUpdatesCallerInfo::NEW_CLIENT;
    case sync_api::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE:
      return GetUpdatesCallerInfo::NEWLY_SUPPORTED_DATATYPE;
    default:
      NOTREACHED();
  }

  return GetUpdatesCallerInfo::UNKNOWN;
}

// The maximum number of times we will automatically overwrite the nigori node
// because the encryption keys don't match (per chrome instantiation).
static const int kNigoriOverwriteLimit = 10;

} // namespace

namespace sync_api {

const int SyncManager::kDefaultNudgeDelayMilliseconds = 200;
const int SyncManager::kPreferencesNudgeDelayMilliseconds = 2000;

// Maximum count and size for traffic recorder.
const unsigned int kMaxMessagesToRecord = 10;
const unsigned int kMaxMessageSizeToRecord = 5 * 1024;

//////////////////////////////////////////////////////////////////////////
// SyncManager's implementation: SyncManager::SyncInternal
class SyncManager::SyncInternal
    : public net::NetworkChangeNotifier::IPAddressObserver,
      public browser_sync::Cryptographer::Observer,
      public sync_notifier::SyncNotifierObserver,
      public JsBackend,
      public SyncEngineEventListener,
      public ServerConnectionEventListener,
      public syncable::DirectoryChangeDelegate {
 public:
  explicit SyncInternal(const std::string& name)
      : name_(name),
        weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        change_delegate_(NULL),
        initialized_(false),
        testing_mode_(NON_TEST),
        observing_ip_address_changes_(false),
        traffic_recorder_(kMaxMessagesToRecord, kMaxMessageSizeToRecord),
        encryptor_(NULL),
        unrecoverable_error_handler_(NULL),
        report_unrecoverable_error_function_(NULL),
        created_on_loop_(MessageLoop::current()),
        nigori_overwrite_count_(0) {
    // Pre-fill |notification_info_map_|.
    for (int i = syncable::FIRST_REAL_MODEL_TYPE;
         i < syncable::MODEL_TYPE_COUNT; ++i) {
      notification_info_map_.insert(
          std::make_pair(syncable::ModelTypeFromInt(i), NotificationInfo()));
    }

    // Bind message handlers.
    BindJsMessageHandler(
        "getNotificationState",
        &SyncManager::SyncInternal::GetNotificationState);
    BindJsMessageHandler(
        "getNotificationInfo",
        &SyncManager::SyncInternal::GetNotificationInfo);
    BindJsMessageHandler(
        "getRootNodeDetails",
        &SyncManager::SyncInternal::GetRootNodeDetails);
    BindJsMessageHandler(
        "getNodeSummariesById",
        &SyncManager::SyncInternal::GetNodeSummariesById);
    BindJsMessageHandler(
        "getNodeDetailsById",
        &SyncManager::SyncInternal::GetNodeDetailsById);
    BindJsMessageHandler(
        "getAllNodes",
        &SyncManager::SyncInternal::GetAllNodes);
    BindJsMessageHandler(
        "getChildNodeIds",
        &SyncManager::SyncInternal::GetChildNodeIds);
    BindJsMessageHandler(
        "getClientServerTraffic",
        &SyncManager::SyncInternal::GetClientServerTraffic);
  }

  virtual ~SyncInternal() {
    CHECK(!initialized_);
  }

  bool Init(const FilePath& database_location,
            const WeakHandle<JsEventHandler>& event_handler,
            const std::string& sync_server_and_path,
            int port,
            bool use_ssl,
            const scoped_refptr<base::TaskRunner>& blocking_task_runner,
            HttpPostProviderFactory* post_factory,
            const browser_sync::ModelSafeRoutingInfo& model_safe_routing_info,
            const std::vector<browser_sync::ModelSafeWorker*>& workers,
            browser_sync::ExtensionsActivityMonitor*
                extensions_activity_monitor,
            ChangeDelegate* change_delegate,
            const std::string& user_agent,
            const SyncCredentials& credentials,
            sync_notifier::SyncNotifier* sync_notifier,
            const std::string& restored_key_for_bootstrapping,
            TestingMode testing_mode,
            Encryptor* encryptor,
            UnrecoverableErrorHandler* unrecoverable_error_handler,
            ReportUnrecoverableErrorFunction
                report_unrecoverable_error_function);

  // Sign into sync with given credentials.
  // We do not verify the tokens given. After this call, the tokens are set
  // and the sync DB is open. True if successful, false if something
  // went wrong.
  bool SignIn(const SyncCredentials& credentials);

  // Update tokens that we're using in Sync. Email must stay the same.
  void UpdateCredentials(const SyncCredentials& credentials);

  // Called when the user disables or enables a sync type.
  void UpdateEnabledTypes(const ModelTypeSet& enabled_types);

  // Tell the sync engine to start the syncing process.
  void StartSyncingNormally(
      const browser_sync::ModelSafeRoutingInfo& routing_info);

  // Whether or not the Nigori node is encrypted using an explicit passphrase.
  bool IsUsingExplicitPassphrase();

  // Update the Cryptographer from the current nigori node and write back any
  // necessary changes to the nigori node. We also detect missing encryption
  // keys and write them into the nigori node.
  // Also updates or adds the device information into the nigori node.
  // Note: opens a transaction and can trigger an ON_PASSPHRASE_REQUIRED, so
  // should only be called after syncapi is fully initialized.
  // Calls the callback argument with true if cryptographer is ready, false
  // otherwise.
  void UpdateCryptographerAndNigori(
      const std::string& chrome_version,
      const base::Closure& done_callback);

  // Stores the current set of encryption keys (if the cryptographer is ready)
  // and encrypted types into the nigori node.
  void UpdateNigoriEncryptionState(Cryptographer* cryptographer,
                                   WriteNode* nigori_node);

  // Updates the nigori node with any new encrypted types and then
  // encrypts the nodes for those new data types as well as other
  // nodes that should be encrypted but aren't.  Triggers
  // OnPassphraseRequired if the cryptographer isn't ready.
  void RefreshEncryption();

  // Re-encrypts the encrypted data types using the passed passphrase, and sets
  // a flag in the nigori node specifying whether the current passphrase is
  // explicit (custom passphrase) or non-explicit (GAIA). If the existing
  // encryption passphrase is "explicit", the data cannot be re-encrypted and
  // SetEncryptionPassphrase will do nothing.
  // If !is_explicit and there are pending keys, we will attempt to decrypt them
  // using this passphrase. If this fails, we will save this encryption key to
  // be applied later after the pending keys are resolved.
  // Calls FinishSetPassphrase at the end, which notifies observers of the
  // result of the set passphrase operation, updates the nigori node, and does
  // re-encryption.
  void SetEncryptionPassphrase(const std::string& passphrase, bool is_explicit);

  // Provides a passphrase for decrypting the user's existing sync data. Calls
  // FinishSetPassphrase at the end, which notifies observers of the result of
  // the set passphrase operation, updates the nigori node, and does
  // re-encryption.
  void SetDecryptionPassphrase(const std::string& passphrase);

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

  // Call periodically from a database-safe thread to persist recent changes
  // to the syncapi model.
  void SaveChanges();

  // DirectoryChangeDelegate implementation.
  // This listener is called upon completion of a syncable transaction, and
  // builds the list of sync-engine initiated changes that will be forwarded to
  // the SyncManager's Observers.
  virtual void HandleTransactionCompleteChangeEvent(
      ModelTypeSet models_with_changes) OVERRIDE;
  virtual ModelTypeSet HandleTransactionEndingChangeEvent(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) OVERRIDE;
  virtual void HandleCalculateChangesChangeEventFromSyncApi(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) OVERRIDE;
  virtual void HandleCalculateChangesChangeEventFromSyncer(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) OVERRIDE;

  // Open the directory named with username_for_share
  bool OpenDirectory();

  // Cryptographer::Observer implementation.
  virtual void OnEncryptedTypesChanged(
      syncable::ModelTypeSet encrypted_types,
      bool encrypt_everything) OVERRIDE;

  // SyncNotifierObserver implementation.
  virtual void OnNotificationStateChange(
      bool notifications_enabled) OVERRIDE;

  virtual void OnIncomingNotification(
      const syncable::ModelTypePayloadMap& type_payloads,
      sync_notifier::IncomingNotificationSource source) OVERRIDE;

  void AddObserver(SyncManager::Observer* observer);
  void RemoveObserver(SyncManager::Observer* observer);

  // Accessors for the private members.
  syncable::Directory* directory() { return share_.directory.get(); }
  SyncAPIServerConnectionManager* connection_manager() {
    return connection_manager_.get();
  }
  SyncSessionContext* session_context() { return session_context_.get(); }
  SyncScheduler* scheduler() const { return scheduler_.get(); }
  UserShare* GetUserShare() {
    DCHECK(initialized_);
    return &share_;
  }

  // Return the currently active (validated) username for use with syncable
  // types.
  const std::string& username_for_share() const {
    return share_.name;
  }

  Status GetStatus();

  void RequestNudge(const tracked_objects::Location& nudge_location);

  void RequestNudgeForDataTypes(
      const tracked_objects::Location& nudge_location,
      ModelTypeSet type);

  TimeDelta GetNudgeDelayTimeDelta(const ModelType& model_type);

  void NotifyCryptographerState(Cryptographer* cryptographer);

  // See SyncManager::Shutdown* for information.
  void StopSyncingForShutdown(const base::Closure& callback);
  void ShutdownOnSyncThread();

  // If this is a deletion for a password, sets the legacy
  // ExtraPasswordChangeRecordData field of |buffer|. Otherwise sets
  // |buffer|'s specifics field to contain the unencrypted data.
  void SetExtraChangeRecordData(int64 id,
                                syncable::ModelType type,
                                ChangeReorderBuffer* buffer,
                                Cryptographer* cryptographer,
                                const syncable::EntryKernel& original,
                                bool existed_before,
                                bool exists_now);

  // Called only by our NetworkChangeNotifier.
  virtual void OnIPAddressChanged() OVERRIDE;

  syncable::ModelTypeSet InitialSyncEndedTypes() {
    DCHECK(initialized_);
    return directory()->initial_sync_ended_types();
  }

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

 private:
  struct NotificationInfo {
    int total_count;
    std::string payload;

    NotificationInfo() : total_count(0) {}

    ~NotificationInfo() {}

    // Returned pointer owned by the caller.
    DictionaryValue* ToValue() const {
      DictionaryValue* value = new DictionaryValue();
      value->SetInteger("totalCount", total_count);
      value->SetString("payload", payload);
      return value;
    }
  };

  typedef std::map<syncable::ModelType, NotificationInfo> NotificationInfoMap;
  typedef JsArgList
      (SyncManager::SyncInternal::*UnboundJsMessageHandler)(const JsArgList&);
  typedef base::Callback<JsArgList(const JsArgList&)> JsMessageHandler;
  typedef std::map<std::string, JsMessageHandler> JsMessageHandlerMap;

  // Internal callback of UpdateCryptographerAndNigoriCallback.
  void UpdateCryptographerAndNigoriCallback(
      const std::string& chrome_version,
      const base::Closure& done_callback,
      const std::string& session_name);

  // Determine if the parents or predecessors differ between the old and new
  // versions of an entry stored in |a| and |b|.  Note that a node's index may
  // change without its NEXT_ID changing if the node at NEXT_ID also moved (but
  // the relative order is unchanged).  To handle such cases, we rely on the
  // caller to treat a position update on any sibling as updating the positions
  // of all siblings.
  static bool VisiblePositionsDiffer(
      const syncable::EntryKernelMutation& mutation) {
    const syncable::EntryKernel& a = mutation.original;
    const syncable::EntryKernel& b = mutation.mutated;
    // If the datatype isn't one where the browser model cares about position,
    // don't bother notifying that data model of position-only changes.
    if (!ShouldMaintainPosition(
            syncable::GetModelTypeFromSpecifics(b.ref(SPECIFICS))))
      return false;
    if (a.ref(syncable::NEXT_ID) != b.ref(syncable::NEXT_ID))
      return true;
    if (a.ref(syncable::PARENT_ID) != b.ref(syncable::PARENT_ID))
      return true;
    return false;
  }

  // Determine if any of the fields made visible to clients of the Sync API
  // differ between the versions of an entry stored in |a| and |b|. A return
  // value of false means that it should be OK to ignore this change.
  static bool VisiblePropertiesDiffer(
      const syncable::EntryKernelMutation& mutation,
      Cryptographer* cryptographer) {
    const syncable::EntryKernel& a = mutation.original;
    const syncable::EntryKernel& b = mutation.mutated;
    const sync_pb::EntitySpecifics& a_specifics = a.ref(SPECIFICS);
    const sync_pb::EntitySpecifics& b_specifics = b.ref(SPECIFICS);
    DCHECK_EQ(syncable::GetModelTypeFromSpecifics(a_specifics),
              syncable::GetModelTypeFromSpecifics(b_specifics));
    syncable::ModelType model_type =
        syncable::GetModelTypeFromSpecifics(b_specifics);
    // Suppress updates to items that aren't tracked by any browser model.
    if (model_type < syncable::FIRST_REAL_MODEL_TYPE ||
        !a.ref(syncable::UNIQUE_SERVER_TAG).empty()) {
      return false;
    }
    if (a.ref(syncable::IS_DIR) != b.ref(syncable::IS_DIR))
      return true;
    if (!AreSpecificsEqual(cryptographer,
                           a.ref(syncable::SPECIFICS),
                           b.ref(syncable::SPECIFICS))) {
      return true;
    }
    // We only care if the name has changed if neither specifics is encrypted
    // (encrypted nodes blow away the NON_UNIQUE_NAME).
    if (!a_specifics.has_encrypted() && !b_specifics.has_encrypted() &&
        a.ref(syncable::NON_UNIQUE_NAME) != b.ref(syncable::NON_UNIQUE_NAME))
      return true;
    if (VisiblePositionsDiffer(mutation))
      return true;
    return false;
  }

  bool ChangeBuffersAreEmpty() {
    for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
      if (!change_buffers_[i].IsEmpty())
        return false;
    }
    return true;
  }

  void ReEncryptEverything(WriteTransaction* trans);

  // Called for every notification. This updates the notification statistics
  // to be displayed in about:sync.
  void UpdateNotificationInfo(
      const syncable::ModelTypePayloadMap& type_payloads);

  // Checks for server reachabilty and requests a nudge.
  void OnIPAddressChangedImpl();

  // Helper function used only by the constructor.
  void BindJsMessageHandler(
    const std::string& name, UnboundJsMessageHandler unbound_message_handler);

  // Returned pointer is owned by the caller.
  static DictionaryValue* NotificationInfoToValue(
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

  FilePath database_path_;

  const std::string name_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SyncInternal> weak_ptr_factory_;

  // Thread-safe handle used by
  // HandleCalculateChangesChangeEventFromSyncApi(), which can be
  // called from any thread.  Valid only between between calls to
  // Init() and Shutdown().
  //
  // TODO(akalin): Ideally, we wouldn't need to store this; instead,
  // we'd have another worker class which implements
  // HandleCalculateChangesChangeEventFromSyncApi() and we'd pass it a
  // WeakHandle when we construct it.
  WeakHandle<SyncInternal> weak_handle_this_;

  // |blocking_task_runner| is a TaskRunner to be used for tasks that
  // may block on disk I/O.
  scoped_refptr<base::TaskRunner> blocking_task_runner_;

  // We give a handle to share_ to clients of the API for use when constructing
  // any transaction type.
  UserShare share_;

  // This can be called from any thread, but only between calls to
  // OpenDirectory() and ShutdownOnSyncThread().
  browser_sync::WeakHandle<SyncManager::ChangeObserver> change_observer_;

  ObserverList<SyncManager::Observer> observers_;

  // The ServerConnectionManager used to abstract communication between the
  // client (the Syncer) and the sync server.
  scoped_ptr<SyncAPIServerConnectionManager> connection_manager_;

  // A container of various bits of information used by the SyncScheduler to
  // create SyncSessions.  Must outlive the SyncScheduler.
  scoped_ptr<SyncSessionContext> session_context_;

  // The scheduler that runs the Syncer. Needs to be explicitly
  // Start()ed.
  scoped_ptr<SyncScheduler> scheduler_;

  // The SyncNotifier which notifies us when updates need to be downloaded.
  scoped_ptr<sync_notifier::SyncNotifier> sync_notifier_;

  // A multi-purpose status watch object that aggregates stats from various
  // sync components.
  AllStatus allstatus_;

  // Each element of this array is a store of change records produced by
  // HandleChangeEvent during the CALCULATE_CHANGES step.  The changes are
  // segregated by model type, and are stored here to be processed and
  // forwarded to the observer slightly later, at the TRANSACTION_ENDING
  // step by HandleTransactionEndingChangeEvent. The list is cleared in the
  // TRANSACTION_COMPLETE step by HandleTransactionCompleteChangeEvent.
  ChangeReorderBuffer change_buffers_[syncable::MODEL_TYPE_COUNT];

  SyncManager::ChangeDelegate* change_delegate_;

  // Set to true once Init has been called.
  bool initialized_;

  // Controls the disabling of certain SyncManager features.
  // Can be used to disable communication with the server and the use of an
  // on-disk file for maintaining syncer state.
  // TODO(117836): Clean up implementation of SyncManager unit tests.
  TestingMode testing_mode_;

  bool observing_ip_address_changes_;

  // Map used to store the notification info to be displayed in
  // about:sync page.
  NotificationInfoMap notification_info_map_;

  // These are for interacting with chrome://sync-internals.
  JsMessageHandlerMap js_message_handlers_;
  WeakHandle<JsEventHandler> js_event_handler_;
  JsSyncManagerObserver js_sync_manager_observer_;
  JsMutationEventObserver js_mutation_event_observer_;

  // This is for keeping track of client events to send to the server.
  DebugInfoEventListener debug_info_event_listener_;

  browser_sync::TrafficRecorder traffic_recorder_;

  Encryptor* encryptor_;
  UnrecoverableErrorHandler* unrecoverable_error_handler_;
  ReportUnrecoverableErrorFunction report_unrecoverable_error_function_;

  MessageLoop* const created_on_loop_;

  // The number of times we've automatically (i.e. not via SetPassphrase or
  // conflict resolver) updated the nigori's encryption keys in this chrome
  // instantiation.
  int nigori_overwrite_count_;
};

// A class to calculate nudge delays for types.
class NudgeStrategy {
 public:
  static TimeDelta GetNudgeDelayTimeDelta(const ModelType& model_type,
                                          SyncManager::SyncInternal* core) {
    NudgeDelayStrategy delay_type = GetNudgeDelayStrategy(model_type);
    return GetNudgeDelayTimeDeltaFromType(delay_type,
                                          model_type,
                                          core);
  }

 private:
  // Possible types of nudge delay for datatypes.
  // Note: These are just hints. If a sync happens then all dirty entries
  // would be committed as part of the sync.
  enum NudgeDelayStrategy {
    // Sync right away.
    IMMEDIATE,

    // Sync this change while syncing another change.
    ACCOMPANY_ONLY,

    // The datatype does not use one of the predefined wait times but defines
    // its own wait time logic for nudge.
    CUSTOM,
  };

  static NudgeDelayStrategy GetNudgeDelayStrategy(const ModelType& type) {
    switch (type) {
     case syncable::AUTOFILL:
       return ACCOMPANY_ONLY;
     case syncable::PREFERENCES:
     case syncable::SESSIONS:
       return CUSTOM;
     default:
       return IMMEDIATE;
    }
  }

  static TimeDelta GetNudgeDelayTimeDeltaFromType(
      const NudgeDelayStrategy& delay_type, const ModelType& model_type,
      const SyncManager::SyncInternal* core) {
    CHECK(core);
    TimeDelta delay = TimeDelta::FromMilliseconds(
       SyncManager::kDefaultNudgeDelayMilliseconds);
    switch (delay_type) {
     case IMMEDIATE:
       delay = TimeDelta::FromMilliseconds(
           SyncManager::kDefaultNudgeDelayMilliseconds);
       break;
     case ACCOMPANY_ONLY:
       delay = TimeDelta::FromSeconds(
           browser_sync::kDefaultShortPollIntervalSeconds);
       break;
     case CUSTOM:
       switch (model_type) {
         case syncable::PREFERENCES:
           delay = TimeDelta::FromMilliseconds(
               SyncManager::kPreferencesNudgeDelayMilliseconds);
           break;
         case syncable::SESSIONS:
           delay = core->scheduler()->sessions_commit_delay();
           break;
         default:
           NOTREACHED();
       }
       break;
     default:
       NOTREACHED();
    }
    return delay;
  }
};

SyncManager::ChangeDelegate::~ChangeDelegate() {}

SyncManager::ChangeObserver::~ChangeObserver() {}

SyncManager::Observer::~Observer() {}

SyncManager::SyncManager(const std::string& name)
    : data_(new SyncInternal(name)) {}

SyncManager::Status::Status()
    : notifications_enabled(false),
      notifications_received(0),
      encryption_conflicts(0),
      hierarchy_conflicts(0),
      simple_conflicts(0),
      server_conflicts(0),
      committed_count(0),
      syncing(false),
      initial_sync_ended(false),
      updates_available(0),
      updates_received(0),
      reflected_updates_received(0),
      tombstone_updates_received(0),
      num_commits_total(0),
      num_local_overwrites_total(0),
      num_server_overwrites_total(0),
      nonempty_get_updates(0),
      empty_get_updates(0),
      sync_cycles_with_commits(0),
      sync_cycles_without_commits(0),
      useless_sync_cycles(0),
      useful_sync_cycles(0),
      cryptographer_ready(false),
      crypto_has_pending_keys(false) {
}

SyncManager::Status::~Status() {
}

bool SyncManager::Init(
    const FilePath& database_location,
    const WeakHandle<JsEventHandler>& event_handler,
    const std::string& sync_server_and_path,
    int sync_server_port,
    bool use_ssl,
    const scoped_refptr<base::TaskRunner>& blocking_task_runner,
    HttpPostProviderFactory* post_factory,
    const browser_sync::ModelSafeRoutingInfo& model_safe_routing_info,
    const std::vector<browser_sync::ModelSafeWorker*>& workers,
    browser_sync::ExtensionsActivityMonitor* extensions_activity_monitor,
    ChangeDelegate* change_delegate,
    const std::string& user_agent,
    const SyncCredentials& credentials,
    sync_notifier::SyncNotifier* sync_notifier,
    const std::string& restored_key_for_bootstrapping,
    TestingMode testing_mode,
    Encryptor* encryptor,
    UnrecoverableErrorHandler* unrecoverable_error_handler,
    ReportUnrecoverableErrorFunction report_unrecoverable_error_function) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(post_factory);
  DVLOG(1) << "SyncManager starting Init...";
  std::string server_string(sync_server_and_path);
  return data_->Init(database_location,
                     event_handler,
                     server_string,
                     sync_server_port,
                     use_ssl,
                     blocking_task_runner,
                     post_factory,
                     model_safe_routing_info,
                     workers,
                     extensions_activity_monitor,
                     change_delegate,
                     user_agent,
                     credentials,
                     sync_notifier,
                     restored_key_for_bootstrapping,
                     testing_mode,
                     encryptor,
                     unrecoverable_error_handler,
                     report_unrecoverable_error_function);
}

void SyncManager::UpdateCredentials(const SyncCredentials& credentials) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->UpdateCredentials(credentials);
}

void SyncManager::UpdateEnabledTypes(const ModelTypeSet& enabled_types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->UpdateEnabledTypes(enabled_types);
}

void SyncManager::ThrowUnrecoverableError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ReadTransaction trans(FROM_HERE, GetUserShare());
  trans.GetWrappedTrans()->OnUnrecoverableError(
      FROM_HERE, "Simulating unrecoverable error for testing purposes.");
}

syncable::ModelTypeSet SyncManager::InitialSyncEndedTypes() {
  return data_->InitialSyncEndedTypes();
}

void SyncManager::StartSyncingNormally(
    const browser_sync::ModelSafeRoutingInfo& routing_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->StartSyncingNormally(routing_info);
}

void SyncManager::SetEncryptionPassphrase(const std::string& passphrase,
                                          bool is_explicit) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->SetEncryptionPassphrase(passphrase, is_explicit);
}

void SyncManager::SetDecryptionPassphrase(const std::string& passphrase) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->SetDecryptionPassphrase(passphrase);
}

void SyncManager::EnableEncryptEverything() {
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    // Update the cryptographer to know we're now encrypting everything.
    WriteTransaction trans(FROM_HERE, GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    // Only set encrypt everything if we know we can encrypt. This allows the
    // user to cancel encryption if they have forgotten their passphrase.
    if (cryptographer->is_ready())
      cryptographer->set_encrypt_everything();
  }

  // Reads from cryptographer so will automatically encrypt all
  // datatypes and update the nigori node as necessary. Will trigger
  // OnPassphraseRequired if necessary.
  data_->RefreshEncryption();
}

bool SyncManager::EncryptEverythingEnabledForTest() const {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  return trans.GetCryptographer()->encrypt_everything();
}

bool SyncManager::IsUsingExplicitPassphrase() {
  return data_ && data_->IsUsingExplicitPassphrase();
}

void SyncManager::RequestCleanupDisabledTypes(
    const browser_sync::ModelSafeRoutingInfo& routing_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (data_->scheduler()) {
    data_->session_context()->set_routing_info(routing_info);
    data_->scheduler()->CleanupDisabledTypes();
  }
}

void SyncManager::RequestClearServerData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (data_->scheduler())
    data_->scheduler()->ClearUserData();
}

void SyncManager::RequestConfig(
    const browser_sync::ModelSafeRoutingInfo& routing_info,
    const ModelTypeSet& types, ConfigureReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!data_->scheduler()) {
    LOG(INFO)
        << "SyncManager::RequestConfig: bailing out because scheduler is "
        << "null";
    return;
  }
  StartConfigurationMode(base::Closure());
  data_->session_context()->set_routing_info(routing_info);
  data_->scheduler()->ScheduleConfiguration(types, GetSourceFromReason(reason));
}

void SyncManager::StartConfigurationMode(const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!data_->scheduler()) {
    LOG(INFO)
        << "SyncManager::StartConfigurationMode: could not start "
        << "configuration mode because because scheduler is null";
    return;
  }
  data_->scheduler()->Start(
      browser_sync::SyncScheduler::CONFIGURATION_MODE, callback);
}

bool SyncManager::SyncInternal::Init(
    const FilePath& database_location,
    const WeakHandle<JsEventHandler>& event_handler,
    const std::string& sync_server_and_path,
    int port,
    bool use_ssl,
    const scoped_refptr<base::TaskRunner>& blocking_task_runner,
    HttpPostProviderFactory* post_factory,
    const browser_sync::ModelSafeRoutingInfo& model_safe_routing_info,
    const std::vector<browser_sync::ModelSafeWorker*>& workers,
    browser_sync::ExtensionsActivityMonitor* extensions_activity_monitor,
    ChangeDelegate* change_delegate,
    const std::string& user_agent,
    const SyncCredentials& credentials,
    sync_notifier::SyncNotifier* sync_notifier,
    const std::string& restored_key_for_bootstrapping,
    TestingMode testing_mode,
    Encryptor* encryptor,
    UnrecoverableErrorHandler* unrecoverable_error_handler,
    ReportUnrecoverableErrorFunction report_unrecoverable_error_function) {
  CHECK(!initialized_);

  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(1) << "Starting SyncInternal initialization.";

  weak_handle_this_ = MakeWeakHandle(weak_ptr_factory_.GetWeakPtr());

  blocking_task_runner_ = blocking_task_runner;

  change_delegate_ = change_delegate;
  testing_mode_ = testing_mode;

  sync_notifier_.reset(sync_notifier);

  AddObserver(&js_sync_manager_observer_);
  SetJsEventHandler(event_handler);

  AddObserver(&debug_info_event_listener_);

  database_path_ = database_location.Append(
      syncable::Directory::kSyncDatabaseFilename);
  encryptor_ = encryptor;
  unrecoverable_error_handler_ = unrecoverable_error_handler;
  report_unrecoverable_error_function_ = report_unrecoverable_error_function;
  share_.directory.reset(
      new syncable::Directory(encryptor_,
                              unrecoverable_error_handler_,
                              report_unrecoverable_error_function_));

  connection_manager_.reset(new SyncAPIServerConnectionManager(
      sync_server_and_path, port, use_ssl, user_agent, post_factory));

  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  observing_ip_address_changes_ = true;

  connection_manager()->AddListener(this);

  // Test mode does not use a syncer context or syncer thread.
  if (testing_mode_ == NON_TEST) {
    // Build a SyncSessionContext and store the worker in it.
    DVLOG(1) << "Sync is bringing up SyncSessionContext.";
    std::vector<SyncEngineEventListener*> listeners;
    listeners.push_back(&allstatus_);
    listeners.push_back(this);
    session_context_.reset(new SyncSessionContext(
        connection_manager_.get(),
        directory(),
        model_safe_routing_info,
        workers,
        extensions_activity_monitor,
        listeners,
        &debug_info_event_listener_,
        &traffic_recorder_));
    session_context()->set_account_name(credentials.email);
    scheduler_.reset(new SyncScheduler(name_, session_context(), new Syncer()));
  }

  bool signed_in = SignIn(credentials);

  if (signed_in) {
    if (scheduler()) {
      scheduler()->Start(
          browser_sync::SyncScheduler::CONFIGURATION_MODE, base::Closure());
    }

    initialized_ = true;

    // Cryptographer should only be accessed while holding a
    // transaction.  Grabbing the user share for the transaction
    // checks the initialization state, so this must come after
    // |initialized_| is set to true.
    ReadTransaction trans(FROM_HERE, GetUserShare());
    trans.GetCryptographer()->Bootstrap(restored_key_for_bootstrapping);
    trans.GetCryptographer()->AddObserver(this);
  }

  // Notify that initialization is complete. Note: This should be the last to
  // execute if |signed_in| is false. Reason being in that case we would
  // post a task to shutdown sync. But if this function posts any other tasks
  // on the UI thread and if shutdown wins then that tasks would execute on
  // a freed pointer. This is because UI thread is not shut down.
  FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                    OnInitializationComplete(
                        MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()),
                        signed_in));

  if (!signed_in && testing_mode_ == NON_TEST)
    return false;

  sync_notifier_->AddObserver(this);

  return signed_in;
}

void SyncManager::SyncInternal::UpdateCryptographerAndNigori(
    const std::string& chrome_version,
    const base::Closure& done_callback) {
  DCHECK(initialized_);
  browser_sync::GetSessionName(
      blocking_task_runner_,
      base::Bind(
          &SyncManager::SyncInternal::UpdateCryptographerAndNigoriCallback,
          weak_ptr_factory_.GetWeakPtr(),
          chrome_version,
          done_callback));
}

void SyncManager::SyncInternal::UpdateNigoriEncryptionState(
    Cryptographer* cryptographer,
    WriteNode* nigori_node) {
  DCHECK(nigori_node);
  sync_pb::NigoriSpecifics nigori = nigori_node->GetNigoriSpecifics();

  if (cryptographer->is_ready() &&
      nigori_overwrite_count_ < kNigoriOverwriteLimit) {
    // Does not modify the encrypted blob if the unencrypted data already
    // matches what is about to be written.
    sync_pb::EncryptedData original_keys = nigori.encrypted();
    if (!cryptographer->GetKeys(nigori.mutable_encrypted()))
      NOTREACHED();

    if (nigori.encrypted().SerializeAsString() !=
        original_keys.SerializeAsString()) {
      // We've updated the nigori node's encryption keys. In order to prevent
      // a possible looping of two clients constantly overwriting each other,
      // we limit the absolute number of overwrites per client instantiation.
      nigori_overwrite_count_++;
      UMA_HISTOGRAM_COUNTS("Sync.AutoNigoriOverwrites",
                           nigori_overwrite_count_);
    }

    // Note: we don't try to set using_explicit_passphrase here since if that
    // is lost the user can always set it again. The main point is to preserve
    // the encryption keys so all data remains decryptable.
  }
  cryptographer->UpdateNigoriFromEncryptedTypes(&nigori);

  // If nothing has changed, this is a no-op.
  nigori_node->SetNigoriSpecifics(nigori);
}

void SyncManager::SyncInternal::UpdateCryptographerAndNigoriCallback(
    const std::string& chrome_version,
    const base::Closure& done_callback,
    const std::string& session_name) {
  if (!directory()->initial_sync_ended_for_type(syncable::NIGORI)) {
    done_callback.Run();  // Should only happen during first time sync.
    return;
  }

  bool success = false;
  {
    WriteTransaction trans(FROM_HERE, GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    WriteNode node(&trans);

    if (node.InitByTagLookup(kNigoriTag) == sync_api::BaseNode::INIT_OK) {
      sync_pb::NigoriSpecifics nigori(node.GetNigoriSpecifics());
      Cryptographer::UpdateResult result = cryptographer->Update(nigori);
      if (result == Cryptographer::NEEDS_PASSPHRASE) {
        sync_pb::EncryptedData pending_keys;
        if (cryptographer->has_pending_keys())
          pending_keys = cryptographer->GetPendingKeys();
        FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                          OnPassphraseRequired(sync_api::REASON_DECRYPTION,
                                               pending_keys));
      }


      // Add or update device information.
      bool contains_this_device = false;
      for (int i = 0; i < nigori.device_information_size(); ++i) {
        const sync_pb::DeviceInformation& device_information =
            nigori.device_information(i);
        if (device_information.cache_guid() == directory()->cache_guid()) {
          // Update the version number in case it changed due to an update.
          if (device_information.chrome_version() != chrome_version) {
            sync_pb::DeviceInformation* mutable_device_information =
                nigori.mutable_device_information(i);
            mutable_device_information->set_chrome_version(
                chrome_version);
          }
          contains_this_device = true;
        }
      }

      if (!contains_this_device) {
        sync_pb::DeviceInformation* device_information =
            nigori.add_device_information();
        device_information->set_cache_guid(directory()->cache_guid());
#if defined(OS_CHROMEOS)
        device_information->set_platform("ChromeOS");
#elif defined(OS_LINUX)
        device_information->set_platform("Linux");
#elif defined(OS_MACOSX)
        device_information->set_platform("Mac");
#elif defined(OS_WIN)
        device_information->set_platform("Windows");
#endif
        device_information->set_name(session_name);
        device_information->set_chrome_version(chrome_version);
      }
      // Disabled to avoid nigori races. TODO(zea): re-enable. crbug.com/122837
      // node.SetNigoriSpecifics(nigori);

      // Make sure the nigori node has the up to date encryption info.
      UpdateNigoriEncryptionState(cryptographer, &node);

      NotifyCryptographerState(cryptographer);
      allstatus_.SetEncryptedTypes(cryptographer->GetEncryptedTypes());

      success = cryptographer->is_ready();
    } else {
      NOTREACHED();
    }
  }

  if (success)
    RefreshEncryption();
  done_callback.Run();
}

void SyncManager::SyncInternal::NotifyCryptographerState(
    Cryptographer * cryptographer) {
  // TODO(lipalani): Explore the possibility of hooking this up to
  // SyncManager::Observer and making |AllStatus| a listener for that.
  allstatus_.SetCryptographerReady(cryptographer->is_ready());
  allstatus_.SetCryptoHasPendingKeys(cryptographer->has_pending_keys());
  debug_info_event_listener_.SetCryptographerReady(cryptographer->is_ready());
  debug_info_event_listener_.SetCrytographerHasPendingKeys(
      cryptographer->has_pending_keys());
}

void SyncManager::SyncInternal::StartSyncingNormally(
    const browser_sync::ModelSafeRoutingInfo& routing_info) {
  // Start the sync scheduler.
  if (scheduler()) { // NULL during certain unittests.
    session_context()->set_routing_info(routing_info);
    scheduler()->Start(SyncScheduler::NORMAL_MODE, base::Closure());
  }
}

bool SyncManager::SyncInternal::OpenDirectory() {
  DCHECK(!initialized_) << "Should only happen once";

  // Set before Open().
  change_observer_ =
      browser_sync::MakeWeakHandle(js_mutation_event_observer_.AsWeakPtr());
  WeakHandle<syncable::TransactionObserver> transaction_observer(
      browser_sync::MakeWeakHandle(js_mutation_event_observer_.AsWeakPtr()));

  syncable::DirOpenResult open_result = syncable::NOT_INITIALIZED;
  if (testing_mode_ == TEST_IN_MEMORY) {
    open_result = directory()->OpenInMemoryForTest(
        username_for_share(), this, transaction_observer);
  } else {
    open_result = directory()->Open(
        database_path_, username_for_share(), this, transaction_observer);
  }
  if (open_result != syncable::OPENED) {
    LOG(ERROR) << "Could not open share for:" << username_for_share();
    return false;
  }

  connection_manager()->set_client_id(directory()->cache_guid());
  return true;
}

bool SyncManager::SyncInternal::SignIn(const SyncCredentials& credentials) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(share_.name.empty());
  share_.name = credentials.email;

  DVLOG(1) << "Signing in user: " << username_for_share();
  if (!OpenDirectory())
    return false;

  // Retrieve and set the sync notifier state. This should be done
  // only after OpenDirectory is called.
  std::string unique_id = directory()->cache_guid();
  std::string state = directory()->GetNotificationState();
  DVLOG(1) << "Read notification unique ID: " << unique_id;
  if (VLOG_IS_ON(1)) {
    std::string encoded_state;
    base::Base64Encode(state, &encoded_state);
    DVLOG(1) << "Read notification state: " << encoded_state;
  }
  allstatus_.SetUniqueId(unique_id);
  sync_notifier_->SetUniqueId(unique_id);
  // TODO(tim): Remove once invalidation state has been migrated to new
  // InvalidationStateTracker store. Bug 124140.
  sync_notifier_->SetStateDeprecated(state);

  UpdateCredentials(credentials);
  return true;
}

void SyncManager::SyncInternal::UpdateCredentials(
    const SyncCredentials& credentials) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(credentials.email, share_.name);
  DCHECK(!credentials.email.empty());
  DCHECK(!credentials.sync_token.empty());

  observing_ip_address_changes_ = true;
  if (connection_manager()->set_auth_token(credentials.sync_token)) {
    sync_notifier_->UpdateCredentials(
        credentials.email, credentials.sync_token);
    if (testing_mode_ == NON_TEST && initialized_) {
      if (scheduler())
        scheduler()->OnCredentialsUpdated();
    }
  }
}

void SyncManager::SyncInternal::UpdateEnabledTypes(
    const ModelTypeSet& enabled_types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_notifier_->UpdateEnabledTypes(enabled_types);
}

void SyncManager::SyncInternal::SetEncryptionPassphrase(
    const std::string& passphrase,
    bool is_explicit) {
  // We do not accept empty passphrases.
  if (passphrase.empty()) {
    NOTREACHED() << "Cannot encrypt with an empty passphrase.";
    return;
  }

  // All accesses to the cryptographer are protected by a transaction.
  WriteTransaction trans(FROM_HERE, GetUserShare());
  Cryptographer* cryptographer = trans.GetCryptographer();
  KeyParams key_params = {"localhost", "dummy", passphrase};
  WriteNode node(&trans);
  if (node.InitByTagLookup(kNigoriTag) != sync_api::BaseNode::INIT_OK) {
    // TODO(albertb): Plumb an UnrecoverableError all the way back to the PSS.
    NOTREACHED();
    return;
  }

  bool nigori_has_explicit_passphrase =
      node.GetNigoriSpecifics().using_explicit_passphrase();
  std::string bootstrap_token;
  sync_pb::EncryptedData pending_keys;
  if (cryptographer->has_pending_keys())
    pending_keys = cryptographer->GetPendingKeys();
  bool success = false;


  // There are six cases to handle here:
  // 1. The user has no pending keys and is setting their current GAIA password
  //    as the encryption passphrase. This happens either during first time sync
  //    with a clean profile, or after re-authenticating on a profile that was
  //    already signed in with the cryptographer ready.
  // 2. The user has no pending keys, and is overwriting an (already provided)
  //    implicit passphrase with an explicit (custom) passphrase.
  // 3. The user has pending keys for an explicit passphrase that is somehow set
  //    to their current GAIA passphrase.
  // 4. The user has pending keys encrypted with their current GAIA passphrase
  //    and the caller passes in the current GAIA passphrase.
  // 5. The user has pending keys encrypted with an older GAIA passphrase
  //    and the caller passes in the current GAIA passphrase.
  // 6. The user has previously done encryption with an explicit passphrase.
  // Furthermore, we enforce the fact that the bootstrap encryption token will
  // always be derived from the newest GAIA password if the account is using
  // an implicit passphrase (even if the data is encrypted with an old GAIA
  // password). If the account is using an explicit (custom) passphrase, the
  // bootstrap token will be derived from the most recently provided explicit
  // passphrase (that was able to decrypt the data).
  if (!nigori_has_explicit_passphrase) {
    if (!cryptographer->has_pending_keys()) {
      if (cryptographer->AddKey(key_params)) {
        // Case 1 and 2. We set a new GAIA passphrase when there are no pending
        // keys (1), or overwriting an implicit passphrase with a new explicit
        // one (2) when there are no pending keys.
        DVLOG(1) << "Setting " << (is_explicit ? "explicit" : "implicit" )
                 << " passphrase for encryption.";
        cryptographer->GetBootstrapToken(&bootstrap_token);
        success = true;
      } else {
        NOTREACHED() << "Failed to add key to cryptographer.";
        success = false;
      }
    } else {  // cryptographer->has_pending_keys() == true
      if (is_explicit) {
        // This can only happen if the nigori node is updated with a new
        // implicit passphrase while a client is attempting to set a new custom
        // passphrase (race condition).
        DVLOG(1) << "Failing because an implicit passphrase is already set.";
        success = false;
      } else {  // is_explicit == false
        if (cryptographer->DecryptPendingKeys(key_params)) {
          // Case 4. We successfully decrypted with the implicit GAIA passphrase
          // passed in.
          DVLOG(1) << "Implicit internal passphrase accepted for decryption.";
          cryptographer->GetBootstrapToken(&bootstrap_token);
          success = true;
        } else {
          // Case 5. Encryption was done with an old GAIA password, but we were
          // provided with the current GAIA password. We need to generate a new
          // bootstrap token to preserve it. We build a temporary cryptographer
          // to allow us to extract these params without polluting our current
          // cryptographer.
          DVLOG(1) << "Implicit internal passphrase failed to decrypt, adding "
                   << "anyways as default passphrase and persisting via "
                   << "bootstrap token.";
          Cryptographer temp_cryptographer(encryptor_);
          temp_cryptographer.AddKey(key_params);
          temp_cryptographer.GetBootstrapToken(&bootstrap_token);
          // We then set the new passphrase as the default passphrase of the
          // real cryptographer, even though we have pending keys. This is safe,
          // as although Cryptographer::is_initialized() will now be true,
          // is_ready() will remain false due to having pending keys.
          cryptographer->AddKey(key_params);
          success = false;
        }
      }  // is_explicit
    }  // cryptographer->has_pending_keys()
  } else {  // nigori_has_explicit_passphrase == true
    // Case 6. We do not want to override a previously set explicit passphrase,
    // so we return a failure.
    DVLOG(1) << "Failing because an explicit passphrase is already set.";
    success = false;
  }

  DVLOG_IF(1, !success)
      << "Failure in SetEncryptionPassphrase; notifying and returning.";
  DVLOG_IF(1, success)
      << "Successfully set encryption passphrase; updating nigori and "
         "reencrypting.";

  FinishSetPassphrase(
      success, bootstrap_token, is_explicit, &trans, &node);
}

void SyncManager::SyncInternal::SetDecryptionPassphrase(
    const std::string& passphrase) {
  // We do not accept empty passphrases.
  if (passphrase.empty()) {
    NOTREACHED() << "Cannot decrypt with an empty passphrase.";
    return;
  }

  // All accesses to the cryptographer are protected by a transaction.
  WriteTransaction trans(FROM_HERE, GetUserShare());
  Cryptographer* cryptographer = trans.GetCryptographer();
  KeyParams key_params = {"localhost", "dummy", passphrase};
  WriteNode node(&trans);
  if (node.InitByTagLookup(kNigoriTag) != sync_api::BaseNode::INIT_OK) {
    // TODO(albertb): Plumb an UnrecoverableError all the way back to the PSS.
    NOTREACHED();
    return;
  }

  if (!cryptographer->has_pending_keys()) {
    // Note that this *can* happen in a rare situation where data is
    // re-encrypted on another client while a SetDecryptionPassphrase() call is
    // in-flight on this client. It is rare enough that we choose to do nothing.
    NOTREACHED() << "Attempt to set decryption passphrase failed because there "
                 << "were no pending keys.";
    return;
  }

  bool nigori_has_explicit_passphrase =
      node.GetNigoriSpecifics().using_explicit_passphrase();
  std::string bootstrap_token;
  sync_pb::EncryptedData pending_keys;
  pending_keys = cryptographer->GetPendingKeys();
  bool success = false;

  // There are three cases to handle here:
  // 7. We're using the current GAIA password to decrypt the pending keys. This
  //    happens when signing in to an account with a previously set implicit
  //    passphrase, where the data is already encrypted with the newest GAIA
  //    password.
  // 8. The user is providing an old GAIA password to decrypt the pending keys.
  //    In this case, the user is using an implicit passphrase, but has changed
  //    their password since they last encrypted their data, and therefore
  //    their current GAIA password was unable to decrypt the data. This will
  //    happen when the user is setting up a new profile with a previously
  //    encrypted account (after changing passwords).
  // 9. The user is providing a previously set explicit passphrase to decrypt
  //    the pending keys.
  if (!nigori_has_explicit_passphrase) {
    if (cryptographer->is_initialized()) {
      // We only want to change the default encryption key to the pending
      // one if the pending keybag already contains the current default.
      // This covers the case where a different client re-encrypted
      // everything with a newer gaia passphrase (and hence the keybag
      // contains keys from all previously used gaia passphrases).
      // Otherwise, we're in a situation where the pending keys are
      // encrypted with an old gaia passphrase, while the default is the
      // current gaia passphrase. In that case, we preserve the default.
      Cryptographer temp_cryptographer(encryptor_);
      temp_cryptographer.SetPendingKeys(cryptographer->GetPendingKeys());
      if (temp_cryptographer.DecryptPendingKeys(key_params)) {
        // Check to see if the pending bag of keys contains the current
        // default key.
        sync_pb::EncryptedData encrypted;
        cryptographer->GetKeys(&encrypted);
        if (temp_cryptographer.CanDecrypt(encrypted)) {
          DVLOG(1) << "Implicit user provided passphrase accepted for "
                   << "decryption, overwriting default.";
          // Case 7. The pending keybag contains the current default. Go ahead
          // and update the cryptographer, letting the default change.
          cryptographer->DecryptPendingKeys(key_params);
          cryptographer->GetBootstrapToken(&bootstrap_token);
          success = true;
        } else {
          // Case 8. The pending keybag does not contain the current default
          // encryption key. We decrypt the pending keys here, and in
          // FinishSetPassphrase, re-encrypt everything with the current GAIA
          // passphrase instead of the passphrase just provided by the user.
          DVLOG(1) << "Implicit user provided passphrase accepted for "
                   << "decryption, restoring implicit internal passphrase "
                   << "as default.";
          std::string bootstrap_token_from_current_key;
          cryptographer->GetBootstrapToken(
              &bootstrap_token_from_current_key);
          cryptographer->DecryptPendingKeys(key_params);
          // Overwrite the default from the pending keys.
          cryptographer->AddKeyFromBootstrapToken(
              bootstrap_token_from_current_key);
          success = true;
        }
      } else {  // !temp_cryptographer.DecryptPendingKeys(..)
        DVLOG(1) << "Implicit user provided passphrase failed to decrypt.";
        success = false;
      }  // temp_cryptographer.DecryptPendingKeys(...)
    } else {  // cryptographer->is_initialized() == false
      if (cryptographer->DecryptPendingKeys(key_params)) {
        // This can happpen in two cases:
        // - First time sync on android, where we'll never have a
        //   !user_provided passphrase.
        // - This is a restart for a client that lost their bootstrap token.
        // In both cases, we should go ahead and initialize the cryptographer
        // and persist the new bootstrap token.
        //
        // Note: at this point, we cannot distinguish between cases 7 and 8
        // above. This user provided passphrase could be the current or the
        // old. But, as long as we persist the token, there's nothing more
        // we can do.
        cryptographer->GetBootstrapToken(&bootstrap_token);
        DVLOG(1) << "Implicit user provided passphrase accepted, initializing"
                 << " cryptographer.";
        success = true;
      } else {
        DVLOG(1) << "Implicit user provided passphrase failed to decrypt.";
        success = false;
      }
    }  // cryptographer->is_initialized()
  } else {  // nigori_has_explicit_passphrase == true
    // Case 9. Encryption was done with an explicit passphrase, and we decrypt
    // with the passphrase provided by the user.
    if (cryptographer->DecryptPendingKeys(key_params)) {
      DVLOG(1) << "Explicit passphrase accepted for decryption.";
      cryptographer->GetBootstrapToken(&bootstrap_token);
      success = true;
    } else {
      DVLOG(1) << "Explicit passphrase failed to decrypt.";
      success = false;
    }
  }  // nigori_has_explicit_passphrase

  DVLOG_IF(1, !success)
      << "Failure in SetDecryptionPassphrase; notifying and returning.";
  DVLOG_IF(1, success)
      << "Successfully set decryption passphrase; updating nigori and "
         "reencrypting.";

  FinishSetPassphrase(success,
                      bootstrap_token,
                      nigori_has_explicit_passphrase,
                      &trans,
                      &node);
}

void SyncManager::SyncInternal::FinishSetPassphrase(
    bool success,
    const std::string& bootstrap_token,
    bool is_explicit,
    WriteTransaction* trans,
    WriteNode* nigori_node) {
  Cryptographer* cryptographer = trans->GetCryptographer();
  NotifyCryptographerState(cryptographer);

  // It's possible we need to change the bootstrap token even if we failed to
  // set the passphrase (for example if we need to preserve the new GAIA
  // passphrase).
  if (!bootstrap_token.empty()) {
    DVLOG(1) << "Bootstrap token updated.";
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnBootstrapTokenUpdated(bootstrap_token));
  }

  if (!success) {
    if (cryptographer->is_ready()) {
      LOG(ERROR) << "Attempt to change passphrase failed while cryptographer "
                 << "was ready.";
    } else if (cryptographer->has_pending_keys()) {
      FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                        OnPassphraseRequired(sync_api::REASON_DECRYPTION,
                                             cryptographer->GetPendingKeys()));
    } else {
      FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                        OnPassphraseRequired(sync_api::REASON_ENCRYPTION,
                                             sync_pb::EncryptedData()));
    }
    return;
  }

  FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                    OnPassphraseAccepted());
  DCHECK(cryptographer->is_ready());

  // TODO(tim): Bug 58231. It would be nice if setting a passphrase didn't
  // require messing with the Nigori node, because we can't set a passphrase
  // until download conditions are met vs Cryptographer init.  It seems like
  // it's safe to defer this work.
  sync_pb::NigoriSpecifics specifics(nigori_node->GetNigoriSpecifics());
  // Does not modify specifics.encrypted() if the original decrypted data was
  // the same.
  if (!cryptographer->GetKeys(specifics.mutable_encrypted())) {
    NOTREACHED();
    return;
  }
  specifics.set_using_explicit_passphrase(is_explicit);
  nigori_node->SetNigoriSpecifics(specifics);

  // Does nothing if everything is already encrypted or the cryptographer has
  // pending keys.
  ReEncryptEverything(trans);
}

bool SyncManager::SyncInternal::IsUsingExplicitPassphrase() {
  ReadTransaction trans(FROM_HERE, &share_);
  ReadNode node(&trans);
  if (node.InitByTagLookup(kNigoriTag) != sync_api::BaseNode::INIT_OK) {
    // TODO(albertb): Plumb an UnrecoverableError all the way back to the PSS.
    NOTREACHED();
    return false;
  }

  return node.GetNigoriSpecifics().using_explicit_passphrase();
}

void SyncManager::SyncInternal::RefreshEncryption() {
  DCHECK(initialized_);

  WriteTransaction trans(FROM_HERE, GetUserShare());
  WriteNode node(&trans);
  if (node.InitByTagLookup(kNigoriTag) != sync_api::BaseNode::INIT_OK) {
    NOTREACHED() << "Unable to set encrypted datatypes because Nigori node not "
                 << "found.";
    return;
  }

  Cryptographer* cryptographer = trans.GetCryptographer();

  if (!cryptographer->is_ready()) {
    DVLOG(1) << "Attempting to encrypt datatypes when cryptographer not "
             << "initialized, prompting for passphrase.";
    // TODO(zea): this isn't really decryption, but that's the only way we have
    // to prompt the user for a passsphrase. See http://crbug.com/91379.
    sync_pb::EncryptedData pending_keys;
    if (cryptographer->has_pending_keys())
      pending_keys = cryptographer->GetPendingKeys();
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnPassphraseRequired(sync_api::REASON_DECRYPTION,
                                           pending_keys));
    return;
  }

  UpdateNigoriEncryptionState(cryptographer, &node);

  allstatus_.SetEncryptedTypes(cryptographer->GetEncryptedTypes());

  // We reencrypt everything regardless of whether the set of encrypted
  // types changed to ensure that any stray unencrypted entries are overwritten.
  ReEncryptEverything(&trans);
}

// This function iterates over all encrypted types.  There are many scenarios in
// which data for some or all types is not currently available.  In that case,
// the lookup of the root node will fail and we will skip encryption for that
// type.
void SyncManager::SyncInternal::ReEncryptEverything(WriteTransaction* trans) {
  Cryptographer* cryptographer = trans->GetCryptographer();
  if (!cryptographer || !cryptographer->is_ready())
    return;
  syncable::ModelTypeSet encrypted_types = GetEncryptedTypes(trans);
  for (syncable::ModelTypeSet::Iterator iter = encrypted_types.First();
       iter.Good(); iter.Inc()) {
    if (iter.Get() == syncable::PASSWORDS ||
        iter.Get() == syncable::NIGORI)
      continue; // These types handle encryption differently.

    ReadNode type_root(trans);
    std::string tag = syncable::ModelTypeToRootTag(iter.Get());
    if (type_root.InitByTagLookup(tag) != sync_api::BaseNode::INIT_OK)
      continue; // Don't try to reencrypt if the type's data is unavailable.

    // Iterate through all children of this datatype.
    std::queue<int64> to_visit;
    int64 child_id = type_root.GetFirstChildId();
    to_visit.push(child_id);
    while (!to_visit.empty()) {
      child_id = to_visit.front();
      to_visit.pop();
      if (child_id == kInvalidId)
        continue;

      WriteNode child(trans);
      if (child.InitByIdLookup(child_id) != sync_api::BaseNode::INIT_OK) {
        NOTREACHED();
        continue;
      }
      if (child.GetIsFolder()) {
        to_visit.push(child.GetFirstChildId());
      }
      if (child.GetEntry()->Get(syncable::UNIQUE_SERVER_TAG).empty()) {
      // Rewrite the specifics of the node with encrypted data if necessary
      // (only rewrite the non-unique folders).
        child.ResetFromSpecifics();
      }
      to_visit.push(child.GetSuccessorId());
    }
  }

  // Passwords are encrypted with their own legacy scheme.  Passwords are always
  // encrypted so we don't need to check GetEncryptedTypes() here.
  ReadNode passwords_root(trans);
  std::string passwords_tag =
      syncable::ModelTypeToRootTag(syncable::PASSWORDS);
  if (passwords_root.InitByTagLookup(passwords_tag) ==
          sync_api::BaseNode::INIT_OK) {
    int64 child_id = passwords_root.GetFirstChildId();
    while (child_id != kInvalidId) {
      WriteNode child(trans);
      if (child.InitByIdLookup(child_id) != sync_api::BaseNode::INIT_OK) {
        NOTREACHED();
        return;
      }
      child.SetPasswordSpecifics(child.GetPasswordSpecifics());
      child_id = child.GetSuccessorId();
    }
  }

  // NOTE: We notify from within a transaction.
  FOR_EACH_OBSERVER(SyncManager::Observer, observers_, OnEncryptionComplete());
}

SyncManager::~SyncManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delete data_;
}

void SyncManager::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->AddObserver(observer);
}

void SyncManager::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->RemoveObserver(observer);
}

void SyncManager::StopSyncingForShutdown(const base::Closure& callback) {
  data_->StopSyncingForShutdown(callback);
}

void SyncManager::SyncInternal::StopSyncingForShutdown(
    const base::Closure& callback) {
  DVLOG(2) << "StopSyncingForShutdown";
  if (scheduler())  // May be null in tests.
    scheduler()->RequestStop(callback);
  else
    created_on_loop_->PostTask(FROM_HERE, callback);

  if (connection_manager_.get())
    connection_manager_->TerminateAllIO();
}

void SyncManager::ShutdownOnSyncThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->ShutdownOnSyncThread();
}

void SyncManager::SyncInternal::ShutdownOnSyncThread() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Prevent any in-flight method calls from running.  Also
  // invalidates |weak_handle_this_| and |change_observer_|.
  weak_ptr_factory_.InvalidateWeakPtrs();
  js_mutation_event_observer_.InvalidateWeakPtrs();

  scheduler_.reset();
  session_context_.reset();

  SetJsEventHandler(WeakHandle<JsEventHandler>());
  RemoveObserver(&js_sync_manager_observer_);

  RemoveObserver(&debug_info_event_listener_);

  if (sync_notifier_.get()) {
    sync_notifier_->RemoveObserver(this);
  }
  sync_notifier_.reset();

  if (connection_manager_.get()) {
    connection_manager_->RemoveListener(this);
  }
  connection_manager_.reset();

  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  observing_ip_address_changes_ = false;

  if (initialized_ && directory()) {
    {
      // Cryptographer should only be accessed while holding a
      // transaction.
      ReadTransaction trans(FROM_HERE, GetUserShare());
      trans.GetCryptographer()->RemoveObserver(this);
    }
    directory()->SaveChanges();
  }

  share_.directory.reset();

  change_delegate_ = NULL;

  initialized_ = false;

  // We reset these here, since only now we know they will not be
  // accessed from other threads (since we shut down everything).
  change_observer_.Reset();
  weak_handle_this_.Reset();
}

void SyncManager::SyncInternal::OnIPAddressChanged() {
  DVLOG(1) << "IP address change detected";
  if (!observing_ip_address_changes_) {
    DVLOG(1) << "IP address change dropped.";
    return;
  }

  OnIPAddressChangedImpl();
}

void SyncManager::SyncInternal::OnIPAddressChangedImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (scheduler())
    scheduler()->OnConnectionStatusChange();
}

void SyncManager::SyncInternal::OnServerConnectionEvent(
    const ServerConnectionEvent& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (event.connection_code ==
      browser_sync::HttpResponse::SERVER_CONNECTION_OK) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnConnectionStatusChange(CONNECTION_OK));
  }

  if (event.connection_code == browser_sync::HttpResponse::SYNC_AUTH_ERROR) {
    observing_ip_address_changes_ = false;
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnConnectionStatusChange(CONNECTION_AUTH_ERROR));
  }

  if (event.connection_code ==
      browser_sync::HttpResponse::SYNC_SERVER_ERROR) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnConnectionStatusChange(CONNECTION_SERVER_ERROR));
  }
}

void SyncManager::SyncInternal::HandleTransactionCompleteChangeEvent(
    ModelTypeSet models_with_changes) {
  // This notification happens immediately after the transaction mutex is
  // released. This allows work to be performed without blocking other threads
  // from acquiring a transaction.
  if (!change_delegate_)
    return;

  // Call commit.
  for (ModelTypeSet::Iterator it = models_with_changes.First();
       it.Good(); it.Inc()) {
    change_delegate_->OnChangesComplete(it.Get());
    change_observer_.Call(
        FROM_HERE, &SyncManager::ChangeObserver::OnChangesComplete, it.Get());
  }
}

ModelTypeSet
    SyncManager::SyncInternal::HandleTransactionEndingChangeEvent(
        const ImmutableWriteTransactionInfo& write_transaction_info,
        syncable::BaseTransaction* trans) {
  // This notification happens immediately before a syncable WriteTransaction
  // falls out of scope. It happens while the channel mutex is still held,
  // and while the transaction mutex is held, so it cannot be re-entrant.
  if (!change_delegate_ || ChangeBuffersAreEmpty())
    return ModelTypeSet();

  // This will continue the WriteTransaction using a read only wrapper.
  // This is the last chance for read to occur in the WriteTransaction
  // that's closing. This special ReadTransaction will not close the
  // underlying transaction.
  ReadTransaction read_trans(GetUserShare(), trans);

  ModelTypeSet models_with_changes;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    const syncable::ModelType type = syncable::ModelTypeFromInt(i);
    if (change_buffers_[type].IsEmpty())
      continue;

    ImmutableChangeRecordList ordered_changes;
    // TODO(akalin): Propagate up the error further (see
    // http://crbug.com/100907).
    CHECK(change_buffers_[type].GetAllChangesInTreeOrder(&read_trans,
                                                         &ordered_changes));
    if (!ordered_changes.Get().empty()) {
      change_delegate_->
          OnChangesApplied(type, &read_trans, ordered_changes);
      change_observer_.Call(FROM_HERE,
          &SyncManager::ChangeObserver::OnChangesApplied,
          type, write_transaction_info.Get().id, ordered_changes);
      models_with_changes.Put(type);
    }
    change_buffers_[i].Clear();
  }
  return models_with_changes;
}

void SyncManager::SyncInternal::HandleCalculateChangesChangeEventFromSyncApi(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    syncable::BaseTransaction* trans) {
  if (!scheduler()) {
    return;
  }

  // We have been notified about a user action changing a sync model.
  LOG_IF(WARNING, !ChangeBuffersAreEmpty()) <<
      "CALCULATE_CHANGES called with unapplied old changes.";

  // The mutated model type, or UNSPECIFIED if nothing was mutated.
  syncable::ModelTypeSet mutated_model_types;

  const syncable::ImmutableEntryKernelMutationMap& mutations =
      write_transaction_info.Get().mutations;
  for (syncable::EntryKernelMutationMap::const_iterator it =
           mutations.Get().begin(); it != mutations.Get().end(); ++it) {
    if (!it->second.mutated.ref(syncable::IS_UNSYNCED)) {
      continue;
    }

    syncable::ModelType model_type =
        syncable::GetModelTypeFromSpecifics(
            it->second.mutated.ref(SPECIFICS));
    if (model_type < syncable::FIRST_REAL_MODEL_TYPE) {
      NOTREACHED() << "Permanent or underspecified item changed via syncapi.";
      continue;
    }

    // Found real mutation.
    if (model_type != syncable::UNSPECIFIED) {
      mutated_model_types.Put(model_type);
    }
  }

  // Nudge if necessary.
  if (!mutated_model_types.Empty()) {
    if (weak_handle_this_.IsInitialized()) {
      weak_handle_this_.Call(FROM_HERE,
                             &SyncInternal::RequestNudgeForDataTypes,
                             FROM_HERE,
                             mutated_model_types);
    } else {
      NOTREACHED();
    }
  }
}

void SyncManager::SyncInternal::SetExtraChangeRecordData(int64 id,
    syncable::ModelType type, ChangeReorderBuffer* buffer,
    Cryptographer* cryptographer, const syncable::EntryKernel& original,
    bool existed_before, bool exists_now) {
  // If this is a deletion and the datatype was encrypted, we need to decrypt it
  // and attach it to the buffer.
  if (!exists_now && existed_before) {
    sync_pb::EntitySpecifics original_specifics(original.ref(SPECIFICS));
    if (type == syncable::PASSWORDS) {
      // Passwords must use their own legacy ExtraPasswordChangeRecordData.
      scoped_ptr<sync_pb::PasswordSpecificsData> data(
          DecryptPasswordSpecifics(original_specifics, cryptographer));
      if (!data.get()) {
        NOTREACHED();
        return;
      }
      buffer->SetExtraDataForId(id, new ExtraPasswordChangeRecordData(*data));
    } else if (original_specifics.has_encrypted()) {
      // All other datatypes can just create a new unencrypted specifics and
      // attach it.
      const sync_pb::EncryptedData& encrypted = original_specifics.encrypted();
      if (!cryptographer->Decrypt(encrypted, &original_specifics)) {
        NOTREACHED();
        return;
      }
    }
    buffer->SetSpecificsForId(id, original_specifics);
  }
}

void SyncManager::SyncInternal::HandleCalculateChangesChangeEventFromSyncer(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    syncable::BaseTransaction* trans) {
  // We only expect one notification per sync step, so change_buffers_ should
  // contain no pending entries.
  LOG_IF(WARNING, !ChangeBuffersAreEmpty()) <<
      "CALCULATE_CHANGES called with unapplied old changes.";

  Cryptographer* crypto = directory()->GetCryptographer(trans);
  const syncable::ImmutableEntryKernelMutationMap& mutations =
      write_transaction_info.Get().mutations;
  for (syncable::EntryKernelMutationMap::const_iterator it =
           mutations.Get().begin(); it != mutations.Get().end(); ++it) {
    bool existed_before = !it->second.original.ref(syncable::IS_DEL);
    bool exists_now = !it->second.mutated.ref(syncable::IS_DEL);

    // Omit items that aren't associated with a model.
    syncable::ModelType type =
        syncable::GetModelTypeFromSpecifics(
            it->second.mutated.ref(SPECIFICS));
    if (type < syncable::FIRST_REAL_MODEL_TYPE)
      continue;

    int64 handle = it->first;
    if (exists_now && !existed_before)
      change_buffers_[type].PushAddedItem(handle);
    else if (!exists_now && existed_before)
      change_buffers_[type].PushDeletedItem(handle);
    else if (exists_now && existed_before &&
             VisiblePropertiesDiffer(it->second, crypto)) {
      change_buffers_[type].PushUpdatedItem(
          handle, VisiblePositionsDiffer(it->second));
    }

    SetExtraChangeRecordData(handle, type, &change_buffers_[type], crypto,
                             it->second.original, existed_before, exists_now);
  }
}

SyncManager::Status SyncManager::SyncInternal::GetStatus() {
  return allstatus_.status();
}

void SyncManager::SyncInternal::RequestNudge(
    const tracked_objects::Location& location) {
  if (scheduler()) {
     scheduler()->ScheduleNudgeAsync(
        TimeDelta::FromMilliseconds(0), browser_sync::NUDGE_SOURCE_LOCAL,
        ModelTypeSet(), location);
  }
}

TimeDelta SyncManager::SyncInternal::GetNudgeDelayTimeDelta(
    const ModelType& model_type) {
  return NudgeStrategy::GetNudgeDelayTimeDelta(model_type, this);
}

void SyncManager::SyncInternal::RequestNudgeForDataTypes(
    const tracked_objects::Location& nudge_location,
    ModelTypeSet types) {
  if (!scheduler()) {
    NOTREACHED();
    return;
  }

  debug_info_event_listener_.OnNudgeFromDatatype(types.First().Get());

  // TODO(lipalani) : Calculate the nudge delay based on all types.
  base::TimeDelta nudge_delay = NudgeStrategy::GetNudgeDelayTimeDelta(
      types.First().Get(),
      this);
  scheduler()->ScheduleNudgeAsync(nudge_delay,
                                  browser_sync::NUDGE_SOURCE_LOCAL,
                                  types,
                                  nudge_location);
}

void SyncManager::SyncInternal::OnSyncEngineEvent(
    const SyncEngineEvent& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Only send an event if this is due to a cycle ending and this cycle
  // concludes a canonical "sync" process; that is, based on what is known
  // locally we are "all happy" and up-to-date.  There may be new changes on
  // the server, but we'll get them on a subsequent sync.
  //
  // Notifications are sent at the end of every sync cycle, regardless of
  // whether we should sync again.
  if (event.what_happened == SyncEngineEvent::SYNC_CYCLE_ENDED) {
    {
      // Check to see if we need to notify the frontend that we have newly
      // encrypted types or that we require a passphrase.
      ReadTransaction trans(FROM_HERE, GetUserShare());
      Cryptographer* cryptographer = trans.GetCryptographer();
      // If we've completed a sync cycle and the cryptographer isn't ready
      // yet, prompt the user for a passphrase.
      if (cryptographer->has_pending_keys()) {
        DVLOG(1) << "OnPassPhraseRequired Sent";
        sync_pb::EncryptedData pending_keys = cryptographer->GetPendingKeys();
        FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                          OnPassphraseRequired(sync_api::REASON_DECRYPTION,
                                               pending_keys));
      } else if (!cryptographer->is_ready() &&
                 event.snapshot.initial_sync_ended().Has(syncable::NIGORI)) {
        DVLOG(1) << "OnPassphraseRequired sent because cryptographer is not "
                 << "ready";
        FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                          OnPassphraseRequired(sync_api::REASON_ENCRYPTION,
                                               sync_pb::EncryptedData()));
      }

      NotifyCryptographerState(cryptographer);
      allstatus_.SetEncryptedTypes(cryptographer->GetEncryptedTypes());
    }

    if (!initialized_) {
      LOG(INFO) << "OnSyncCycleCompleted not sent because sync api is not "
                << "initialized";
      return;
    }

    if (!event.snapshot.has_more_to_sync()) {
      // To account for a nigori node arriving with stale/bad data, we ensure
      // that the nigori node is up to date at the end of each cycle.
      WriteTransaction trans(FROM_HERE, GetUserShare());
      WriteNode nigori_node(&trans);
      if (nigori_node.InitByTagLookup(kNigoriTag) ==
              sync_api::BaseNode::INIT_OK) {
        Cryptographer* cryptographer = trans.GetCryptographer();
        UpdateNigoriEncryptionState(cryptographer, &nigori_node);
      }

      DVLOG(1) << "Sending OnSyncCycleCompleted";
      FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                        OnSyncCycleCompleted(event.snapshot));
    }

    // This is here for tests, which are still using p2p notifications.
    //
    // TODO(chron): Consider changing this back to track has_more_to_sync
    // only notify peers if a successful commit has occurred.
    bool is_notifiable_commit =
        (event.snapshot.syncer_status().num_successful_commits > 0);
    if (is_notifiable_commit) {
      if (sync_notifier_.get()) {
        const ModelTypeSet changed_types =
            syncable::ModelTypePayloadMapToEnumSet(
                event.snapshot.source().types);
        sync_notifier_->SendNotification(changed_types);
      } else {
        DVLOG(1) << "Not sending notification: sync_notifier_ is NULL";
      }
    }
  }

  if (event.what_happened == SyncEngineEvent::STOP_SYNCING_PERMANENTLY) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnStopSyncingPermanently());
    return;
  }

  if (event.what_happened == SyncEngineEvent::CLEAR_SERVER_DATA_SUCCEEDED) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnClearServerDataSucceeded());
    return;
  }

  if (event.what_happened == SyncEngineEvent::CLEAR_SERVER_DATA_FAILED) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnClearServerDataFailed());
    return;
  }

  if (event.what_happened == SyncEngineEvent::UPDATED_TOKEN) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnUpdatedToken(event.updated_token));
    return;
  }

  if (event.what_happened == SyncEngineEvent::ACTIONABLE_ERROR) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnActionableError(
                          event.snapshot.errors().sync_protocol_error));
    return;
  }

}

void SyncManager::SyncInternal::SetJsEventHandler(
    const WeakHandle<JsEventHandler>& event_handler) {
  js_event_handler_ = event_handler;
  js_sync_manager_observer_.SetJsEventHandler(js_event_handler_);
  js_mutation_event_observer_.SetJsEventHandler(js_event_handler_);
}

void SyncManager::SyncInternal::ProcessJsMessage(
    const std::string& name, const JsArgList& args,
    const WeakHandle<JsReplyHandler>& reply_handler) {
  if (!initialized_) {
    NOTREACHED();
    return;
  }

  if (!reply_handler.IsInitialized()) {
    DVLOG(1) << "Uninitialized reply handler; dropping unknown message "
            << name << " with args " << args.ToString();
    return;
  }

  JsMessageHandler js_message_handler = js_message_handlers_[name];
  if (js_message_handler.is_null()) {
    DVLOG(1) << "Dropping unknown message " << name
             << " with args " << args.ToString();
    return;
  }

  reply_handler.Call(FROM_HERE,
                     &JsReplyHandler::HandleJsReply,
                     name, js_message_handler.Run(args));
}

void SyncManager::SyncInternal::BindJsMessageHandler(
    const std::string& name,
    UnboundJsMessageHandler unbound_message_handler) {
  js_message_handlers_[name] =
      base::Bind(unbound_message_handler, base::Unretained(this));
}

DictionaryValue* SyncManager::SyncInternal::NotificationInfoToValue(
    const NotificationInfoMap& notification_info) {
  DictionaryValue* value = new DictionaryValue();

  for (NotificationInfoMap::const_iterator it = notification_info.begin();
      it != notification_info.end(); ++it) {
    const std::string& model_type_str =
        syncable::ModelTypeToString(it->first);
    value->Set(model_type_str, it->second.ToValue());
  }

  return value;
}

JsArgList SyncManager::SyncInternal::GetNotificationState(
    const JsArgList& args) {
  bool notifications_enabled = allstatus_.status().notifications_enabled;
  ListValue return_args;
  return_args.Append(Value::CreateBooleanValue(notifications_enabled));
  return JsArgList(&return_args);
}

JsArgList SyncManager::SyncInternal::GetNotificationInfo(
    const JsArgList& args) {
  ListValue return_args;
  return_args.Append(NotificationInfoToValue(notification_info_map_));
  return JsArgList(&return_args);
}

JsArgList SyncManager::SyncInternal::GetRootNodeDetails(
    const JsArgList& args) {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  ReadNode root(&trans);
  root.InitByRootLookup();
  ListValue return_args;
  return_args.Append(root.GetDetailsAsValue());
  return JsArgList(&return_args);
}

JsArgList SyncManager::SyncInternal::GetClientServerTraffic(
    const JsArgList& args) {
  ListValue return_args;
  ListValue* value = traffic_recorder_.ToValue();
  if (value != NULL)
    return_args.Append(value);
  return JsArgList(&return_args);
}

namespace {

int64 GetId(const ListValue& ids, int i) {
  std::string id_str;
  if (!ids.GetString(i, &id_str)) {
    return kInvalidId;
  }
  int64 id = kInvalidId;
  if (!base::StringToInt64(id_str, &id)) {
    return kInvalidId;
  }
  return id;
}

JsArgList GetNodeInfoById(const JsArgList& args,
                          UserShare* user_share,
                          DictionaryValue* (BaseNode::*info_getter)() const) {
  CHECK(info_getter);
  ListValue return_args;
  ListValue* node_summaries = new ListValue();
  return_args.Append(node_summaries);
  ListValue* id_list = NULL;
  ReadTransaction trans(FROM_HERE, user_share);
  if (args.Get().GetList(0, &id_list)) {
    CHECK(id_list);
    for (size_t i = 0; i < id_list->GetSize(); ++i) {
      int64 id = GetId(*id_list, i);
      if (id == kInvalidId) {
        continue;
      }
      ReadNode node(&trans);
      if (node.InitByIdLookup(id) != sync_api::BaseNode::INIT_OK) {
        continue;
      }
      node_summaries->Append((node.*info_getter)());
    }
  }
  return JsArgList(&return_args);
}

}  // namespace

JsArgList SyncManager::SyncInternal::GetNodeSummariesById(
    const JsArgList& args) {
  return GetNodeInfoById(args, GetUserShare(), &BaseNode::GetSummaryAsValue);
}

JsArgList SyncManager::SyncInternal::GetNodeDetailsById(
    const JsArgList& args) {
  return GetNodeInfoById(args, GetUserShare(), &BaseNode::GetDetailsAsValue);
}

JsArgList SyncManager::SyncInternal::GetAllNodes(
    const JsArgList& args) {
  ListValue return_args;
  ListValue* result = new ListValue();
  return_args.Append(result);

  ReadTransaction trans(FROM_HERE, GetUserShare());
  std::vector<const syncable::EntryKernel*> entry_kernels;
  trans.GetDirectory()->GetAllEntryKernels(trans.GetWrappedTrans(),
                                           &entry_kernels);

  for (std::vector<const syncable::EntryKernel*>::const_iterator it =
           entry_kernels.begin(); it != entry_kernels.end(); ++it) {
    result->Append((*it)->ToValue());
  }

  return JsArgList(&return_args);
}

JsArgList SyncManager::SyncInternal::GetChildNodeIds(
    const JsArgList& args) {
  ListValue return_args;
  ListValue* child_ids = new ListValue();
  return_args.Append(child_ids);
  int64 id = GetId(args.Get(), 0);
  if (id != kInvalidId) {
    ReadTransaction trans(FROM_HERE, GetUserShare());
    syncable::Directory::ChildHandles child_handles;
    trans.GetDirectory()->GetChildHandlesByHandle(trans.GetWrappedTrans(),
                                                  id, &child_handles);
    for (syncable::Directory::ChildHandles::const_iterator it =
             child_handles.begin(); it != child_handles.end(); ++it) {
      child_ids->Append(Value::CreateStringValue(
          base::Int64ToString(*it)));
    }
  }
  return JsArgList(&return_args);
}

void SyncManager::SyncInternal::OnEncryptedTypesChanged(
    syncable::ModelTypeSet encrypted_types,
    bool encrypt_everything) {
  // NOTE: We're in a transaction.
  FOR_EACH_OBSERVER(
      SyncManager::Observer, observers_,
      OnEncryptedTypesChanged(encrypted_types, encrypt_everything));
}

void SyncManager::SyncInternal::OnNotificationStateChange(
    bool notifications_enabled) {
  DVLOG(1) << "P2P: Notifications enabled = "
           << (notifications_enabled ? "true" : "false");
  allstatus_.SetNotificationsEnabled(notifications_enabled);
  if (scheduler()) {
    scheduler()->set_notifications_enabled(notifications_enabled);
  }
  if (js_event_handler_.IsInitialized()) {
    DictionaryValue details;
    details.Set("enabled", Value::CreateBooleanValue(notifications_enabled));
    js_event_handler_.Call(FROM_HERE,
                           &JsEventHandler::HandleJsEvent,
                           "onNotificationStateChange",
                           JsEventDetails(&details));
  }
}

void SyncManager::SyncInternal::UpdateNotificationInfo(
    const syncable::ModelTypePayloadMap& type_payloads) {
  for (syncable::ModelTypePayloadMap::const_iterator it = type_payloads.begin();
       it != type_payloads.end(); ++it) {
    NotificationInfo* info = &notification_info_map_[it->first];
    info->total_count++;
    info->payload = it->second;
  }
}

void SyncManager::SyncInternal::OnIncomingNotification(
    const syncable::ModelTypePayloadMap& type_payloads,
    sync_notifier::IncomingNotificationSource source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (source == sync_notifier::LOCAL_NOTIFICATION) {
    if (scheduler()) {
      scheduler()->ScheduleNudgeWithPayloadsAsync(
          TimeDelta::FromMilliseconds(kSyncRefreshDelayMsec),
          browser_sync::NUDGE_SOURCE_LOCAL_REFRESH,
          type_payloads, FROM_HERE);
    }
  } else if (!type_payloads.empty()) {
    if (scheduler()) {
      scheduler()->ScheduleNudgeWithPayloadsAsync(
          TimeDelta::FromMilliseconds(kSyncSchedulerDelayMsec),
          browser_sync::NUDGE_SOURCE_NOTIFICATION,
          type_payloads, FROM_HERE);
    }
    allstatus_.IncrementNotificationsReceived();
    UpdateNotificationInfo(type_payloads);
    debug_info_event_listener_.OnIncomingNotification(type_payloads);
  } else {
    LOG(WARNING) << "Sync received notification without any type information.";
  }

  if (js_event_handler_.IsInitialized()) {
    DictionaryValue details;
    ListValue* changed_types = new ListValue();
    details.Set("changedTypes", changed_types);
    for (syncable::ModelTypePayloadMap::const_iterator
             it = type_payloads.begin();
         it != type_payloads.end(); ++it) {
      const std::string& model_type_str =
          syncable::ModelTypeToString(it->first);
      changed_types->Append(Value::CreateStringValue(model_type_str));
    }
    details.SetString("source", (source == sync_notifier::LOCAL_NOTIFICATION) ?
        "LOCAL_NOTIFICATION" : "REMOTE_NOTIFICATION");
    js_event_handler_.Call(FROM_HERE,
                           &JsEventHandler::HandleJsEvent,
                           "onIncomingNotification",
                           JsEventDetails(&details));
  }
}

void SyncManager::SyncInternal::AddObserver(
    SyncManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void SyncManager::SyncInternal::RemoveObserver(
    SyncManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

SyncManager::Status SyncManager::GetDetailedStatus() const {
  return data_->GetStatus();
}

void SyncManager::SaveChanges() {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->SaveChanges();
}

void SyncManager::SyncInternal::SaveChanges() {
  directory()->SaveChanges();
}

UserShare* SyncManager::GetUserShare() const {
  return data_->GetUserShare();
}

void SyncManager::RefreshNigori(const std::string& chrome_version,
                                const base::Closure& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->UpdateCryptographerAndNigori(
      chrome_version,
      done_callback);
}

TimeDelta SyncManager::GetNudgeDelayTimeDelta(
    const ModelType& model_type) {
  return data_->GetNudgeDelayTimeDelta(model_type);
}

syncable::ModelTypeSet SyncManager::GetEncryptedDataTypesForTest() const {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  return GetEncryptedTypes(&trans);
}

bool SyncManager::ReceivedExperiment(browser_sync::Experiments* experiments)
    const {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  ReadNode node(&trans);
  if (node.InitByTagLookup(kNigoriTag) != sync_api::BaseNode::INIT_OK) {
    DVLOG(1) << "Couldn't find Nigori node.";
    return false;
  }
  bool found_experiment = false;
  if (node.GetNigoriSpecifics().sync_tab_favicons()) {
    experiments->sync_tab_favicons = true;
    found_experiment = true;
  }
  return found_experiment;
}

bool SyncManager::HasUnsyncedItems() const {
  sync_api::ReadTransaction trans(FROM_HERE, GetUserShare());
  return (trans.GetWrappedTrans()->directory()->unsynced_entity_count() != 0);
}

void SyncManager::TriggerOnNotificationStateChangeForTest(
    bool notifications_enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->OnNotificationStateChange(notifications_enabled);
}

void SyncManager::TriggerOnIncomingNotificationForTest(
    ModelTypeSet model_types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  syncable::ModelTypePayloadMap model_types_with_payloads =
      syncable::ModelTypePayloadMapFromEnumSet(model_types,
          std::string());

  data_->OnIncomingNotification(model_types_with_payloads,
                                sync_notifier::REMOTE_NOTIFICATION);
}

const char* ConnectionStatusToString(ConnectionStatus status) {
  switch (status) {
    case CONNECTION_OK:
      return "CONNECTION_OK";
    case CONNECTION_AUTH_ERROR:
      return "CONNECTION_AUTH_ERROR";
    case CONNECTION_SERVER_ERROR:
      return "CONNECTION_SERVER_ERROR";
    default:
      NOTREACHED();
      return "INVALID_CONNECTION_STATUS";
  }
}

// Helper function that converts a PassphraseRequiredReason value to a string.
const char* PassphraseRequiredReasonToString(
    PassphraseRequiredReason reason) {
  switch (reason) {
    case REASON_PASSPHRASE_NOT_REQUIRED:
      return "REASON_PASSPHRASE_NOT_REQUIRED";
    case REASON_ENCRYPTION:
      return "REASON_ENCRYPTION";
    case REASON_DECRYPTION:
      return "REASON_DECRYPTION";
    default:
      NOTREACHED();
      return "INVALID_REASON";
  }
}

// Helper function to determine if initial sync had ended for types.
bool InitialSyncEndedForTypes(syncable::ModelTypeSet types,
                              sync_api::UserShare* share) {
  for (syncable::ModelTypeSet::Iterator i = types.First();
       i.Good(); i.Inc()) {
    if (!share->directory->initial_sync_ended_for_type(i.Get()))
      return false;
  }
  return true;
}

syncable::ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
    syncable::ModelTypeSet types,
    sync_api::UserShare* share) {
  syncable::ModelTypeSet result;
  for (syncable::ModelTypeSet::Iterator i = types.First();
       i.Good(); i.Inc()) {
    sync_pb::DataTypeProgressMarker marker;
    share->directory->GetDownloadProgress(i.Get(), &marker);

    if (marker.token().empty())
      result.Put(i.Get());

  }
  return result;
}

}  // namespace sync_api
