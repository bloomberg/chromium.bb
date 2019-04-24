// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_backend_host_core.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/sync/base/get_session_name.h"
#include "components/sync/base/invalidation_adapter.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/device_info/local_device_info_provider_impl.h"
#include "components/sync/engine/cycle/commit_counters.h"
#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/cycle/update_counters.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/sync_backend_registrar.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/engine/sync_manager_factory.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/user_share.h"

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncers involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

static const int kSaveChangesIntervalSeconds = 10;

namespace net {
class URLFetcher;
}

namespace syncer {

class EngineComponentsFactory;

namespace {

void BindFetcherToDataTracker(net::URLFetcher* fetcher) {
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher, data_use_measurement::DataUseUserData::SYNC);
}

void RecordPerModelTypeInvalidation(int model_type, bool is_grouped) {
  UMA_HISTOGRAM_ENUMERATION("Sync.InvalidationPerModelType", model_type,
                            static_cast<int>(syncer::MODEL_TYPE_COUNT));
  if (!is_grouped) {
    // When recording metrics it's important to distinguish between
    // many/one case, since "many" aka grouped case is only common in
    // the deprecated implementation.
    UMA_HISTOGRAM_ENUMERATION("Sync.NonGroupedInvalidation", model_type,
                              static_cast<int>(syncer::MODEL_TYPE_COUNT));
  }
}

}  // namespace

SyncBackendHostCore::SyncBackendHostCore(
    const std::string& name,
    const base::FilePath& sync_data_folder,
    const base::WeakPtr<SyncEngineImpl>& host)
    : name_(name),
      sync_data_folder_(sync_data_folder),
      host_(host),
      weak_ptr_factory_(this) {
  DCHECK(host);
  // This is constructed on the UI thread but used from the sync thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SyncBackendHostCore::~SyncBackendHostCore() {
  DCHECK(!sync_manager_);
}

bool SyncBackendHostCore::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sync_manager_)
    return false;
  sync_manager_->OnMemoryDump(pmd);
  return true;
}

void SyncBackendHostCore::OnSyncCycleCompleted(
    const SyncCycleSnapshot& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE, &SyncEngineImpl::HandleSyncCycleCompletedOnFrontendLoop,
             snapshot);
}

void SyncBackendHostCore::DoRefreshTypes(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->RefreshTypes(types);
}

void SyncBackendHostCore::OnInitializationComplete(
    const WeakHandle<JsBackend>& js_backend,
    const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
    bool success,
    const ModelTypeSet restored_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    DoDestroySyncManager();
    host_.Call(FROM_HERE,
               &SyncEngineImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  // Register for encryption related changes now. We have to do this before
  // the initializing downloading control types or initializing the encryption
  // handler in order to receive notifications triggered during encryption
  // startup.
  DCHECK(encryption_observer_proxy_);
  sync_manager_->GetEncryptionHandler()->AddObserver(
      encryption_observer_proxy_.get());

  // Sync manager initialization is complete, so we can schedule recurring
  // SaveChanges.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SyncBackendHostCore::StartSavingChanges,
                                weak_ptr_factory_.GetWeakPtr()));

  // Hang on to these for a while longer.  We're not ready to hand them back to
  // the UI thread yet.
  js_backend_ = js_backend;
  debug_info_listener_ = debug_info_listener;

  // Before proceeding any further, we need to download the control types and
  // purge any partial data (ie. data downloaded for a type that was on its way
  // to being initially synced, but didn't quite make it.).  The following
  // configure cycle will take care of this.  It depends on the registrar state
  // which we initialize below to ensure that we don't perform any downloads if
  // all control types have already completed their initial sync.
  registrar_->SetInitialTypes(restored_types);

  ConfigureReason reason = restored_types.Empty()
                               ? CONFIGURE_REASON_NEW_CLIENT
                               : CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE;

  ModelTypeSet new_control_types =
      registrar_->ConfigureDataTypes(ControlTypes(), ModelTypeSet());

  // Control types don't have DataTypeControllers, but they need to have
  // update handlers registered in ModelTypeRegistry. Register them here.
  ModelTypeConnector* model_type_connector =
      sync_manager_->GetModelTypeConnector();
  ModelTypeSet control_types = ControlTypes();
  for (ModelType type : control_types) {
    model_type_connector->RegisterDirectoryType(type, GROUP_PASSIVE);
  }

  ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);
  SDVLOG(1) << "Control Types " << ModelTypeSetToString(new_control_types)
            << " added; calling ConfigureSyncer";

  ModelTypeSet types_to_purge =
      Difference(ModelTypeSet::All(), GetRoutingInfoTypes(routing_info));

  sync_manager_->PurgeDisabledTypes(types_to_purge, ModelTypeSet(),
                                    ModelTypeSet());
  sync_manager_->ConfigureSyncer(
      reason, new_control_types, SyncManager::SyncFeatureState::INITIALIZING,
      base::Bind(&SyncBackendHostCore::DoInitialProcessControlTypes,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SyncBackendHostCore::OnConnectionStatusChange(ConnectionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE,
             &SyncEngineImpl::HandleConnectionStatusChangeOnFrontendLoop,
             status);
}

void SyncBackendHostCore::OnCommitCountersUpdated(
    ModelType type,
    const CommitCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(
      FROM_HERE,
      &SyncEngineImpl::HandleDirectoryCommitCountersUpdatedOnFrontendLoop, type,
      counters);
}

void SyncBackendHostCore::OnUpdateCountersUpdated(
    ModelType type,
    const UpdateCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(
      FROM_HERE,
      &SyncEngineImpl::HandleDirectoryUpdateCountersUpdatedOnFrontendLoop, type,
      counters);
}

void SyncBackendHostCore::OnStatusCountersUpdated(
    ModelType type,
    const StatusCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(
      FROM_HERE,
      &SyncEngineImpl::HandleDirectoryStatusCountersUpdatedOnFrontendLoop, type,
      counters);
}

void SyncBackendHostCore::OnActionableError(
    const SyncProtocolError& sync_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE,
             &SyncEngineImpl::HandleActionableErrorEventOnFrontendLoop,
             sync_error);
}

void SyncBackendHostCore::OnMigrationRequested(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_.Call(FROM_HERE, &SyncEngineImpl::HandleMigrationRequestedOnFrontendLoop,
             types);
}

void SyncBackendHostCore::OnProtocolEvent(const ProtocolEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (forward_protocol_events_) {
    std::unique_ptr<ProtocolEvent> event_clone(event.Clone());
    host_.Call(FROM_HERE, &SyncEngineImpl::HandleProtocolEventOnFrontendLoop,
               base::Passed(std::move(event_clone)));
  }
}

void SyncBackendHostCore::DoOnInvalidatorStateChange(InvalidatorState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->SetInvalidatorEnabled(state == INVALIDATIONS_ENABLED);
}

void SyncBackendHostCore::DoOnIncomingInvalidation(
    const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ObjectIdSet ids = invalidation_map.GetObjectIds();
  for (const invalidation::ObjectId& object_id : ids) {
    ModelType type;
    if (!NotificationTypeToRealModelType(object_id.name(), &type)) {
      DLOG(WARNING) << "Notification has invalid id: "
                    << ObjectIdToString(object_id);
    } else {
      bool is_grouped = (ids.size() != 1);
      RecordPerModelTypeInvalidation(ModelTypeToHistogramInt(type), is_grouped);
      SingleObjectInvalidationSet invalidation_set =
          invalidation_map.ForObject(object_id);
      for (Invalidation invalidation : invalidation_set) {
        auto last_invalidation = last_invalidation_versions_.find(type);
        if (!invalidation.is_unknown_version() &&
            last_invalidation != last_invalidation_versions_.end() &&
            invalidation.version() <= last_invalidation->second) {
          DVLOG(1) << "Ignoring redundant invalidation for "
                   << ModelTypeToString(type) << " with version "
                   << invalidation.version() << ", last seen version was "
                   << last_invalidation->second;
          continue;
        }
        if (!is_grouped && !invalidation.is_unknown_version()) {
          UMA_HISTOGRAM_ENUMERATION("Sync.NonGroupedInvalidationKnownVersion",
                                    ModelTypeToHistogramInt(type),
                                    static_cast<int>(MODEL_TYPE_COUNT));
        }
        std::unique_ptr<InvalidationInterface> inv_adapter(
            new InvalidationAdapter(invalidation));
        sync_manager_->OnIncomingInvalidation(type, std::move(inv_adapter));
        if (!invalidation.is_unknown_version())
          last_invalidation_versions_[type] = invalidation.version();
      }
    }
  }

  host_.Call(FROM_HERE, &SyncEngineImpl::UpdateInvalidationVersions,
             last_invalidation_versions_);
}

void SyncBackendHostCore::DoInitialize(SyncEngine::InitParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Blow away the partial or corrupt sync data folder before doing any more
  // initialization, if necessary.
  if (params.delete_sync_data_folder) {
    syncable::Directory::DeleteDirectoryFiles(sync_data_folder_);
  }

  // Make sure that the directory exists before initializing the backend.
  // If it already exists, this will do no harm.
  if (!base::CreateDirectory(sync_data_folder_)) {
    DLOG(FATAL) << "Sync Data directory creation failed.";
  }

  // Load the previously persisted set of invalidation versions into memory.
  last_invalidation_versions_ = params.invalidation_versions;

  DCHECK(!registrar_);
  DCHECK(params.registrar);
  registrar_ = std::move(params.registrar);

  DCHECK(!encryption_observer_proxy_);
  DCHECK(params.encryption_observer_proxy);
  encryption_observer_proxy_ = std::move(params.encryption_observer_proxy);

  sync_manager_ = params.sync_manager_factory->CreateSyncManager(name_);
  sync_manager_->AddObserver(this);

  SyncManager::InitArgs args;
  args.database_location = sync_data_folder_;
  args.event_handler = params.event_handler;
  args.service_url = params.service_url;
  args.enable_local_sync_backend = params.enable_local_sync_backend;
  args.local_sync_backend_folder = params.local_sync_backend_folder;
  args.post_factory = std::move(params.http_factory_getter)
                          .Run(&release_request_context_signal_);
  // Finish initializing the HttpBridgeFactory.  We do this here because
  // building the user agent may block on some platforms.
  args.post_factory->Init(params.sync_user_agent,
                          base::Bind(&BindFetcherToDataTracker));
  registrar_->GetWorkers(&args.workers);
  args.extensions_activity = params.extensions_activity.get();
  args.change_delegate = registrar_.get();  // as SyncManager::ChangeDelegate
  args.credentials = params.credentials;
  args.invalidator_client_id = params.invalidator_client_id;
  args.restored_key_for_bootstrapping = params.restored_key_for_bootstrapping;
  args.restored_keystore_key_for_bootstrapping =
      params.restored_keystore_key_for_bootstrapping;
  args.engine_components_factory = std::move(params.engine_components_factory);
  args.encryptor = &encryptor_;
  args.unrecoverable_error_handler = params.unrecoverable_error_handler;
  args.report_unrecoverable_error_function =
      params.report_unrecoverable_error_function;
  args.cancelation_signal = &stop_syncing_signal_;
  args.saved_nigori_state = std::move(params.saved_nigori_state);
  args.short_poll_interval = params.short_poll_interval;
  args.long_poll_interval = params.long_poll_interval;
  args.cache_guid = params.cache_guid;
  args.birthday = params.birthday;
  args.bag_of_chips = params.bag_of_chips;
  sync_manager_->Init(&args);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "SyncDirectory", base::ThreadTaskRunnerHandle::Get());
}

void SyncBackendHostCore::DoUpdateCredentials(
    const SyncCredentials& credentials) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // UpdateCredentials can be called during backend initialization, possibly
  // when backend initialization has failed but hasn't notified the UI thread
  // yet. In that case, the sync manager may have been destroyed on the sync
  // thread before this task was executed, so we do nothing.
  if (sync_manager_) {
    sync_manager_->UpdateCredentials(credentials);
  }
}

void SyncBackendHostCore::DoInvalidateCredentials() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sync_manager_) {
    sync_manager_->InvalidateCredentials();
  }
}

void SyncBackendHostCore::DoStartConfiguration() {
  sync_manager_->StartConfiguration();
}

void SyncBackendHostCore::DoStartSyncing(base::Time last_poll_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->StartSyncingNormally(last_poll_time);
}

void SyncBackendHostCore::DoSetEncryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->GetEncryptionHandler()->SetEncryptionPassphrase(passphrase);
}

void SyncBackendHostCore::DoInitialProcessControlTypes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Initilalizing Control Types";

  // Initialize encryption.
  sync_manager_->GetEncryptionHandler()->Init();

  // Note: experiments are currently handled via SBH::AddExperimentalTypes,
  // which is called at the end of every sync cycle.
  // TODO(zea): eventually add an experiment handler and initialize it here.

  const UserShare* user_share = sync_manager_->GetUserShare();
  if (!user_share) {  // Null in some tests.
    DVLOG(1) << "Skipping initialization of DeviceInfo";
    host_.Call(FROM_HERE,
               &SyncEngineImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  if (!sync_manager_->InitialSyncEndedTypes().HasAll(ControlTypes())) {
    LOG(ERROR) << "Failed to download control types";
    host_.Call(FROM_HERE,
               &SyncEngineImpl::HandleInitializationFailureOnFrontendLoop);
    return;
  }

  DCHECK_EQ(user_share->directory->cache_guid(), sync_manager_->cache_guid());
  host_.Call(
      FROM_HERE, &SyncEngineImpl::HandleInitializationSuccessOnFrontendLoop,
      registrar_->GetLastConfiguredTypes(), js_backend_, debug_info_listener_,
      base::Passed(sync_manager_->GetModelTypeConnectorProxy()),
      sync_manager_->cache_guid(), GetSessionNameBlocking(),
      user_share->directory->store_birthday(),
      user_share->directory->bag_of_chips());

  js_backend_.Reset();
  debug_info_listener_.Reset();
}

void SyncBackendHostCore::DoSetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->GetEncryptionHandler()->SetDecryptionPassphrase(passphrase);
}

void SyncBackendHostCore::DoEnableEncryptEverything() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->GetEncryptionHandler()->EnableEncryptEverything();
}

void SyncBackendHostCore::ShutdownOnUIThread() {
  // This will cut short any blocking network tasks, cut short any in-progress
  // sync cycles, and prevent the creation of new blocking network tasks and new
  // sync cycles.  If there was an in-progress network request, it would have
  // had a reference to the RequestContextGetter.  This reference will be
  // dropped by the time this function returns.
  //
  // It is safe to call this even if Sync's backend classes have not been
  // initialized yet.  Those classes will receive the message when the sync
  // thread finally getes around to constructing them.
  stop_syncing_signal_.Signal();

  // This will drop the HttpBridgeFactory's reference to the
  // RequestContextGetter.  Once this has been called, the HttpBridgeFactory can
  // no longer be used to create new HttpBridge instances.  We can get away with
  // this because the stop_syncing_signal_ has already been signalled, which
  // guarantees that the ServerConnectionManager will no longer attempt to
  // create new connections.
  release_request_context_signal_.Signal();
}

void SyncBackendHostCore::DoShutdown(ShutdownReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DoDestroySyncManager();

  registrar_ = nullptr;

  if (reason == DISABLE_SYNC)
    syncable::Directory::DeleteDirectoryFiles(sync_data_folder_);

  host_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SyncBackendHostCore::DoDestroySyncManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  if (sync_manager_) {
    DisableDirectoryTypeDebugInfoForwarding();
    save_changes_timer_.reset();
    sync_manager_->RemoveObserver(this);
    sync_manager_->ShutdownOnSyncThread();
    sync_manager_.reset();
  }
}

void SyncBackendHostCore::DoPurgeDisabledTypes(const ModelTypeSet& to_purge,
                                               const ModelTypeSet& to_journal,
                                               const ModelTypeSet& to_unapply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->PurgeDisabledTypes(to_purge, to_journal, to_unapply);
}

void SyncBackendHostCore::DoConfigureSyncer(
    ModelTypeConfigurer::ConfigureParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!params.ready_task.is_null());

  registrar_->ConfigureDataTypes(params.enabled_types, params.disabled_types);

  base::Closure chained_ready_task(base::Bind(
      &SyncBackendHostCore::DoFinishConfigureDataTypes,
      weak_ptr_factory_.GetWeakPtr(), params.to_download, params.ready_task));

  sync_manager_->ConfigureSyncer(params.reason, params.to_download,
                                 params.is_sync_feature_enabled
                                     ? SyncManager::SyncFeatureState::ON
                                     : SyncManager::SyncFeatureState::OFF,
                                 chained_ready_task);
}

void SyncBackendHostCore::DoFinishConfigureDataTypes(
    ModelTypeSet types_to_config,
    const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Update the enabled types for the bridge and sync manager.
  ModelSafeRoutingInfo routing_info;
  registrar_->GetModelSafeRoutingInfo(&routing_info);
  ModelTypeSet enabled_types = GetRoutingInfoTypes(routing_info);
  enabled_types.RemoveAll(ProxyTypes());

  const ModelTypeSet failed_configuration_types =
      Difference(types_to_config, sync_manager_->InitialSyncEndedTypes());
  const ModelTypeSet succeeded_configuration_types =
      Difference(types_to_config, failed_configuration_types);
  host_.Call(FROM_HERE, &SyncEngineImpl::FinishConfigureDataTypesOnFrontendLoop,
             enabled_types, succeeded_configuration_types,
             failed_configuration_types, ready_task);
}

void SyncBackendHostCore::SendBufferedProtocolEventsAndEnableForwarding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  forward_protocol_events_ = true;

  if (sync_manager_) {
    // Grab our own copy of the buffered events.
    // The buffer is not modified by this operation.
    std::vector<std::unique_ptr<ProtocolEvent>> buffered_events =
        sync_manager_->GetBufferedProtocolEvents();

    // Send them all over the fence to the host.
    for (auto& event : buffered_events) {
      host_.Call(FROM_HERE, &SyncEngineImpl::HandleProtocolEventOnFrontendLoop,
                 base::Passed(std::move(event)));
    }
  }
}

void SyncBackendHostCore::DisableProtocolEventForwarding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  forward_protocol_events_ = false;
}

void SyncBackendHostCore::EnableDirectoryTypeDebugInfoForwarding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sync_manager_);

  forward_type_info_ = true;

  if (!sync_manager_->HasDirectoryTypeDebugInfoObserver(this))
    sync_manager_->RegisterDirectoryTypeDebugInfoObserver(this);
  sync_manager_->RequestEmitDebugInfo();
}

void SyncBackendHostCore::DisableDirectoryTypeDebugInfoForwarding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sync_manager_);

  if (!forward_type_info_)
    return;

  forward_type_info_ = false;

  if (sync_manager_->HasDirectoryTypeDebugInfoObserver(this))
    sync_manager_->UnregisterDirectoryTypeDebugInfoObserver(this);
}

void SyncBackendHostCore::StartSavingChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!save_changes_timer_);
  save_changes_timer_ = std::make_unique<base::RepeatingTimer>();
  save_changes_timer_->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kSaveChangesIntervalSeconds),
      this, &SyncBackendHostCore::SaveChanges);
}

void SyncBackendHostCore::SaveChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->SaveChanges();
}

void SyncBackendHostCore::DoOnCookieJarChanged(bool account_mismatch,
                                               bool empty_jar,
                                               const base::Closure& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_manager_->OnCookieJarChanged(account_mismatch, empty_jar);
  if (!callback.is_null()) {
    host_.Call(FROM_HERE, &SyncEngineImpl::OnCookieJarChangedDoneOnFrontendLoop,
               callback);
  }
}

void SyncBackendHostCore::DoOnInvalidatorClientIdChange(
    const std::string& client_id) {
  if (base::FeatureList::IsEnabled(switches::kSyncE2ELatencyMeasurement)) {
    // Don't populate the ID, if client participates in latency measurement
    // experiment.
    return;
  }
  sync_manager_->UpdateInvalidationClientId(client_id);
}

bool SyncBackendHostCore::HasUnsyncedItemsForTest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sync_manager_);
  return sync_manager_->HasUnsyncedItemsForTest();
}

}  // namespace syncer
