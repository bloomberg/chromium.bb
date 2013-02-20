// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_manager_impl.h"

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
#include "sync/engine/sync_scheduler.h"
#include "sync/engine/syncer_types.h"
#include "sync/internal_api/change_reorder_buffer.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "sync/internal_api/public/base_node.h"
#include "sync/internal_api/public/configure_reason.h"
#include "sync/internal_api/public/engine/polling_constants.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/internal_components_factory.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/internal_api/syncapi_internal.h"
#include "sync/internal_api/syncapi_server_connection_manager.h"
#include "sync/js/js_arg_list.h"
#include "sync/js/js_event_details.h"
#include "sync/js/js_event_handler.h"
#include "sync/js/js_reply_handler.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/invalidator.h"
#include "sync/protocol/proto_value_conversions.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/in_memory_directory_backing_store.h"
#include "sync/syncable/on_disk_directory_backing_store.h"

using base::TimeDelta;
using sync_pb::GetUpdatesCallerInfo;

namespace syncer {

using sessions::SyncSessionContext;
using syncable::ImmutableWriteTransactionInfo;
using syncable::SPECIFICS;

namespace {

// Delays for syncer nudges.
static const int kDefaultNudgeDelayMilliseconds = 200;
static const int kPreferencesNudgeDelayMilliseconds = 2000;
static const int kSyncRefreshDelayMsec = 500;
static const int kSyncSchedulerDelayMsec = 250;

// Maximum count and size for traffic recorder.
static const unsigned int kMaxMessagesToRecord = 10;
static const unsigned int kMaxMessageSizeToRecord = 5 * 1024;

GetUpdatesCallerInfo::GetUpdatesSource GetSourceFromReason(
    ConfigureReason reason) {
  switch (reason) {
    case CONFIGURE_REASON_RECONFIGURATION:
      return GetUpdatesCallerInfo::RECONFIGURATION;
    case CONFIGURE_REASON_MIGRATION:
      return GetUpdatesCallerInfo::MIGRATION;
    case CONFIGURE_REASON_NEW_CLIENT:
      return GetUpdatesCallerInfo::NEW_CLIENT;
    case CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE:
      return GetUpdatesCallerInfo::NEWLY_SUPPORTED_DATATYPE;
    default:
      NOTREACHED();
  }
  return GetUpdatesCallerInfo::UNKNOWN;
}

}  // namespace

// A class to calculate nudge delays for types.
class NudgeStrategy {
 public:
  static TimeDelta GetNudgeDelayTimeDelta(const ModelType& model_type,
                                          SyncManagerImpl* core) {
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
     case AUTOFILL:
       return ACCOMPANY_ONLY;
     case PREFERENCES:
     case SESSIONS:
       return CUSTOM;
     default:
       return IMMEDIATE;
    }
  }

  static TimeDelta GetNudgeDelayTimeDeltaFromType(
      const NudgeDelayStrategy& delay_type, const ModelType& model_type,
      const SyncManagerImpl* core) {
    CHECK(core);
    TimeDelta delay = TimeDelta::FromMilliseconds(
       kDefaultNudgeDelayMilliseconds);
    switch (delay_type) {
     case IMMEDIATE:
       delay = TimeDelta::FromMilliseconds(
           kDefaultNudgeDelayMilliseconds);
       break;
     case ACCOMPANY_ONLY:
       delay = TimeDelta::FromSeconds(kDefaultShortPollIntervalSeconds);
       break;
     case CUSTOM:
       switch (model_type) {
         case PREFERENCES:
           delay = TimeDelta::FromMilliseconds(
               kPreferencesNudgeDelayMilliseconds);
           break;
         case SESSIONS:
           delay = core->scheduler()->GetSessionsCommitDelay();
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

SyncManagerImpl::SyncManagerImpl(const std::string& name)
    : name_(name),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      change_delegate_(NULL),
      initialized_(false),
      observing_network_connectivity_changes_(false),
      invalidator_state_(DEFAULT_INVALIDATION_ERROR),
      throttled_data_type_tracker_(&allstatus_),
      traffic_recorder_(kMaxMessagesToRecord, kMaxMessageSizeToRecord),
      encryptor_(NULL),
      unrecoverable_error_handler_(NULL),
      report_unrecoverable_error_function_(NULL) {
  // Pre-fill |notification_info_map_|.
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    notification_info_map_.insert(
        std::make_pair(ModelTypeFromInt(i), NotificationInfo()));
  }

  // Bind message handlers.
  BindJsMessageHandler(
      "getNotificationState",
      &SyncManagerImpl::GetNotificationState);
  BindJsMessageHandler(
      "getNotificationInfo",
      &SyncManagerImpl::GetNotificationInfo);
  BindJsMessageHandler(
      "getRootNodeDetails",
      &SyncManagerImpl::GetRootNodeDetails);
  BindJsMessageHandler(
      "getNodeSummariesById",
      &SyncManagerImpl::GetNodeSummariesById);
  BindJsMessageHandler(
     "getNodeDetailsById",
      &SyncManagerImpl::GetNodeDetailsById);
  BindJsMessageHandler(
      "getAllNodes",
      &SyncManagerImpl::GetAllNodes);
  BindJsMessageHandler(
      "getChildNodeIds",
      &SyncManagerImpl::GetChildNodeIds);
  BindJsMessageHandler(
      "getClientServerTraffic",
      &SyncManagerImpl::GetClientServerTraffic);
}

SyncManagerImpl::~SyncManagerImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(!initialized_);
}

SyncManagerImpl::NotificationInfo::NotificationInfo() : total_count(0) {}
SyncManagerImpl::NotificationInfo::~NotificationInfo() {}

base::DictionaryValue* SyncManagerImpl::NotificationInfo::ToValue() const {
  base::DictionaryValue* value = new base::DictionaryValue();
  value->SetInteger("totalCount", total_count);
  value->SetString("payload", payload);
  return value;
}

bool SyncManagerImpl::VisiblePositionsDiffer(
    const syncable::EntryKernelMutation& mutation) const {
  const syncable::EntryKernel& a = mutation.original;
  const syncable::EntryKernel& b = mutation.mutated;
  // If the datatype isn't one where the browser model cares about position,
  // don't bother notifying that data model of position-only changes.
  if (!ShouldMaintainPosition(GetModelTypeFromSpecifics(b.ref(SPECIFICS)))) {
    return false;
  }
  if (a.ref(syncable::NEXT_ID) != b.ref(syncable::NEXT_ID))
    return true;
  if (a.ref(syncable::PARENT_ID) != b.ref(syncable::PARENT_ID))
    return true;
  return false;
}

bool SyncManagerImpl::VisiblePropertiesDiffer(
    const syncable::EntryKernelMutation& mutation,
    Cryptographer* cryptographer) const {
  const syncable::EntryKernel& a = mutation.original;
  const syncable::EntryKernel& b = mutation.mutated;
  const sync_pb::EntitySpecifics& a_specifics = a.ref(SPECIFICS);
  const sync_pb::EntitySpecifics& b_specifics = b.ref(SPECIFICS);
  DCHECK_EQ(GetModelTypeFromSpecifics(a_specifics),
            GetModelTypeFromSpecifics(b_specifics));
  ModelType model_type = GetModelTypeFromSpecifics(b_specifics);
  // Suppress updates to items that aren't tracked by any browser model.
  if (model_type < FIRST_REAL_MODEL_TYPE ||
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

void SyncManagerImpl::ThrowUnrecoverableError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ReadTransaction trans(FROM_HERE, GetUserShare());
  trans.GetWrappedTrans()->OnUnrecoverableError(
      FROM_HERE, "Simulating unrecoverable error for testing purposes.");
}

ModelTypeSet SyncManagerImpl::InitialSyncEndedTypes() {
  return directory()->InitialSyncEndedTypes();
}

ModelTypeSet SyncManagerImpl::GetTypesWithEmptyProgressMarkerToken(
    ModelTypeSet types) {
  ModelTypeSet result;
  for (ModelTypeSet::Iterator i = types.First(); i.Good(); i.Inc()) {
    sync_pb::DataTypeProgressMarker marker;
    directory()->GetDownloadProgress(i.Get(), &marker);

    if (marker.token().empty())
      result.Put(i.Get());
  }
  return result;
}

void SyncManagerImpl::ConfigureSyncer(
    ConfigureReason reason,
    ModelTypeSet types_to_config,
    ModelTypeSet failed_types,
    const ModelSafeRoutingInfo& new_routing_info,
    const base::Closure& ready_task,
    const base::Closure& retry_task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!ready_task.is_null());
  DCHECK(!retry_task.is_null());

  // Cleanup any types that might have just been disabled.
  ModelTypeSet previous_types = ModelTypeSet::All();
  if (!session_context_->routing_info().empty())
    previous_types = GetRoutingInfoTypes(session_context_->routing_info());
  if (!PurgeDisabledTypes(previous_types,
                          GetRoutingInfoTypes(new_routing_info),
                          failed_types)) {
    // We failed to cleanup the types. Invoke the ready task without actually
    // configuring any types. The caller should detect this as a configuration
    // failure and act appropriately.
    ready_task.Run();
    return;
  }

  ConfigurationParams params(GetSourceFromReason(reason),
                             types_to_config,
                             new_routing_info,
                             ready_task);

  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE);
  if (!scheduler_->ScheduleConfiguration(params))
    retry_task.Run();
}

void SyncManagerImpl::Init(
    const base::FilePath& database_location,
    const WeakHandle<JsEventHandler>& event_handler,
    const std::string& sync_server_and_path,
    int port,
    bool use_ssl,
    scoped_ptr<HttpPostProviderFactory> post_factory,
    const std::vector<ModelSafeWorker*>& workers,
    ExtensionsActivityMonitor* extensions_activity_monitor,
    SyncManager::ChangeDelegate* change_delegate,
    const SyncCredentials& credentials,
    scoped_ptr<Invalidator> invalidator,
    const std::string& restored_key_for_bootstrapping,
    const std::string& restored_keystore_key_for_bootstrapping,
    scoped_ptr<InternalComponentsFactory> internal_components_factory,
    Encryptor* encryptor,
    UnrecoverableErrorHandler* unrecoverable_error_handler,
    ReportUnrecoverableErrorFunction report_unrecoverable_error_function) {
  CHECK(!initialized_);
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(post_factory.get());
  DCHECK(!credentials.email.empty());
  DCHECK(!credentials.sync_token.empty());
  DVLOG(1) << "SyncManager starting Init...";

  weak_handle_this_ = MakeWeakHandle(weak_ptr_factory_.GetWeakPtr());

  change_delegate_ = change_delegate;

  invalidator_ = invalidator.Pass();
  invalidator_->RegisterHandler(this);

  AddObserver(&js_sync_manager_observer_);
  SetJsEventHandler(event_handler);

  AddObserver(&debug_info_event_listener_);

  database_path_ = database_location.Append(
      syncable::Directory::kSyncDatabaseFilename);
  encryptor_ = encryptor;
  unrecoverable_error_handler_ = unrecoverable_error_handler;
  report_unrecoverable_error_function_ = report_unrecoverable_error_function;

  allstatus_.SetHasKeystoreKey(
      !restored_keystore_key_for_bootstrapping.empty());
  sync_encryption_handler_.reset(new SyncEncryptionHandlerImpl(
      &share_,
      encryptor,
      restored_key_for_bootstrapping,
      restored_keystore_key_for_bootstrapping));
  sync_encryption_handler_->AddObserver(this);
  sync_encryption_handler_->AddObserver(&debug_info_event_listener_);
  sync_encryption_handler_->AddObserver(&js_sync_encryption_handler_observer_);

  base::FilePath absolute_db_path(database_path_);
  file_util::AbsolutePath(&absolute_db_path);
  scoped_ptr<syncable::DirectoryBackingStore> backing_store =
      internal_components_factory->BuildDirectoryBackingStore(
          credentials.email, absolute_db_path).Pass();

  DCHECK(backing_store.get());
  const std::string& username = credentials.email;
  share_.directory.reset(
      new syncable::Directory(
          backing_store.release(),
          unrecoverable_error_handler_,
          report_unrecoverable_error_function_,
          sync_encryption_handler_.get(),
          sync_encryption_handler_->GetCryptographerUnsafe()));

  DVLOG(1) << "Username: " << username;
  if (!OpenDirectory(username)) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnInitializationComplete(
                          MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()),
                          MakeWeakHandle(
                              debug_info_event_listener_.GetWeakPtr()),
                          false, ModelTypeSet()));
    LOG(ERROR) << "Sync manager initialization failed!";
    return;
  }

  connection_manager_.reset(new SyncAPIServerConnectionManager(
      sync_server_and_path, port, use_ssl, post_factory.release()));
  connection_manager_->set_client_id(directory()->cache_guid());
  connection_manager_->AddListener(this);

  // Retrieve and set the sync notifier id.
  std::string unique_id = directory()->cache_guid();
  DVLOG(1) << "Read notification unique ID: " << unique_id;
  allstatus_.SetUniqueId(unique_id);
  invalidator_->SetUniqueId(unique_id);

  // Build a SyncSessionContext and store the worker in it.
  DVLOG(1) << "Sync is bringing up SyncSessionContext.";
  std::vector<SyncEngineEventListener*> listeners;
  listeners.push_back(&allstatus_);
  listeners.push_back(this);
  session_context_ = internal_components_factory->BuildContext(
      connection_manager_.get(),
      directory(),
      workers,
      extensions_activity_monitor,
      &throttled_data_type_tracker_,
      listeners,
      &debug_info_event_listener_,
      &traffic_recorder_).Pass();
  session_context_->set_account_name(credentials.email);
  scheduler_ = internal_components_factory->BuildScheduler(
      name_, session_context_.get()).Pass();

  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE);

  initialized_ = true;

  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  observing_network_connectivity_changes_ = true;

  UpdateCredentials(credentials);

  FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                    OnInitializationComplete(
                        MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()),
                        MakeWeakHandle(debug_info_event_listener_.GetWeakPtr()),
                        true, InitialSyncEndedTypes()));
}

void SyncManagerImpl::OnPassphraseRequired(
    PassphraseRequiredReason reason,
    const sync_pb::EncryptedData& pending_keys) {
  // Does nothing.
}

void SyncManagerImpl::OnPassphraseAccepted() {
  // Does nothing.
}

void SyncManagerImpl::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token,
    BootstrapTokenType type) {
  if (type == KEYSTORE_BOOTSTRAP_TOKEN)
    allstatus_.SetHasKeystoreKey(true);
}

void SyncManagerImpl::OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                                              bool encrypt_everything) {
  allstatus_.SetEncryptedTypes(encrypted_types);
}

void SyncManagerImpl::OnEncryptionComplete() {
  // Does nothing.
}

void SyncManagerImpl::OnCryptographerStateChanged(
    Cryptographer* cryptographer) {
  allstatus_.SetCryptographerReady(cryptographer->is_ready());
  allstatus_.SetCryptoHasPendingKeys(cryptographer->has_pending_keys());
  allstatus_.SetKeystoreMigrationTime(
      sync_encryption_handler_->migration_time());
}

void SyncManagerImpl::OnPassphraseTypeChanged(
    PassphraseType type,
    base::Time explicit_passphrase_time) {
  allstatus_.SetPassphraseType(type);
  allstatus_.SetKeystoreMigrationTime(
      sync_encryption_handler_->migration_time());
}

void SyncManagerImpl::StartSyncingNormally(
    const ModelSafeRoutingInfo& routing_info) {
  // Start the sync scheduler.
  // TODO(sync): We always want the newest set of routes when we switch back
  // to normal mode. Figure out how to enforce set_routing_info is always
  // appropriately set and that it's only modified when switching to normal
  // mode.
  DCHECK(thread_checker_.CalledOnValidThread());
  session_context_->set_routing_info(routing_info);
  scheduler_->Start(SyncScheduler::NORMAL_MODE);
}

syncable::Directory* SyncManagerImpl::directory() {
  return share_.directory.get();
}

const SyncScheduler* SyncManagerImpl::scheduler() const {
  return scheduler_.get();
}

bool SyncManagerImpl::GetHasInvalidAuthTokenForTest() const {
  return connection_manager_->HasInvalidAuthToken();
}

bool SyncManagerImpl::OpenDirectory(const std::string& username) {
  DCHECK(!initialized_) << "Should only happen once";

  // Set before Open().
  change_observer_ = MakeWeakHandle(js_mutation_event_observer_.AsWeakPtr());
  WeakHandle<syncable::TransactionObserver> transaction_observer(
      MakeWeakHandle(js_mutation_event_observer_.AsWeakPtr()));

  syncable::DirOpenResult open_result = syncable::NOT_INITIALIZED;
  open_result = directory()->Open(username, this, transaction_observer);
  if (open_result != syncable::OPENED) {
    LOG(ERROR) << "Could not open share for:" << username;
    return false;
  }

  // Unapplied datatypes (those that do not have initial sync ended set) get
  // re-downloaded during any configuration. But, it's possible for a datatype
  // to have a progress marker but not have initial sync ended yet, making
  // it a candidate for migration. This is a problem, as the DataTypeManager
  // does not support a migration while it's already in the middle of a
  // configuration. As a result, any partially synced datatype can stall the
  // DTM, waiting for the configuration to complete, which it never will due
  // to the migration error. In addition, a partially synced nigori will
  // trigger the migration logic before the backend is initialized, resulting
  // in crashes. We therefore detect and purge any partially synced types as
  // part of initialization.
  if (!PurgePartiallySyncedTypes())
    return false;

  return true;
}

bool SyncManagerImpl::PurgePartiallySyncedTypes() {
  ModelTypeSet partially_synced_types = ModelTypeSet::All();
  partially_synced_types.RemoveAll(InitialSyncEndedTypes());
  partially_synced_types.RemoveAll(GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet::All()));

  DVLOG(1) << "Purging partially synced types "
           << ModelTypeSetToString(partially_synced_types);
  UMA_HISTOGRAM_COUNTS("Sync.PartiallySyncedTypes",
                       partially_synced_types.Size());
  if (partially_synced_types.Empty())
    return true;
  return directory()->PurgeEntriesWithTypeIn(partially_synced_types,
                                             ModelTypeSet());
}

bool SyncManagerImpl::PurgeDisabledTypes(
    ModelTypeSet previously_enabled_types,
    ModelTypeSet currently_enabled_types,
    ModelTypeSet failed_types) {
  ModelTypeSet disabled_types = Difference(previously_enabled_types,
                                           currently_enabled_types);
  if (disabled_types.Empty())
    return true;

  DVLOG(1) << "Purging disabled types "
           << ModelTypeSetToString(disabled_types);
  return directory()->PurgeEntriesWithTypeIn(disabled_types, failed_types);
}

void SyncManagerImpl::UpdateCredentials(const SyncCredentials& credentials) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  DCHECK(!credentials.email.empty());
  DCHECK(!credentials.sync_token.empty());

  observing_network_connectivity_changes_ = true;
  if (!connection_manager_->set_auth_token(credentials.sync_token))
    return;  // Auth token is known to be invalid, so exit early.

  invalidator_->UpdateCredentials(credentials.email, credentials.sync_token);
  scheduler_->OnCredentialsUpdated();
}

void SyncManagerImpl::UpdateEnabledTypes(ModelTypeSet enabled_types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  invalidator_->UpdateRegisteredIds(
      this,
      ModelTypeSetToObjectIdSet(enabled_types));
}

void SyncManagerImpl::RegisterInvalidationHandler(
    InvalidationHandler* handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  invalidator_->RegisterHandler(handler);
}

void SyncManagerImpl::UpdateRegisteredInvalidationIds(
    InvalidationHandler* handler,
    const ObjectIdSet& ids) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  invalidator_->UpdateRegisteredIds(handler, ids);
}

void SyncManagerImpl::UnregisterInvalidationHandler(
    InvalidationHandler* handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  invalidator_->UnregisterHandler(handler);
}

void SyncManagerImpl::AddObserver(SyncManager::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void SyncManagerImpl::RemoveObserver(SyncManager::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void SyncManagerImpl::StopSyncingForShutdown(const base::Closure& callback) {
  DVLOG(2) << "StopSyncingForShutdown";
  scheduler_->RequestStop(callback);
  if (connection_manager_.get())
    connection_manager_->TerminateAllIO();
}

void SyncManagerImpl::ShutdownOnSyncThread() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Prevent any in-flight method calls from running.  Also
  // invalidates |weak_handle_this_| and |change_observer_|.
  weak_ptr_factory_.InvalidateWeakPtrs();
  js_mutation_event_observer_.InvalidateWeakPtrs();

  scheduler_.reset();
  session_context_.reset();

  if (sync_encryption_handler_.get()) {
    sync_encryption_handler_->RemoveObserver(&debug_info_event_listener_);
    sync_encryption_handler_->RemoveObserver(this);
  }

  SetJsEventHandler(WeakHandle<JsEventHandler>());
  RemoveObserver(&js_sync_manager_observer_);

  RemoveObserver(&debug_info_event_listener_);

  // |invalidator_| and |connection_manager_| may end up being NULL here in
  // tests (in synchronous initialization mode).
  //
  // TODO(akalin): Fix this behavior.

  if (invalidator_.get())
    invalidator_->UnregisterHandler(this);
  invalidator_.reset();

  if (connection_manager_.get())
    connection_manager_->RemoveListener(this);
  connection_manager_.reset();

  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  observing_network_connectivity_changes_ = false;

  if (initialized_ && directory()) {
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

void SyncManagerImpl::OnIPAddressChanged() {
  if (!observing_network_connectivity_changes_) {
    DVLOG(1) << "IP address change dropped.";
    return;
  }
  DVLOG(1) << "IP address change detected.";
  OnNetworkConnectivityChangedImpl();
}

void SyncManagerImpl::OnConnectionTypeChanged(
  net::NetworkChangeNotifier::ConnectionType) {
  if (!observing_network_connectivity_changes_) {
    DVLOG(1) << "Connection type change dropped.";
    return;
  }
  DVLOG(1) << "Connection type change detected.";
  OnNetworkConnectivityChangedImpl();
}

void SyncManagerImpl::OnNetworkConnectivityChangedImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  scheduler_->OnConnectionStatusChange();
}

void SyncManagerImpl::OnServerConnectionEvent(
    const ServerConnectionEvent& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (event.connection_code ==
      HttpResponse::SERVER_CONNECTION_OK) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnConnectionStatusChange(CONNECTION_OK));
  }

  if (event.connection_code == HttpResponse::SYNC_AUTH_ERROR) {
    observing_network_connectivity_changes_ = false;
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnConnectionStatusChange(CONNECTION_AUTH_ERROR));
  }

  if (event.connection_code == HttpResponse::SYNC_SERVER_ERROR) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnConnectionStatusChange(CONNECTION_SERVER_ERROR));
  }
}

void SyncManagerImpl::HandleTransactionCompleteChangeEvent(
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
        FROM_HERE,
        &SyncManager::ChangeObserver::OnChangesComplete,
        it.Get());
  }
}

ModelTypeSet
SyncManagerImpl::HandleTransactionEndingChangeEvent(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    syncable::BaseTransaction* trans) {
  // This notification happens immediately before a syncable WriteTransaction
  // falls out of scope. It happens while the channel mutex is still held,
  // and while the transaction mutex is held, so it cannot be re-entrant.
  if (!change_delegate_ || change_records_.empty())
    return ModelTypeSet();

  // This will continue the WriteTransaction using a read only wrapper.
  // This is the last chance for read to occur in the WriteTransaction
  // that's closing. This special ReadTransaction will not close the
  // underlying transaction.
  ReadTransaction read_trans(GetUserShare(), trans);

  ModelTypeSet models_with_changes;
  for (ChangeRecordMap::const_iterator it = change_records_.begin();
      it != change_records_.end(); ++it) {
    DCHECK(!it->second.Get().empty());
    ModelType type = ModelTypeFromInt(it->first);
    change_delegate_->
        OnChangesApplied(type, trans->directory()->GetTransactionVersion(type),
                         &read_trans, it->second);
    change_observer_.Call(FROM_HERE,
        &SyncManager::ChangeObserver::OnChangesApplied,
        type, write_transaction_info.Get().id, it->second);
    models_with_changes.Put(type);
  }
  change_records_.clear();
  return models_with_changes;
}

void SyncManagerImpl::HandleCalculateChangesChangeEventFromSyncApi(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    syncable::BaseTransaction* trans,
    std::vector<int64>* entries_changed) {
  // We have been notified about a user action changing a sync model.
  LOG_IF(WARNING, !change_records_.empty()) <<
      "CALCULATE_CHANGES called with unapplied old changes.";

  // The mutated model type, or UNSPECIFIED if nothing was mutated.
  ModelTypeSet mutated_model_types;

  const syncable::ImmutableEntryKernelMutationMap& mutations =
      write_transaction_info.Get().mutations;
  for (syncable::EntryKernelMutationMap::const_iterator it =
           mutations.Get().begin(); it != mutations.Get().end(); ++it) {
    if (!it->second.mutated.ref(syncable::IS_UNSYNCED)) {
      continue;
    }

    ModelType model_type =
        GetModelTypeFromSpecifics(it->second.mutated.ref(SPECIFICS));
    if (model_type < FIRST_REAL_MODEL_TYPE) {
      NOTREACHED() << "Permanent or underspecified item changed via syncapi.";
      continue;
    }

    // Found real mutation.
    if (model_type != UNSPECIFIED) {
      mutated_model_types.Put(model_type);
      entries_changed->push_back(it->second.mutated.ref(syncable::META_HANDLE));
    }
  }

  // Nudge if necessary.
  if (!mutated_model_types.Empty()) {
    if (weak_handle_this_.IsInitialized()) {
      weak_handle_this_.Call(FROM_HERE,
                             &SyncManagerImpl::RequestNudgeForDataTypes,
                             FROM_HERE,
                             mutated_model_types);
    } else {
      NOTREACHED();
    }
  }
}

void SyncManagerImpl::SetExtraChangeRecordData(int64 id,
    ModelType type, ChangeReorderBuffer* buffer,
    Cryptographer* cryptographer, const syncable::EntryKernel& original,
    bool existed_before, bool exists_now) {
  // If this is a deletion and the datatype was encrypted, we need to decrypt it
  // and attach it to the buffer.
  if (!exists_now && existed_before) {
    sync_pb::EntitySpecifics original_specifics(original.ref(SPECIFICS));
    if (type == PASSWORDS) {
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

void SyncManagerImpl::HandleCalculateChangesChangeEventFromSyncer(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    syncable::BaseTransaction* trans,
    std::vector<int64>* entries_changed) {
  // We only expect one notification per sync step, so change_buffers_ should
  // contain no pending entries.
  LOG_IF(WARNING, !change_records_.empty()) <<
      "CALCULATE_CHANGES called with unapplied old changes.";

  ChangeReorderBuffer change_buffers[MODEL_TYPE_COUNT];

  Cryptographer* crypto = directory()->GetCryptographer(trans);
  const syncable::ImmutableEntryKernelMutationMap& mutations =
      write_transaction_info.Get().mutations;
  for (syncable::EntryKernelMutationMap::const_iterator it =
           mutations.Get().begin(); it != mutations.Get().end(); ++it) {
    bool existed_before = !it->second.original.ref(syncable::IS_DEL);
    bool exists_now = !it->second.mutated.ref(syncable::IS_DEL);

    // Omit items that aren't associated with a model.
    ModelType type =
        GetModelTypeFromSpecifics(it->second.mutated.ref(SPECIFICS));
    if (type < FIRST_REAL_MODEL_TYPE)
      continue;

    int64 handle = it->first;
    if (exists_now && !existed_before)
      change_buffers[type].PushAddedItem(handle);
    else if (!exists_now && existed_before)
      change_buffers[type].PushDeletedItem(handle);
    else if (exists_now && existed_before &&
             VisiblePropertiesDiffer(it->second, crypto)) {
      change_buffers[type].PushUpdatedItem(
          handle, VisiblePositionsDiffer(it->second));
    }

    SetExtraChangeRecordData(handle, type, &change_buffers[type], crypto,
                             it->second.original, existed_before, exists_now);
  }

  ReadTransaction read_trans(GetUserShare(), trans);
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    if (!change_buffers[i].IsEmpty()) {
      if (change_buffers[i].GetAllChangesInTreeOrder(&read_trans,
                                                     &(change_records_[i]))) {
        for (size_t j = 0; j < change_records_[i].Get().size(); ++j)
          entries_changed->push_back((change_records_[i].Get())[j].id);
      }
      if (change_records_[i].Get().empty())
        change_records_.erase(i);
    }
  }
}

TimeDelta SyncManagerImpl::GetNudgeDelayTimeDelta(
    const ModelType& model_type) {
  return NudgeStrategy::GetNudgeDelayTimeDelta(model_type, this);
}

void SyncManagerImpl::RequestNudgeForDataTypes(
    const tracked_objects::Location& nudge_location,
    ModelTypeSet types) {
  debug_info_event_listener_.OnNudgeFromDatatype(types.First().Get());

  // TODO(lipalani) : Calculate the nudge delay based on all types.
  base::TimeDelta nudge_delay = NudgeStrategy::GetNudgeDelayTimeDelta(
      types.First().Get(),
      this);
  allstatus_.IncrementNudgeCounter(NUDGE_SOURCE_LOCAL);
  scheduler_->ScheduleNudgeAsync(nudge_delay,
                                 NUDGE_SOURCE_LOCAL,
                                 types,
                                 nudge_location);
}

void SyncManagerImpl::OnSyncEngineEvent(const SyncEngineEvent& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Only send an event if this is due to a cycle ending and this cycle
  // concludes a canonical "sync" process; that is, based on what is known
  // locally we are "all happy" and up-to-date.  There may be new changes on
  // the server, but we'll get them on a subsequent sync.
  //
  // Notifications are sent at the end of every sync cycle, regardless of
  // whether we should sync again.
  if (event.what_happened == SyncEngineEvent::SYNC_CYCLE_ENDED) {
    if (!initialized_) {
      LOG(INFO) << "OnSyncCycleCompleted not sent because sync api is not "
                << "initialized";
      return;
    }

    DVLOG(1) << "Sending OnSyncCycleCompleted";
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnSyncCycleCompleted(event.snapshot));

    // This is here for tests, which are still using p2p notifications.
    bool is_notifiable_commit =
        (event.snapshot.model_neutral_state().num_successful_commits > 0);
    if (is_notifiable_commit) {
      if (invalidator_.get()) {
        const ObjectIdInvalidationMap& invalidation_map =
            ModelTypeInvalidationMapToObjectIdInvalidationMap(
                event.snapshot.source().types);
        invalidator_->SendInvalidation(invalidation_map);
      } else {
        DVLOG(1) << "Not sending invalidation: invalidator_ is NULL";
      }
    }
  }

  if (event.what_happened == SyncEngineEvent::STOP_SYNCING_PERMANENTLY) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnStopSyncingPermanently());
    return;
  }

  if (event.what_happened == SyncEngineEvent::UPDATED_TOKEN) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnUpdatedToken(event.updated_token));
    return;
  }

  if (event.what_happened == SyncEngineEvent::ACTIONABLE_ERROR) {
    FOR_EACH_OBSERVER(
        SyncManager::Observer, observers_,
        OnActionableError(
            event.snapshot.model_neutral_state().sync_protocol_error));
    return;
  }
}

void SyncManagerImpl::SetJsEventHandler(
    const WeakHandle<JsEventHandler>& event_handler) {
  js_event_handler_ = event_handler;
  js_sync_manager_observer_.SetJsEventHandler(js_event_handler_);
  js_mutation_event_observer_.SetJsEventHandler(js_event_handler_);
  js_sync_encryption_handler_observer_.SetJsEventHandler(js_event_handler_);
}

void SyncManagerImpl::ProcessJsMessage(
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

void SyncManagerImpl::BindJsMessageHandler(
    const std::string& name,
    UnboundJsMessageHandler unbound_message_handler) {
  js_message_handlers_[name] =
      base::Bind(unbound_message_handler, base::Unretained(this));
}

base::DictionaryValue* SyncManagerImpl::NotificationInfoToValue(
    const NotificationInfoMap& notification_info) {
  base::DictionaryValue* value = new base::DictionaryValue();

  for (NotificationInfoMap::const_iterator it = notification_info.begin();
      it != notification_info.end(); ++it) {
    const std::string& model_type_str = ModelTypeToString(it->first);
    value->Set(model_type_str, it->second.ToValue());
  }

  return value;
}

std::string SyncManagerImpl::NotificationInfoToString(
    const NotificationInfoMap& notification_info) {
  scoped_ptr<base::DictionaryValue> value(
      NotificationInfoToValue(notification_info));
  std::string str;
  base::JSONWriter::Write(value.get(), &str);
  return str;
}

JsArgList SyncManagerImpl::GetNotificationState(
    const JsArgList& args) {
  const std::string& notification_state =
      InvalidatorStateToString(invalidator_state_);
  DVLOG(1) << "GetNotificationState: " << notification_state;
  base::ListValue return_args;
  return_args.Append(new base::StringValue(notification_state));
  return JsArgList(&return_args);
}

JsArgList SyncManagerImpl::GetNotificationInfo(
    const JsArgList& args) {
  DVLOG(1) << "GetNotificationInfo: "
           << NotificationInfoToString(notification_info_map_);
  base::ListValue return_args;
  return_args.Append(NotificationInfoToValue(notification_info_map_));
  return JsArgList(&return_args);
}

JsArgList SyncManagerImpl::GetRootNodeDetails(
    const JsArgList& args) {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  ReadNode root(&trans);
  root.InitByRootLookup();
  base::ListValue return_args;
  return_args.Append(root.GetDetailsAsValue());
  return JsArgList(&return_args);
}

JsArgList SyncManagerImpl::GetClientServerTraffic(
    const JsArgList& args) {
  base::ListValue return_args;
  base::ListValue* value = traffic_recorder_.ToValue();
  if (value != NULL)
    return_args.Append(value);
  return JsArgList(&return_args);
}

namespace {

int64 GetId(const base::ListValue& ids, int i) {
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

JsArgList GetNodeInfoById(
    const JsArgList& args,
    UserShare* user_share,
    base::DictionaryValue* (BaseNode::*info_getter)() const) {
  CHECK(info_getter);
  base::ListValue return_args;
  base::ListValue* node_summaries = new base::ListValue();
  return_args.Append(node_summaries);
  const base::ListValue* id_list = NULL;
  ReadTransaction trans(FROM_HERE, user_share);
  if (args.Get().GetList(0, &id_list)) {
    CHECK(id_list);
    for (size_t i = 0; i < id_list->GetSize(); ++i) {
      int64 id = GetId(*id_list, i);
      if (id == kInvalidId) {
        continue;
      }
      ReadNode node(&trans);
      if (node.InitByIdLookup(id) != BaseNode::INIT_OK) {
        continue;
      }
      node_summaries->Append((node.*info_getter)());
    }
  }
  return JsArgList(&return_args);
}

}  // namespace

JsArgList SyncManagerImpl::GetNodeSummariesById(const JsArgList& args) {
  return GetNodeInfoById(args, GetUserShare(), &BaseNode::GetSummaryAsValue);
}

JsArgList SyncManagerImpl::GetNodeDetailsById(const JsArgList& args) {
  return GetNodeInfoById(args, GetUserShare(), &BaseNode::GetDetailsAsValue);
}

JsArgList SyncManagerImpl::GetAllNodes(const JsArgList& args) {
  base::ListValue return_args;
  base::ListValue* result = new base::ListValue();
  return_args.Append(result);

  ReadTransaction trans(FROM_HERE, GetUserShare());
  std::vector<const syncable::EntryKernel*> entry_kernels;
  trans.GetDirectory()->GetAllEntryKernels(trans.GetWrappedTrans(),
                                           &entry_kernels);

  for (std::vector<const syncable::EntryKernel*>::const_iterator it =
           entry_kernels.begin(); it != entry_kernels.end(); ++it) {
    result->Append((*it)->ToValue(trans.GetCryptographer()));
  }

  return JsArgList(&return_args);
}

JsArgList SyncManagerImpl::GetChildNodeIds(const JsArgList& args) {
  base::ListValue return_args;
  base::ListValue* child_ids = new base::ListValue();
  return_args.Append(child_ids);
  int64 id = GetId(args.Get(), 0);
  if (id != kInvalidId) {
    ReadTransaction trans(FROM_HERE, GetUserShare());
    syncable::Directory::ChildHandles child_handles;
    trans.GetDirectory()->GetChildHandlesByHandle(trans.GetWrappedTrans(),
                                                  id, &child_handles);
    for (syncable::Directory::ChildHandles::const_iterator it =
             child_handles.begin(); it != child_handles.end(); ++it) {
      child_ids->Append(new base::StringValue(base::Int64ToString(*it)));
    }
  }
  return JsArgList(&return_args);
}

void SyncManagerImpl::UpdateNotificationInfo(
    const ModelTypeInvalidationMap& invalidation_map) {
  for (ModelTypeInvalidationMap::const_iterator it = invalidation_map.begin();
       it != invalidation_map.end(); ++it) {
    NotificationInfo* info = &notification_info_map_[it->first];
    info->total_count++;
    info->payload = it->second.payload;
  }
}

void SyncManagerImpl::OnInvalidatorStateChange(InvalidatorState state) {
  const std::string& state_str = InvalidatorStateToString(state);
  invalidator_state_ = state;
  DVLOG(1) << "Invalidator state changed to: " << state_str;
  const bool notifications_enabled =
      (invalidator_state_ == INVALIDATIONS_ENABLED);
  allstatus_.SetNotificationsEnabled(notifications_enabled);
  scheduler_->SetNotificationsEnabled(notifications_enabled);

  if (invalidator_state_ == syncer::INVALIDATION_CREDENTIALS_REJECTED) {
    // If the invalidator's credentials were rejected, that means that
    // our sync credentials are also bad, so invalidate those.
    connection_manager_->OnInvalidationCredentialsRejected();
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnConnectionStatusChange(CONNECTION_AUTH_ERROR));
  }

  if (js_event_handler_.IsInitialized()) {
    base::DictionaryValue details;
    details.SetString("state", state_str);
    js_event_handler_.Call(FROM_HERE,
                           &JsEventHandler::HandleJsEvent,
                           "onNotificationStateChange",
                           JsEventDetails(&details));
  }
}

void SyncManagerImpl::OnIncomingInvalidation(
    const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const ModelTypeInvalidationMap& type_invalidation_map =
      ObjectIdInvalidationMapToModelTypeInvalidationMap(invalidation_map);
  if (type_invalidation_map.empty()) {
    LOG(WARNING) << "Sync received invalidation without any type information.";
  } else {
    allstatus_.IncrementNudgeCounter(NUDGE_SOURCE_NOTIFICATION);
    scheduler_->ScheduleNudgeWithStatesAsync(
        TimeDelta::FromMilliseconds(kSyncSchedulerDelayMsec),
        NUDGE_SOURCE_NOTIFICATION,
        type_invalidation_map, FROM_HERE);
    allstatus_.IncrementNotificationsReceived();
    UpdateNotificationInfo(type_invalidation_map);
    debug_info_event_listener_.OnIncomingNotification(type_invalidation_map);
  }

  if (js_event_handler_.IsInitialized()) {
    base::DictionaryValue details;
    base::ListValue* changed_types = new base::ListValue();
    details.Set("changedTypes", changed_types);
    for (ModelTypeInvalidationMap::const_iterator it =
             type_invalidation_map.begin(); it != type_invalidation_map.end();
         ++it) {
      const std::string& model_type_str =
          ModelTypeToString(it->first);
      changed_types->Append(new base::StringValue(model_type_str));
    }
    details.SetString("source", "REMOTE_INVALIDATION");
    js_event_handler_.Call(FROM_HERE,
                           &JsEventHandler::HandleJsEvent,
                           "onIncomingNotification",
                           JsEventDetails(&details));
  }
}

void SyncManagerImpl::RefreshTypes(ModelTypeSet types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const ModelTypeInvalidationMap& type_invalidation_map =
      ModelTypeSetToInvalidationMap(types, "");
  if (type_invalidation_map.empty()) {
    LOG(WARNING) << "Sync received refresh request with no types specified.";
  } else {
    allstatus_.IncrementNudgeCounter(NUDGE_SOURCE_LOCAL_REFRESH);
    scheduler_->ScheduleNudgeWithStatesAsync(
        TimeDelta::FromMilliseconds(kSyncRefreshDelayMsec),
        NUDGE_SOURCE_LOCAL_REFRESH,
        type_invalidation_map, FROM_HERE);
  }

  if (js_event_handler_.IsInitialized()) {
    base::DictionaryValue details;
    base::ListValue* changed_types = new base::ListValue();
    details.Set("changedTypes", changed_types);
    for (ModelTypeInvalidationMap::const_iterator it =
             type_invalidation_map.begin(); it != type_invalidation_map.end();
         ++it) {
      const std::string& model_type_str =
          ModelTypeToString(it->first);
      changed_types->Append(new base::StringValue(model_type_str));
    }
    details.SetString("source", "LOCAL_INVALIDATION");
    js_event_handler_.Call(FROM_HERE,
                           &JsEventHandler::HandleJsEvent,
                           "onIncomingNotification",
                           JsEventDetails(&details));
  }
}

SyncStatus SyncManagerImpl::GetDetailedStatus() const {
  return allstatus_.status();
}

void SyncManagerImpl::SaveChanges() {
  directory()->SaveChanges();
}

UserShare* SyncManagerImpl::GetUserShare() {
  DCHECK(initialized_);
  return &share_;
}

const std::string SyncManagerImpl::cache_guid() {
  DCHECK(initialized_);
  return directory()->cache_guid();
}

bool SyncManagerImpl::ReceivedExperiment(Experiments* experiments) {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  ReadNode nigori_node(&trans);
  if (nigori_node.InitByTagLookup(kNigoriTag) != BaseNode::INIT_OK) {
    DVLOG(1) << "Couldn't find Nigori node.";
    return false;
  }
  bool found_experiment = false;
  if (nigori_node.GetNigoriSpecifics().sync_tab_favicons()) {
    experiments->sync_tab_favicons = true;
    found_experiment = true;
  }

  ReadNode keystore_node(&trans);
  if (keystore_node.InitByClientTagLookup(
          syncer::EXPERIMENTS,
          syncer::kKeystoreEncryptionTag) == BaseNode::INIT_OK &&
      keystore_node.GetExperimentsSpecifics().keystore_encryption().enabled()) {
    experiments->keystore_encryption = true;
    found_experiment = true;
  }

  ReadNode autofill_culling_node(&trans);
  if (autofill_culling_node.InitByClientTagLookup(
          syncer::EXPERIMENTS,
          syncer::kAutofillCullingTag) == BaseNode::INIT_OK &&
      autofill_culling_node.GetExperimentsSpecifics().
          autofill_culling().enabled()) {
    experiments->autofill_culling = true;
    found_experiment = true;
  }

  ReadNode full_history_sync_node(&trans);
  if (full_history_sync_node.InitByClientTagLookup(
          syncer::EXPERIMENTS,
          syncer::kFullHistorySyncTag) == BaseNode::INIT_OK &&
      full_history_sync_node.GetExperimentsSpecifics().
          history_delete_directives().enabled()) {
    experiments->full_history_sync = true;
    found_experiment = true;
  }

  return found_experiment;
}

bool SyncManagerImpl::HasUnsyncedItems() {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  return (trans.GetWrappedTrans()->directory()->unsynced_entity_count() != 0);
}

SyncEncryptionHandler* SyncManagerImpl::GetEncryptionHandler() {
  return sync_encryption_handler_.get();
}

// static.
int SyncManagerImpl::GetDefaultNudgeDelay() {
  return kDefaultNudgeDelayMilliseconds;
}

// static.
int SyncManagerImpl::GetPreferencesNudgeDelay() {
  return kPreferencesNudgeDelayMilliseconds;
}

}  // namespace syncer
