// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_manager_impl.h"

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "sync/engine/sync_scheduler.h"
#include "sync/engine/syncer_types.h"
#include "sync/internal_api/change_reorder_buffer.h"
#include "sync/internal_api/public/base/cancelation_signal.h"
#include "sync/internal_api/public/base/invalidation_interface.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base_node.h"
#include "sync/internal_api/public/configure_reason.h"
#include "sync/internal_api/public/engine/polling_constants.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/internal_components_factory.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/sync_context.h"
#include "sync/internal_api/public/sync_context_proxy.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/internal_api/public/util/experiments.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/internal_api/sync_context_proxy_impl.h"
#include "sync/internal_api/syncapi_internal.h"
#include "sync/internal_api/syncapi_server_connection_manager.h"
#include "sync/protocol/proto_value_conversions.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/directory_type_debug_info_emitter.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/in_memory_directory_backing_store.h"
#include "sync/syncable/on_disk_directory_backing_store.h"

using base::TimeDelta;
using sync_pb::GetUpdatesCallerInfo;

class GURL;

namespace syncer {

using sessions::SyncSessionContext;
using syncable::ImmutableWriteTransactionInfo;
using syncable::SPECIFICS;
using syncable::UNIQUE_POSITION;

namespace {

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
    case CONFIGURE_REASON_CRYPTO:
      return GetUpdatesCallerInfo::NEWLY_SUPPORTED_DATATYPE;
    case CONFIGURE_REASON_PROGRAMMATIC:
      return GetUpdatesCallerInfo::PROGRAMMATIC;
    default:
      NOTREACHED();
  }
  return GetUpdatesCallerInfo::UNKNOWN;
}

}  // namespace

SyncManagerImpl::SyncManagerImpl(const std::string& name)
    : name_(name),
      change_delegate_(NULL),
      initialized_(false),
      observing_network_connectivity_changes_(false),
      report_unrecoverable_error_function_(NULL),
      weak_ptr_factory_(this) {
  // Pre-fill |notification_info_map_|.
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    notification_info_map_.insert(
        std::make_pair(ModelTypeFromInt(i), NotificationInfo()));
  }
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
  if (!b.ShouldMaintainPosition())
    return false;
  if (!a.ref(UNIQUE_POSITION).Equals(b.ref(UNIQUE_POSITION)))
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
  if (!AreAttachmentMetadataEqual(a.ref(syncable::ATTACHMENT_METADATA),
                                  b.ref(syncable::ATTACHMENT_METADATA))) {
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
    ModelTypeSet to_download,
    ModelTypeSet to_purge,
    ModelTypeSet to_journal,
    ModelTypeSet to_unapply,
    const ModelSafeRoutingInfo& new_routing_info,
    const base::Closure& ready_task,
    const base::Closure& retry_task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!ready_task.is_null());
  DCHECK(!retry_task.is_null());
  DCHECK(initialized_);

  DVLOG(1) << "Configuring -"
           << "\n\t" << "current types: "
           << ModelTypeSetToString(GetRoutingInfoTypes(new_routing_info))
           << "\n\t" << "types to download: "
           << ModelTypeSetToString(to_download)
           << "\n\t" << "types to purge: "
           << ModelTypeSetToString(to_purge)
           << "\n\t" << "types to journal: "
           << ModelTypeSetToString(to_journal)
           << "\n\t" << "types to unapply: "
           << ModelTypeSetToString(to_unapply);
  if (!PurgeDisabledTypes(to_purge,
                          to_journal,
                          to_unapply)) {
    // We failed to cleanup the types. Invoke the ready task without actually
    // configuring any types. The caller should detect this as a configuration
    // failure and act appropriately.
    ready_task.Run();
    return;
  }

  ConfigurationParams params(GetSourceFromReason(reason),
                             to_download,
                             new_routing_info,
                             ready_task,
                             retry_task);

  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE);
  scheduler_->ScheduleConfiguration(params);
}

void SyncManagerImpl::Init(InitArgs* args) {
  CHECK(!initialized_);
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(args->post_factory.get());
  DCHECK(!args->credentials.email.empty());
  DCHECK(!args->credentials.sync_token.empty());
  DCHECK(!args->credentials.scope_set.empty());
  DCHECK(args->cancelation_signal);
  DVLOG(1) << "SyncManager starting Init...";

  weak_handle_this_ = MakeWeakHandle(weak_ptr_factory_.GetWeakPtr());

  change_delegate_ = args->change_delegate;

  AddObserver(&js_sync_manager_observer_);
  SetJsEventHandler(args->event_handler);

  AddObserver(&debug_info_event_listener_);

  database_path_ = args->database_location.Append(
      syncable::Directory::kSyncDatabaseFilename);
  unrecoverable_error_handler_ = args->unrecoverable_error_handler.Pass();
  report_unrecoverable_error_function_ =
      args->report_unrecoverable_error_function;

  allstatus_.SetHasKeystoreKey(
      !args->restored_keystore_key_for_bootstrapping.empty());
  sync_encryption_handler_.reset(new SyncEncryptionHandlerImpl(
      &share_,
      args->encryptor,
      args->restored_key_for_bootstrapping,
      args->restored_keystore_key_for_bootstrapping));
  sync_encryption_handler_->AddObserver(this);
  sync_encryption_handler_->AddObserver(&debug_info_event_listener_);
  sync_encryption_handler_->AddObserver(&js_sync_encryption_handler_observer_);

  base::FilePath absolute_db_path = database_path_;
  DCHECK(absolute_db_path.IsAbsolute());

  scoped_ptr<syncable::DirectoryBackingStore> backing_store =
      args->internal_components_factory->BuildDirectoryBackingStore(
          InternalComponentsFactory::STORAGE_ON_DISK,
          args->credentials.email, absolute_db_path).Pass();

  DCHECK(backing_store.get());
  share_.directory.reset(
      new syncable::Directory(
          backing_store.release(),
          unrecoverable_error_handler_.get(),
          report_unrecoverable_error_function_,
          sync_encryption_handler_.get(),
          sync_encryption_handler_->GetCryptographerUnsafe()));
  share_.sync_credentials = args->credentials;

  // UserShare is accessible to a lot of code that doesn't need access to the
  // sync token so clear sync_token from the UserShare.
  share_.sync_credentials.sync_token = "";

  const std::string& username = args->credentials.email;
  DVLOG(1) << "Username: " << username;
  if (!OpenDirectory(username)) {
    NotifyInitializationFailure();
    LOG(ERROR) << "Sync manager initialization failed!";
    return;
  }

  connection_manager_.reset(new SyncAPIServerConnectionManager(
      args->service_url.host() + args->service_url.path(),
      args->service_url.EffectiveIntPort(),
      args->service_url.SchemeIsSecure(),
      args->post_factory.release(),
      args->cancelation_signal));
  connection_manager_->set_client_id(directory()->cache_guid());
  connection_manager_->AddListener(this);

  std::string sync_id = directory()->cache_guid();

  DVLOG(1) << "Setting sync client ID: " << sync_id;
  allstatus_.SetSyncId(sync_id);
  DVLOG(1) << "Setting invalidator client ID: " << args->invalidator_client_id;
  allstatus_.SetInvalidatorClientId(args->invalidator_client_id);

  model_type_registry_.reset(
      new ModelTypeRegistry(args->workers, directory(), this));
  sync_encryption_handler_->AddObserver(model_type_registry_.get());

  // Bind the SyncContext WeakPtr to this thread.  This helps us crash earlier
  // if the pointer is misused in debug mode.
  base::WeakPtr<SyncContext> weak_core = model_type_registry_->AsWeakPtr();
  weak_core.get();

  sync_context_proxy_.reset(
      new SyncContextProxyImpl(base::ThreadTaskRunnerHandle::Get(), weak_core));

  // Build a SyncSessionContext and store the worker in it.
  DVLOG(1) << "Sync is bringing up SyncSessionContext.";
  std::vector<SyncEngineEventListener*> listeners;
  listeners.push_back(&allstatus_);
  listeners.push_back(this);
  session_context_ =
      args->internal_components_factory->BuildContext(
                                             connection_manager_.get(),
                                             directory(),
                                             args->extensions_activity,
                                             listeners,
                                             &debug_info_event_listener_,
                                             model_type_registry_.get(),
                                             args->invalidator_client_id)
          .Pass();
  session_context_->set_account_name(args->credentials.email);
  scheduler_ = args->internal_components_factory->BuildScheduler(
      name_, session_context_.get(), args->cancelation_signal).Pass();

  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE);

  initialized_ = true;

  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  observing_network_connectivity_changes_ = true;

  UpdateCredentials(args->credentials);

  NotifyInitializationSuccess();
}

void SyncManagerImpl::NotifyInitializationSuccess() {
  FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                    OnInitializationComplete(
                        MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()),
                        MakeWeakHandle(debug_info_event_listener_.GetWeakPtr()),
                        true, InitialSyncEndedTypes()));
}

void SyncManagerImpl::NotifyInitializationFailure() {
  FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                    OnInitializationComplete(
                        MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()),
                        MakeWeakHandle(debug_info_event_listener_.GetWeakPtr()),
                        false, ModelTypeSet()));
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
  session_context_->SetRoutingInfo(routing_info);
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
                                             ModelTypeSet(),
                                             ModelTypeSet());
}

bool SyncManagerImpl::PurgeDisabledTypes(
    ModelTypeSet to_purge,
    ModelTypeSet to_journal,
    ModelTypeSet to_unapply) {
  if (to_purge.Empty())
    return true;
  DVLOG(1) << "Purging disabled types " << ModelTypeSetToString(to_purge);
  DCHECK(to_purge.HasAll(to_journal));
  DCHECK(to_purge.HasAll(to_unapply));
  return directory()->PurgeEntriesWithTypeIn(to_purge, to_journal, to_unapply);
}

void SyncManagerImpl::UpdateCredentials(const SyncCredentials& credentials) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  DCHECK(!credentials.email.empty());
  DCHECK(!credentials.sync_token.empty());
  DCHECK(!credentials.scope_set.empty());

  observing_network_connectivity_changes_ = true;
  if (!connection_manager_->SetAuthToken(credentials.sync_token))
    return;  // Auth token is known to be invalid, so exit early.

  scheduler_->OnCredentialsUpdated();

  // TODO(zea): pass the credential age to the debug info event listener.
}

void SyncManagerImpl::AddObserver(SyncManager::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void SyncManagerImpl::RemoveObserver(SyncManager::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void SyncManagerImpl::ShutdownOnSyncThread(ShutdownReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Prevent any in-flight method calls from running.  Also
  // invalidates |weak_handle_this_| and |change_observer_|.
  weak_ptr_factory_.InvalidateWeakPtrs();
  js_mutation_event_observer_.InvalidateWeakPtrs();

  scheduler_.reset();
  session_context_.reset();

  if (model_type_registry_)
    sync_encryption_handler_->RemoveObserver(model_type_registry_.get());

  model_type_registry_.reset();

  if (sync_encryption_handler_) {
    sync_encryption_handler_->RemoveObserver(&debug_info_event_listener_);
    sync_encryption_handler_->RemoveObserver(this);
  }

  SetJsEventHandler(WeakHandle<JsEventHandler>());
  RemoveObserver(&js_sync_manager_observer_);

  RemoveObserver(&debug_info_event_listener_);

  // |connection_manager_| may end up being NULL here in tests (in synchronous
  // initialization mode).
  //
  // TODO(akalin): Fix this behavior.
  if (connection_manager_)
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
      if (!data) {
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
      change_buffers[type].PushUpdatedItem(handle);
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

void SyncManagerImpl::RequestNudgeForDataTypes(
    const tracked_objects::Location& nudge_location,
    ModelTypeSet types) {
  debug_info_event_listener_.OnNudgeFromDatatype(types.First().Get());

  scheduler_->ScheduleLocalNudge(types, nudge_location);
}

void SyncManagerImpl::NudgeForInitialDownload(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scheduler_->ScheduleInitialSyncNudge(type);
}

void SyncManagerImpl::NudgeForCommit(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RequestNudgeForDataTypes(FROM_HERE, ModelTypeSet(type));
}

void SyncManagerImpl::NudgeForRefresh(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RefreshTypes(ModelTypeSet(type));
}

void SyncManagerImpl::OnSyncCycleEvent(const SyncCycleEvent& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Only send an event if this is due to a cycle ending and this cycle
  // concludes a canonical "sync" process; that is, based on what is known
  // locally we are "all happy" and up-to-date.  There may be new changes on
  // the server, but we'll get them on a subsequent sync.
  //
  // Notifications are sent at the end of every sync cycle, regardless of
  // whether we should sync again.
  if (event.what_happened == SyncCycleEvent::SYNC_CYCLE_ENDED) {
    if (!initialized_) {
      DVLOG(1) << "OnSyncCycleCompleted not sent because sync api is not "
               << "initialized";
      return;
    }

    DVLOG(1) << "Sending OnSyncCycleCompleted";
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnSyncCycleCompleted(event.snapshot));
  }
}

void SyncManagerImpl::OnActionableError(const SyncProtocolError& error) {
  FOR_EACH_OBSERVER(
      SyncManager::Observer, observers_,
      OnActionableError(error));
}

void SyncManagerImpl::OnRetryTimeChanged(base::Time) {}

void SyncManagerImpl::OnThrottledTypesChanged(ModelTypeSet) {}

void SyncManagerImpl::OnMigrationRequested(ModelTypeSet types) {
  FOR_EACH_OBSERVER(
      SyncManager::Observer, observers_,
      OnMigrationRequested(types));
}

void SyncManagerImpl::OnProtocolEvent(const ProtocolEvent& event) {
  protocol_event_buffer_.RecordProtocolEvent(event);
  FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                    OnProtocolEvent(event));
}

void SyncManagerImpl::SetJsEventHandler(
    const WeakHandle<JsEventHandler>& event_handler) {
  js_sync_manager_observer_.SetJsEventHandler(event_handler);
  js_mutation_event_observer_.SetJsEventHandler(event_handler);
  js_sync_encryption_handler_observer_.SetJsEventHandler(event_handler);
}

scoped_ptr<base::ListValue> SyncManagerImpl::GetAllNodesForType(
    syncer::ModelType type) {
  DirectoryTypeDebugInfoEmitterMap* emitter_map =
      model_type_registry_->directory_type_debug_info_emitter_map();
  DirectoryTypeDebugInfoEmitterMap::iterator it = emitter_map->find(type);

  if (it == emitter_map->end()) {
    // This can happen in some cases.  The UI thread makes requests of us
    // when it doesn't really know which types are enabled or disabled.
    DLOG(WARNING) << "Asked to return debug info for invalid type "
                  << ModelTypeToString(type);
    return scoped_ptr<base::ListValue>(new base::ListValue());
  }

  return it->second->GetAllNodes();
}

void SyncManagerImpl::SetInvalidatorEnabled(bool invalidator_enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(1) << "Invalidator enabled state is now: " << invalidator_enabled;
  allstatus_.SetNotificationsEnabled(invalidator_enabled);
  scheduler_->SetNotificationsEnabled(invalidator_enabled);
}

void SyncManagerImpl::OnIncomingInvalidation(
    syncer::ModelType type,
    scoped_ptr<InvalidationInterface> invalidation) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scheduler_->ScheduleInvalidationNudge(
      type,
      invalidation.Pass(),
      FROM_HERE);
}

void SyncManagerImpl::RefreshTypes(ModelTypeSet types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (types.Empty()) {
    LOG(WARNING) << "Sync received refresh request with no types specified.";
  } else {
    scheduler_->ScheduleLocalRefreshRequest(
        types, FROM_HERE);
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

syncer::SyncContextProxy* SyncManagerImpl::GetSyncContextProxy() {
  DCHECK(initialized_);
  return sync_context_proxy_.get();
}

const std::string SyncManagerImpl::cache_guid() {
  DCHECK(initialized_);
  return directory()->cache_guid();
}

bool SyncManagerImpl::ReceivedExperiment(Experiments* experiments) {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  ReadNode nigori_node(&trans);
  if (nigori_node.InitTypeRoot(NIGORI) != BaseNode::INIT_OK) {
    DVLOG(1) << "Couldn't find Nigori node.";
    return false;
  }
  bool found_experiment = false;

  ReadNode favicon_sync_node(&trans);
  if (favicon_sync_node.InitByClientTagLookup(
          syncer::EXPERIMENTS,
          syncer::kFaviconSyncTag) == BaseNode::INIT_OK) {
    experiments->favicon_sync_limit =
        favicon_sync_node.GetExperimentsSpecifics().favicon_sync().
            favicon_sync_limit();
    found_experiment = true;
  }

  ReadNode pre_commit_update_avoidance_node(&trans);
  if (pre_commit_update_avoidance_node.InitByClientTagLookup(
          syncer::EXPERIMENTS,
          syncer::kPreCommitUpdateAvoidanceTag) == BaseNode::INIT_OK) {
    session_context_->set_server_enabled_pre_commit_update_avoidance(
        pre_commit_update_avoidance_node.GetExperimentsSpecifics().
            pre_commit_update_avoidance().enabled());
    // We don't bother setting found_experiment.  The frontend doesn't need to
    // know about this.
  }

  ReadNode gcm_channel_node(&trans);
  if (gcm_channel_node.InitByClientTagLookup(
          syncer::EXPERIMENTS,
          syncer::kGCMChannelTag) == BaseNode::INIT_OK &&
      gcm_channel_node.GetExperimentsSpecifics().gcm_channel().has_enabled()) {
    experiments->gcm_channel_state =
        (gcm_channel_node.GetExperimentsSpecifics().gcm_channel().enabled() ?
         syncer::Experiments::ENABLED : syncer::Experiments::SUPPRESSED);
    found_experiment = true;
  }

  ReadNode enhanced_bookmarks_node(&trans);
  if (enhanced_bookmarks_node.InitByClientTagLookup(
          syncer::EXPERIMENTS, syncer::kEnhancedBookmarksTag) ==
          BaseNode::INIT_OK &&
      enhanced_bookmarks_node.GetExperimentsSpecifics()
          .has_enhanced_bookmarks()) {
    const sync_pb::EnhancedBookmarksFlags& enhanced_bookmarks =
        enhanced_bookmarks_node.GetExperimentsSpecifics().enhanced_bookmarks();
    if (enhanced_bookmarks.has_enabled())
      experiments->enhanced_bookmarks_enabled = enhanced_bookmarks.enabled();
    if (enhanced_bookmarks.has_extension_id()) {
      experiments->enhanced_bookmarks_ext_id =
          enhanced_bookmarks.extension_id();
    }
    found_experiment = true;
  }

  ReadNode gcm_invalidations_node(&trans);
  if (gcm_invalidations_node.InitByClientTagLookup(
          syncer::EXPERIMENTS, syncer::kGCMInvalidationsTag) ==
      BaseNode::INIT_OK) {
    const sync_pb::GcmInvalidationsFlags& gcm_invalidations =
        gcm_invalidations_node.GetExperimentsSpecifics().gcm_invalidations();
    if (gcm_invalidations.has_enabled()) {
      experiments->gcm_invalidations_enabled = gcm_invalidations.enabled();
      found_experiment = true;
    }
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

ScopedVector<syncer::ProtocolEvent>
    SyncManagerImpl::GetBufferedProtocolEvents() {
  return protocol_event_buffer_.GetBufferedProtocolEvents();
}

void SyncManagerImpl::RegisterDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) {
  model_type_registry_->RegisterDirectoryTypeDebugInfoObserver(observer);
}

void SyncManagerImpl::UnregisterDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) {
  model_type_registry_->UnregisterDirectoryTypeDebugInfoObserver(observer);
}

bool SyncManagerImpl::HasDirectoryTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* observer) {
  return model_type_registry_->HasDirectoryTypeDebugInfoObserver(observer);
}

void SyncManagerImpl::RequestEmitDebugInfo() {
  model_type_registry_->RequestEmitDebugInfo();
}

}  // namespace syncer
