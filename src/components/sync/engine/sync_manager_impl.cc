// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_manager_impl.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "components/sync/base/invalidation_interface.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/loopback_server/loopback_connection_manager.h"
#include "components/sync/engine/model_type_connector_proxy.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/net/sync_server_connection_manager.h"
#include "components/sync/engine/net/url_translator.h"
#include "components/sync/engine/nigori/cryptographer.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/keystore_keys_handler.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine/sync_scheduler.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {
namespace {

sync_pb::SyncEnums::GetUpdatesOrigin GetOriginFromReason(
    ConfigureReason reason) {
  switch (reason) {
    case CONFIGURE_REASON_RECONFIGURATION:
      return sync_pb::SyncEnums::RECONFIGURATION;
    case CONFIGURE_REASON_MIGRATION:
      return sync_pb::SyncEnums::MIGRATION;
    case CONFIGURE_REASON_NEW_CLIENT:
      return sync_pb::SyncEnums::NEW_CLIENT;
    case CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE:
    case CONFIGURE_REASON_CRYPTO:
      return sync_pb::SyncEnums::NEWLY_SUPPORTED_DATATYPE;
    case CONFIGURE_REASON_PROGRAMMATIC:
      return sync_pb::SyncEnums::PROGRAMMATIC;
    case CONFIGURE_REASON_UNKNOWN:
      NOTREACHED();
  }
  return sync_pb::SyncEnums::UNKNOWN_ORIGIN;
}

const char kSyncServerSyncPath[] = "/command/";

std::string StripTrailingSlash(const std::string& s) {
  int stripped_end_pos = s.size();
  if (s.at(stripped_end_pos - 1) == '/') {
    stripped_end_pos = stripped_end_pos - 1;
  }

  return s.substr(0, stripped_end_pos);
}

GURL MakeConnectionURL(const GURL& sync_server, const std::string& client_id) {
  DCHECK_EQ(kSyncServerSyncPath[0], '/');
  std::string full_path =
      StripTrailingSlash(sync_server.path()) + kSyncServerSyncPath;

  GURL::Replacements path_replacement;
  path_replacement.SetPathStr(full_path);
  return AppendSyncQueryString(sync_server.ReplaceComponents(path_replacement),
                               client_id);
}

}  // namespace

SyncManagerImpl::SyncManagerImpl(
    const std::string& name,
    network::NetworkConnectionTracker* network_connection_tracker)
    : name_(name),
      network_connection_tracker_(network_connection_tracker),
      initialized_(false),
      observing_network_connectivity_changes_(false),
      sync_encryption_handler_(nullptr) {}

SyncManagerImpl::~SyncManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialized_);
}

ModelTypeSet SyncManagerImpl::InitialSyncEndedTypes() {
  DCHECK(initialized_);
  return model_type_registry_->GetInitialSyncEndedTypes();
}

ModelTypeSet SyncManagerImpl::GetEnabledTypes() {
  DCHECK(initialized_);
  return model_type_registry_->GetEnabledDataTypes();
}

void SyncManagerImpl::ConfigureSyncer(ConfigureReason reason,
                                      ModelTypeSet to_download,
                                      SyncFeatureState sync_feature_state,
                                      base::OnceClosure ready_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ready_task.is_null());
  DCHECK(initialized_);

  DVLOG(1) << "Configuring -"
           << "\n\t"
           << "types to download: " << ModelTypeSetToString(to_download);

  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());
  scheduler_->ScheduleConfiguration(GetOriginFromReason(reason), to_download,
                                    std::move(ready_task));
  if (sync_feature_state != SyncFeatureState::INITIALIZING) {
    cycle_context_->set_is_sync_feature_enabled(sync_feature_state ==
                                                SyncFeatureState::ON);
  }
}

void SyncManagerImpl::Init(InitArgs* args) {
  DCHECK(!initialized_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!args->cache_guid.empty());
  DCHECK(args->post_factory);
  DCHECK(!args->poll_interval.is_zero());
  DCHECK(args->cancelation_signal);
  DVLOG(1) << "SyncManager starting Init...";

  DCHECK(args->encryption_observer_proxy);
  encryption_observer_proxy_ = std::move(args->encryption_observer_proxy);

  AddObserver(&js_sync_manager_observer_);
  SetJsEventHandler(args->event_handler);

  AddObserver(&debug_info_event_listener_);

  DCHECK(args->encryption_handler);
  sync_encryption_handler_ = args->encryption_handler;

  // Register for encryption related changes now. We have to do this before
  // the initial download of control types or initializing the encryption
  // handler in order to receive notifications triggered during encryption
  // startup.
  sync_encryption_handler_->AddObserver(this);
  sync_encryption_handler_->AddObserver(encryption_observer_proxy_.get());
  sync_encryption_handler_->AddObserver(&debug_info_event_listener_);
  sync_encryption_handler_->AddObserver(&js_sync_encryption_handler_observer_);

  allstatus_.SetHasKeystoreKey(
      !sync_encryption_handler_->GetKeystoreKeysHandler()->NeedKeystoreKey());

  if (args->enable_local_sync_backend) {
    VLOG(1) << "Running against local sync backend.";
    allstatus_.SetLocalBackendFolder(
        args->local_sync_backend_folder.AsUTF8Unsafe());
    connection_manager_ = std::make_unique<LoopbackConnectionManager>(
        args->local_sync_backend_folder);
  } else {
    connection_manager_ = std::make_unique<SyncServerConnectionManager>(
        MakeConnectionURL(args->service_url, args->cache_guid),
        std::move(args->post_factory), args->cancelation_signal);
  }
  connection_manager_->AddListener(this);

  DVLOG(1) << "Setting sync client ID: " << args->cache_guid;
  allstatus_.SetSyncId(args->cache_guid);
  DVLOG(1) << "Setting invalidator client ID: " << args->invalidator_client_id;
  allstatus_.SetInvalidatorClientId(args->invalidator_client_id);

  // Add observers after all initializations. Each observer will get initialized
  // status while being added.
  for (SyncStatusObserver* observer : args->sync_status_observers) {
    allstatus_.AddObserver(observer);
  }

  model_type_registry_ = std::make_unique<ModelTypeRegistry>(
      this, args->cancelation_signal, sync_encryption_handler_);

  // Build a SyncCycleContext and store the worker in it.
  DVLOG(1) << "Sync is bringing up SyncCycleContext.";
  std::vector<SyncEngineEventListener*> listeners;
  listeners.push_back(&allstatus_);
  listeners.push_back(this);
  cycle_context_ = args->engine_components_factory->BuildContext(
      connection_manager_.get(), args->extensions_activity, listeners,
      &debug_info_event_listener_, model_type_registry_.get(),
      args->invalidator_client_id, args->cache_guid, args->birthday,
      args->bag_of_chips, args->poll_interval);
  scheduler_ = args->engine_components_factory->BuildScheduler(
      name_, cycle_context_.get(), args->cancelation_signal,
      args->enable_local_sync_backend);

  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());

  initialized_ = true;

  if (!args->enable_local_sync_backend) {
    network_connection_tracker_->AddNetworkConnectionObserver(this);
  } else {
    scheduler_->OnCredentialsUpdated();
  }

  debug_info_event_listener_.InitializationComplete();
  js_sync_manager_observer_.InitializationComplete();
}

void SyncManagerImpl::OnPassphraseRequired(
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {
  // Does nothing.
}

void SyncManagerImpl::OnPassphraseAccepted() {
  // Does nothing.
}

void SyncManagerImpl::OnTrustedVaultKeyRequired() {
  // Does nothing.
}

void SyncManagerImpl::OnTrustedVaultKeyAccepted() {
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

void SyncManagerImpl::OnCryptographerStateChanged(Cryptographer* cryptographer,
                                                  bool has_pending_keys) {
  allstatus_.SetCryptographerCanEncrypt(cryptographer->CanEncrypt());
  allstatus_.SetCryptoHasPendingKeys(has_pending_keys);
  allstatus_.SetKeystoreMigrationTime(
      sync_encryption_handler_->GetKeystoreMigrationTime());
  allstatus_.SetTrustedVaultDebugInfo(
      sync_encryption_handler_->GetTrustedVaultDebugInfo());
}

void SyncManagerImpl::OnPassphraseTypeChanged(
    PassphraseType type,
    base::Time explicit_passphrase_time) {
  allstatus_.SetPassphraseType(type);
  allstatus_.SetKeystoreMigrationTime(
      sync_encryption_handler_->GetKeystoreMigrationTime());
}

void SyncManagerImpl::StartSyncingNormally(base::Time last_poll_time) {
  // Start the sync scheduler.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_->Start(SyncScheduler::NORMAL_MODE, last_poll_time);
}

void SyncManagerImpl::StartConfiguration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());
}

void SyncManagerImpl::UpdateCredentials(const SyncCredentials& credentials) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initialized_);

  cycle_context_->set_account_name(credentials.email);

  observing_network_connectivity_changes_ = true;
  if (!connection_manager_->SetAccessToken(credentials.access_token))
    return;  // Auth token is known to be invalid, so exit early.

  scheduler_->OnCredentialsUpdated();

  // TODO(zea): pass the credential age to the debug info event listener.
}

void SyncManagerImpl::InvalidateCredentials() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_manager_->SetAccessToken(std::string());
}

void SyncManagerImpl::AddObserver(SyncManager::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void SyncManagerImpl::RemoveObserver(SyncManager::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void SyncManagerImpl::ShutdownOnSyncThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Prevent any in-flight method calls from running.
  weak_ptr_factory_.InvalidateWeakPtrs();

  scheduler_.reset();
  cycle_context_.reset();

  model_type_registry_.reset();

  if (sync_encryption_handler_) {
    sync_encryption_handler_->RemoveObserver(&debug_info_event_listener_);
    sync_encryption_handler_->RemoveObserver(this);
  }

  SetJsEventHandler(WeakHandle<JsEventHandler>());
  RemoveObserver(&js_sync_manager_observer_);

  RemoveObserver(&debug_info_event_listener_);

  // |connection_manager_| may end up being null here in tests (in synchronous
  // initialization mode).
  //
  // TODO(akalin): Fix this behavior.
  if (connection_manager_)
    connection_manager_->RemoveListener(this);
  connection_manager_.reset();

  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  observing_network_connectivity_changes_ = false;

  initialized_ = false;
}

void SyncManagerImpl::OnConnectionChanged(network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!observing_network_connectivity_changes_) {
    DVLOG(1) << "Network change dropped.";
    return;
  }
  DVLOG(1) << "Network change detected.";
  scheduler_->OnConnectionStatusChange(type);
}

void SyncManagerImpl::OnServerConnectionEvent(
    const ServerConnectionEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event.connection_code == HttpResponse::SERVER_CONNECTION_OK) {
    for (auto& observer : observers_) {
      observer.OnConnectionStatusChange(CONNECTION_OK);
    }
  }

  if (event.connection_code == HttpResponse::SYNC_AUTH_ERROR) {
    observing_network_connectivity_changes_ = false;
    for (auto& observer : observers_) {
      observer.OnConnectionStatusChange(CONNECTION_AUTH_ERROR);
    }
  }

  if (event.connection_code == HttpResponse::SYNC_SERVER_ERROR) {
    for (auto& observer : observers_) {
      observer.OnConnectionStatusChange(CONNECTION_SERVER_ERROR);
    }
  }
}

void SyncManagerImpl::NudgeForInitialDownload(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_->ScheduleInitialSyncNudge(type);
}

void SyncManagerImpl::NudgeForCommit(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  debug_info_event_listener_.OnNudgeFromDatatype(type);
  scheduler_->ScheduleLocalNudge(type);
}

void SyncManagerImpl::OnSyncCycleEvent(const SyncCycleEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only send an event if this is due to a cycle ending and this cycle
  // concludes a canonical "sync" process; that is, based on what is known
  // locally we are "all happy" and up to date.  There may be new changes on
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
    for (auto& observer : observers_) {
      observer.OnSyncCycleCompleted(event.snapshot);
    }
  }
}

void SyncManagerImpl::OnActionableError(const SyncProtocolError& error) {
  for (auto& observer : observers_) {
    observer.OnActionableError(error);
  }
}

void SyncManagerImpl::OnRetryTimeChanged(base::Time) {}

void SyncManagerImpl::OnThrottledTypesChanged(ModelTypeSet) {}

void SyncManagerImpl::OnBackedOffTypesChanged(ModelTypeSet) {}

void SyncManagerImpl::OnMigrationRequested(ModelTypeSet types) {
  for (auto& observer : observers_) {
    observer.OnMigrationRequested(types);
  }
}

void SyncManagerImpl::OnProtocolEvent(const ProtocolEvent& event) {
  protocol_event_buffer_.RecordProtocolEvent(event);
  for (auto& observer : observers_) {
    observer.OnProtocolEvent(event);
  }
}

void SyncManagerImpl::SetJsEventHandler(
    const WeakHandle<JsEventHandler>& event_handler) {
  js_sync_manager_observer_.SetJsEventHandler(event_handler);
  js_sync_encryption_handler_observer_.SetJsEventHandler(event_handler);
}

void SyncManagerImpl::SetInvalidatorEnabled(bool invalidator_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Invalidator enabled state is now: " << invalidator_enabled;
  allstatus_.SetNotificationsEnabled(invalidator_enabled);
  scheduler_->SetNotificationsEnabled(invalidator_enabled);
}

void SyncManagerImpl::OnIncomingInvalidation(
    ModelType type,
    std::unique_ptr<InvalidationInterface> invalidation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  allstatus_.IncrementNotificationsReceived();
  scheduler_->ScheduleInvalidationNudge(type, std::move(invalidation));
}

void SyncManagerImpl::RefreshTypes(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (types.Empty()) {
    LOG(WARNING) << "Sync received refresh request with no types specified.";
  } else {
    scheduler_->ScheduleLocalRefreshRequest(types);
  }
}

ModelTypeConnector* SyncManagerImpl::GetModelTypeConnector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_type_registry_.get();
}

std::unique_ptr<ModelTypeConnector>
SyncManagerImpl::GetModelTypeConnectorProxy() {
  DCHECK(initialized_);
  return std::make_unique<ModelTypeConnectorProxy>(
      base::SequencedTaskRunnerHandle::Get(),
      model_type_registry_->AsWeakPtr());
}

WeakHandle<JsBackend> SyncManagerImpl::GetJsBackend() {
  return MakeWeakHandle(weak_ptr_factory_.GetWeakPtr());
}

WeakHandle<DataTypeDebugInfoListener> SyncManagerImpl::GetDebugInfoListener() {
  return MakeWeakHandle(debug_info_event_listener_.GetWeakPtr());
}

std::string SyncManagerImpl::cache_guid() {
  DCHECK(initialized_);
  return cycle_context_->cache_guid();
}

std::string SyncManagerImpl::birthday() {
  DCHECK(initialized_);
  DCHECK(cycle_context_);
  return cycle_context_->birthday();
}

std::string SyncManagerImpl::bag_of_chips() {
  DCHECK(initialized_);
  DCHECK(cycle_context_);
  return cycle_context_->bag_of_chips();
}

bool SyncManagerImpl::HasUnsyncedItemsForTest() {
  return model_type_registry_->HasUnsyncedItems();
}

SyncEncryptionHandler* SyncManagerImpl::GetEncryptionHandler() {
  DCHECK(sync_encryption_handler_);
  return sync_encryption_handler_;
}

std::vector<std::unique_ptr<ProtocolEvent>>
SyncManagerImpl::GetBufferedProtocolEvents() {
  return protocol_event_buffer_.GetBufferedProtocolEvents();
}

void SyncManagerImpl::OnCookieJarChanged(bool account_mismatch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cycle_context_->set_cookie_jar_mismatch(account_mismatch);
}

void SyncManagerImpl::UpdateInvalidationClientId(const std::string& client_id) {
  DVLOG(1) << "Setting invalidator client ID: " << client_id;
  allstatus_.SetInvalidatorClientId(client_id);
  cycle_context_->set_invalidator_client_id(client_id);
}

void SyncManagerImpl::UpdateSingleClientStatus(bool single_client) {
  cycle_context_->set_single_client(single_client);
}

void SyncManagerImpl::UpdateActiveDeviceFCMRegistrationTokens(
    std::vector<std::string> fcm_registration_tokens) {
  cycle_context_->set_active_device_fcm_registration_tokens(
      std::move(fcm_registration_tokens));
}

}  // namespace syncer
