// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/profile_sync_service.h"

#include <cstddef>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_sync/sync_auth_manager.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/device_info/device_info_sync_service.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/driver/backend_migrator.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/directory_data_type_controller.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_type_preference_provider.h"
#include "components/sync/driver/sync_util.h"
#include "components/sync/engine/cycle/type_debug_info_observer.h"
#include "components/sync/engine/engine_components_factory_impl.h"
#include "components/sync/engine/net/http_bridge_network_resources.h"
#include "components/sync/engine/net/network_resources.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/syncable/base_transaction.h"
#include "components/sync/syncable/directory.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/version_info/version_info_values.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_mutator.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using syncer::DataTypeController;
using syncer::DataTypeManager;
using syncer::EngineComponentsFactory;
using syncer::EngineComponentsFactoryImpl;

namespace browser_sync {

namespace {

// The initial state of sync, for the Sync.InitialState histogram. Even if
// this value is CAN_START, sync startup might fail for reasons that we may
// want to consider logging in the future, such as a passphrase needed for
// decryption, or the version of Chrome being too old. This enum is used to
// back a UMA histogram, and should therefore be treated as append-only.
enum SyncInitialState {
  CAN_START,                // Sync can attempt to start up.
  NOT_SIGNED_IN,            // There is no signed in user.
  NOT_REQUESTED,            // The user turned off sync.
  NOT_REQUESTED_NOT_SETUP,  // The user turned off sync and setup completed
                            // is false. Might indicate a stop-and-clear.
  NEEDS_CONFIRMATION,       // The user must confirm sync settings.
  NOT_ALLOWED_BY_POLICY,    // Sync is disallowed by enterprise policy.
  NOT_ALLOWED_BY_PLATFORM,  // Sync is disallowed by the platform.
  SYNC_INITIAL_STATE_LIMIT
};

void RecordSyncInitialState(int disable_reasons, bool first_setup_complete) {
  SyncInitialState sync_state = CAN_START;
  if (disable_reasons & ProfileSyncService::DISABLE_REASON_NOT_SIGNED_IN) {
    sync_state = NOT_SIGNED_IN;
  } else if (disable_reasons &
             ProfileSyncService::DISABLE_REASON_ENTERPRISE_POLICY) {
    sync_state = NOT_ALLOWED_BY_POLICY;
  } else if (disable_reasons &
             ProfileSyncService::DISABLE_REASON_PLATFORM_OVERRIDE) {
    // This case means Android's "MasterSync" toggle. However, that is not
    // plumbed into ProfileSyncService until after this method, so we never get
    // here. See http://crbug.com/568771.
    sync_state = NOT_ALLOWED_BY_PLATFORM;
  } else if (disable_reasons & ProfileSyncService::DISABLE_REASON_USER_CHOICE) {
    if (first_setup_complete) {
      sync_state = NOT_REQUESTED;
    } else {
      sync_state = NOT_REQUESTED_NOT_SETUP;
    }
  } else if (!first_setup_complete) {
    sync_state = NEEDS_CONFIRMATION;
  }
  UMA_HISTOGRAM_ENUMERATION("Sync.InitialState", sync_state,
                            SYNC_INITIAL_STATE_LIMIT);
}

constexpr char kSyncUnrecoverableErrorHistogram[] = "Sync.UnrecoverableErrors";

EngineComponentsFactory::Switches EngineSwitchesFromCommandLine() {
  EngineComponentsFactory::Switches factory_switches = {
      EngineComponentsFactory::ENCRYPTION_KEYSTORE,
      EngineComponentsFactory::BACKOFF_NORMAL};

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kSyncShortInitialRetryOverride)) {
    factory_switches.backoff_override =
        EngineComponentsFactory::BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE;
  }
  if (cl->HasSwitch(switches::kSyncEnableGetUpdateAvoidance)) {
    factory_switches.pre_commit_updates_policy =
        EngineComponentsFactory::FORCE_ENABLE_PRE_COMMIT_UPDATE_AVOIDANCE;
  }
  if (cl->HasSwitch(switches::kSyncShortNudgeDelayForTest)) {
    factory_switches.force_short_nudge_delay_for_test = true;
  }
  return factory_switches;
}

DataTypeController::TypeMap BuildDataTypeControllerMap(
    DataTypeController::TypeVector controllers) {
  DataTypeController::TypeMap type_map;
  for (std::unique_ptr<DataTypeController>& controller : controllers) {
    DCHECK(controller);
    syncer::ModelType type = controller->type();
    DCHECK_EQ(0U, type_map.count(type));
    type_map[type] = std::move(controller);
  }
  return type_map;
}

}  // namespace

ProfileSyncService::InitParams::InitParams() = default;
ProfileSyncService::InitParams::InitParams(InitParams&& other) = default;
ProfileSyncService::InitParams::~InitParams() = default;

ProfileSyncService::ProfileSyncService(InitParams init_params)
    : sync_client_(std::move(init_params.sync_client)),
      sync_prefs_(sync_client_->GetPrefService()),
      identity_manager_(init_params.identity_manager),
      auth_manager_(std::make_unique<SyncAuthManager>(
          identity_manager_,
          base::BindRepeating(&ProfileSyncService::AccountStateChanged,
                              base::Unretained(this)),
          base::BindRepeating(&ProfileSyncService::CredentialsChanged,
                              base::Unretained(this)))),
      debug_identifier_(init_params.debug_identifier),
      sync_service_url_(
          syncer::GetSyncServiceURL(*base::CommandLine::ForCurrentProcess(),
                                    sync_client_->GetDeviceInfoSyncService()
                                        ->GetLocalDeviceInfoProvider()
                                        ->GetChannel())),
      crypto_(
          base::BindRepeating(&ProfileSyncService::NotifyObservers,
                              base::Unretained(this)),
          base::BindRepeating(&ProfileSyncService::ReconfigureDueToPassphrase,
                              base::Unretained(this)),
          &sync_prefs_),
      network_time_update_callback_(
          std::move(init_params.network_time_update_callback)),
      url_loader_factory_(std::move(init_params.url_loader_factory)),
      network_connection_tracker_(init_params.network_connection_tracker),
      is_first_time_sync_configure_(false),
      engine_initialized_(false),
      sync_disabled_by_admin_(false),
      unrecoverable_error_reason_(ERROR_REASON_UNSET),
      expect_sync_configuration_aborted_(false),
      invalidations_identity_providers_(
          init_params.invalidations_identity_providers),
      network_resources_(
          std::make_unique<syncer::HttpBridgeNetworkResources>()),
      start_behavior_(init_params.start_behavior),
      passphrase_prompt_triggered_by_version_(false),
      is_stopping_and_clearing_(false),
      sync_enabled_weak_factory_(this),
      weak_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sync_client_);
  DCHECK(IsLocalSyncEnabled() || identity_manager_ != nullptr);

  // If Sync is disabled via command line flag, then ProfileSyncService
  // shouldn't be instantiated.
  DCHECK(switches::IsSyncAllowedByFlag());

  std::string last_version = sync_prefs_.GetLastRunVersion();
  std::string current_version = PRODUCT_VERSION;
  sync_prefs_.SetLastRunVersion(current_version);

  // Check for a major version change. Note that the versions have format
  // MAJOR.MINOR.BUILD.PATCH.
  if (last_version.substr(0, last_version.find('.')) !=
      current_version.substr(0, current_version.find('.'))) {
    passphrase_prompt_triggered_by_version_ = true;
  }

  startup_controller_ = std::make_unique<syncer::StartupController>(
      base::BindRepeating(&ProfileSyncService::GetPreferredDataTypes,
                          base::Unretained(this)),
      base::BindRepeating(&ProfileSyncService::IsEngineAllowedToStart,
                          base::Unretained(this)),
      base::BindRepeating(&ProfileSyncService::StartUpSlowEngineComponents,
                          base::Unretained(this)));

  sync_stopped_reporter_ = std::make_unique<syncer::SyncStoppedReporter>(
      sync_service_url_,
      sync_client_->GetDeviceInfoSyncService()
          ->GetLocalDeviceInfoProvider()
          ->GetSyncUserAgent(),
      url_loader_factory_, syncer::SyncStoppedReporter::ResultCallback());

  if (identity_manager_)
    identity_manager_->AddObserver(this);

#if defined(OS_CHROMEOS)
  std::string bootstrap_token = sync_prefs_.GetEncryptionBootstrapToken();
  if (bootstrap_token.empty()) {
    sync_prefs_.SetEncryptionBootstrapToken(
        sync_prefs_.GetSpareBootstrapToken());
  }
#endif

  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      base::BindRepeating(&ProfileSyncService::OnMemoryPressure,
                          sync_enabled_weak_factory_.GetWeakPtr()));
}

ProfileSyncService::~ProfileSyncService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (identity_manager_)
    identity_manager_->RemoveObserver(this);
  sync_prefs_.RemoveSyncPrefObserver(this);
  // Shutdown() should have been called before destruction.
  DCHECK(!engine_initialized_);
}

void ProfileSyncService::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(mastiz): The controllers map should be provided as argument.
  data_type_controllers_ =
      BuildDataTypeControllerMap(sync_client_->CreateDataTypeControllers(this));

  user_settings_ = std::make_unique<syncer::SyncUserSettingsImpl>(
      &crypto_, &sync_prefs_, GetRegisteredDataTypes(),
      base::BindRepeating(&ProfileSyncService::SyncAllowedByPlatformChanged,
                          base::Unretained(this)));

  sync_prefs_.AddSyncPrefObserver(this);

  // If sync is disallowed by policy, clean up.
  if (HasDisableReason(DISABLE_REASON_ENTERPRISE_POLICY)) {
    // Note that this won't actually clear data, since neither |engine_| nor
    // |sync_thread_| exist at this point. Bug or feature?
    StopImpl(CLEAR_DATA);
  }

  if (!IsLocalSyncEnabled()) {
    auth_manager_->RegisterForAuthNotifications();
    for (auto* provider : invalidations_identity_providers_) {
      if (provider) {
        provider->SetActiveAccountId(GetAuthenticatedAccountInfo().account_id);
      }
    }

    if (!IsSignedIn()) {
      // Clean up in case of previous crash during signout.
      StopImpl(CLEAR_DATA);
    }
  }

  // Note: We need to record the initial state *after* calling
  // RegisterForAuthNotifications(), because before that the authenticated
  // account isn't initialized.
  RecordSyncInitialState(GetDisableReasons(), IsFirstSetupComplete());

  // Auto-start means the first time the profile starts up, sync should start up
  // immediately.
  bool force_immediate = (start_behavior_ == AUTO_START &&
                          !HasDisableReason(DISABLE_REASON_USER_CHOICE) &&
                          !IsFirstSetupComplete());
  startup_controller_->TryStart(force_immediate);
}

void ProfileSyncService::StartSyncingWithServer() {
  if (engine_)
    engine_->StartSyncingWithServer();

  if (IsLocalSyncEnabled()) {
    TriggerRefresh(
        syncer::Intersection(GetActiveDataTypes(), syncer::ProtocolTypes()));
  }
}

bool ProfileSyncService::IsDataTypeControllerRunning(
    syncer::ModelType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = data_type_controllers_.find(type);
  if (iter == data_type_controllers_.end()) {
    return false;
  }
  return iter->second->state() == DataTypeController::RUNNING;
}

syncer::WeakHandle<syncer::JsEventHandler>
ProfileSyncService::GetJsEventHandler() {
  return syncer::MakeWeakHandle(sync_js_controller_.AsWeakPtr());
}

syncer::SyncEngine::HttpPostProviderFactoryGetter
ProfileSyncService::MakeHttpPostProviderFactoryGetter() {
  return base::BindOnce(&syncer::NetworkResources::GetHttpPostProviderFactory,
                        base::Unretained(network_resources_.get()),
                        url_loader_factory_->Clone(),
                        network_time_update_callback_);
}

syncer::WeakHandle<syncer::UnrecoverableErrorHandler>
ProfileSyncService::GetUnrecoverableErrorHandler() {
  return syncer::MakeWeakHandle(sync_enabled_weak_factory_.GetWeakPtr());
}

void ProfileSyncService::AccountStateChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsSignedIn()) {
    sync_disabled_by_admin_ = false;
    StopImpl(CLEAR_DATA);
    DCHECK(!engine_);
  } else {
    DCHECK(!engine_);
    startup_controller_->TryStart(/*force_immediate=*/IsSetupInProgress());
  }
  for (auto* provider : invalidations_identity_providers_) {
    if (provider) {
      provider->SetActiveAccountId(GetAuthenticatedAccountInfo().account_id);
    }
  }
}

void ProfileSyncService::CredentialsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the engine isn't allowed to start anymore due to the credentials change,
  // then shut down. This happens when the user signs out on the web, i.e. we're
  // in the "Sync paused" state.
  if (!IsEngineAllowedToStart()) {
    // This will notify observers if appropriate.
    StopImpl(KEEP_DATA);
    return;
  }

  if (!engine_) {
    startup_controller_->TryStart(/*force_immediate=*/true);
  } else {
    // If the engine already exists, just propagate the new credentials.
    syncer::SyncCredentials credentials = auth_manager_->GetCredentials();
    if (credentials.sync_token.empty()) {
      engine_->InvalidateCredentials();
    } else {
      engine_->UpdateCredentials(credentials);
    }
  }

  NotifyObservers();
}

bool ProfileSyncService::IsEngineAllowedToStart() const {
  // USER_CHOICE (i.e. the Sync feature toggle) and PLATFORM_OVERRIDE (i.e.
  // Android's "MasterSync" toggle) do not prevent starting up the Sync
  // transport.
  const int kDisableReasonMask =
      ~(DISABLE_REASON_USER_CHOICE | DISABLE_REASON_PLATFORM_OVERRIDE);
  return (GetDisableReasons() & kDisableReasonMask) == DISABLE_REASON_NONE;
}

bool ProfileSyncService::IsEncryptedDatatypeEnabled() const {
  return user_settings_->IsEncryptedDatatypeEnabled();
}

void ProfileSyncService::OnProtocolEvent(const syncer::ProtocolEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : protocol_event_observers_)
    observer.OnProtocolEvent(event);
}

void ProfileSyncService::OnDirectoryTypeCommitCounterUpdated(
    syncer::ModelType type,
    const syncer::CommitCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : type_debug_info_observers_)
    observer.OnCommitCountersUpdated(type, counters);
}

void ProfileSyncService::OnDirectoryTypeUpdateCounterUpdated(
    syncer::ModelType type,
    const syncer::UpdateCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : type_debug_info_observers_)
    observer.OnUpdateCountersUpdated(type, counters);
}

void ProfileSyncService::OnDatatypeStatusCounterUpdated(
    syncer::ModelType type,
    const syncer::StatusCounters& counters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : type_debug_info_observers_)
    observer.OnStatusCountersUpdated(type, counters);
}

void ProfileSyncService::OnDataTypeRequestsSyncStartup(syncer::ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(syncer::UserTypes().Has(type));

  if (!GetPreferredDataTypes().Has(type)) {
    // We can get here as datatype SyncableServices are typically wired up
    // to the native datatype even if sync isn't enabled.
    DVLOG(1) << "Dropping sync startup request because type "
             << syncer::ModelTypeToString(type) << "not enabled.";
    return;
  }

  // If this is a data type change after a major version update, reset the
  // passphrase prompted state and notify observers.
  if (IsPassphraseRequired() && passphrase_prompt_triggered_by_version_) {
    // The major version has changed and a local syncable change was made.
    // Reset the passphrase prompt state.
    passphrase_prompt_triggered_by_version_ = false;
    SetPassphrasePrompted(false);
    NotifyObservers();
  }

  if (engine_) {
    DVLOG(1) << "A data type requested sync startup, but it looks like "
                "something else beat it to the punch.";
    return;
  }

  startup_controller_->OnDataTypeRequestsSyncStartup(type);
}

void ProfileSyncService::StartUpSlowEngineComponents() {
  DCHECK(IsEngineAllowedToStart());

  engine_ = sync_client_->GetSyncApiComponentFactory()->CreateSyncEngine(
      debug_identifier_, sync_client_->GetInvalidationService(),
      sync_prefs_.AsWeakPtr());

  // Clear any old errors the first time sync starts.
  if (!IsFirstSetupComplete())
    ClearStaleErrors();

  if (!sync_thread_) {
    sync_thread_ = std::make_unique<base::Thread>("Chrome_SyncThread");
    base::Thread::Options options;
    options.timer_slack = base::TIMER_SLACK_MAXIMUM;
    bool success = sync_thread_->StartWithOptions(options);
    DCHECK(success);
  }

  syncer::SyncEngine::InitParams params;
  params.sync_task_runner = sync_thread_->task_runner();
  params.host = this;
  params.registrar = std::make_unique<syncer::SyncBackendRegistrar>(
      debug_identifier_,
      base::BindRepeating(&syncer::SyncClient::CreateModelWorkerForGroup,
                          base::Unretained(sync_client_.get())));
  params.encryption_observer_proxy = crypto_.GetEncryptionObserverProxy();

  params.extensions_activity = sync_client_->GetExtensionsActivity();
  params.event_handler = GetJsEventHandler();
  params.service_url = sync_service_url();
  params.sync_user_agent = sync_client_->GetDeviceInfoSyncService()
                               ->GetLocalDeviceInfoProvider()
                               ->GetSyncUserAgent();
  params.http_factory_getter = MakeHttpPostProviderFactoryGetter();
  params.credentials = auth_manager_->GetCredentials();
  DCHECK(!params.credentials.account_id.empty() || IsLocalSyncEnabled());
  if (!base::FeatureList::IsEnabled(switches::kSyncE2ELatencyMeasurement)) {
    invalidation::InvalidationService* invalidator =
        sync_client_->GetInvalidationService();
    params.invalidator_client_id =
        invalidator ? invalidator->GetInvalidatorClientId() : std::string();
  }
  params.sync_manager_factory =
      std::make_unique<syncer::SyncManagerFactory>(network_connection_tracker_);
  // The first time we start up the engine we want to ensure we have a clean
  // directory, so delete any old one that might be there.
  params.delete_sync_data_folder = !IsFirstSetupComplete();
  params.enable_local_sync_backend = sync_prefs_.IsLocalSyncEnabled();
  params.local_sync_backend_folder = sync_client_->GetLocalSyncBackendFolder();
  params.restored_key_for_bootstrapping =
      sync_prefs_.GetEncryptionBootstrapToken();
  params.restored_keystore_key_for_bootstrapping =
      sync_prefs_.GetKeystoreEncryptionBootstrapToken();
  params.cache_guid = sync_prefs_.GetCacheGuid();
  params.birthday = sync_prefs_.GetBirthday();
  params.bag_of_chips = sync_prefs_.GetBagOfChips();
  params.engine_components_factory =
      std::make_unique<EngineComponentsFactoryImpl>(
          EngineSwitchesFromCommandLine());
  params.unrecoverable_error_handler = GetUnrecoverableErrorHandler();
  params.report_unrecoverable_error_function = base::BindRepeating(
      syncer::ReportUnrecoverableError, sync_client_->GetDeviceInfoSyncService()
                                            ->GetLocalDeviceInfoProvider()
                                            ->GetChannel());
  params.saved_nigori_state = crypto_.TakeSavedNigoriState();
  sync_prefs_.GetInvalidationVersions(&params.invalidation_versions);
  params.short_poll_interval = sync_prefs_.GetShortPollInterval();
  if (params.short_poll_interval.is_zero()) {
    params.short_poll_interval =
        base::TimeDelta::FromSeconds(syncer::kDefaultShortPollIntervalSeconds);
  }
  params.long_poll_interval = sync_prefs_.GetLongPollInterval();
  if (params.long_poll_interval.is_zero()) {
    params.long_poll_interval =
        base::TimeDelta::FromSeconds(syncer::kDefaultLongPollIntervalSeconds);
  }

  engine_->Initialize(std::move(params));

  UpdateFirstSyncTimePref();

  ReportPreviousSessionMemoryWarningCount();

  // TODO(treib): Consider kicking off an access token fetch here. Currently,
  // the flow goes as follows: The SyncEngine tries to connect to the server,
  // but has no access token, so it ends up calling OnConnectionStatusChange(
  // syncer::CONNECTION_AUTH_ERROR) which in turn causes SyncAuthManager to
  // request a new access token. That seems needlessly convoluted.
}

void ProfileSyncService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyShutdown();
  ShutdownImpl(syncer::BROWSER_SHUTDOWN);

  // All observers must be gone now: All KeyedServices should have unregistered
  // their observers already before, in their own Shutdown(), and all others
  // should have done it now when they got the shutdown notification.
  // Note: "might_have_observers" sounds like it might be inaccurate, but it can
  // only return false positives while an iteration over the ObserverList is
  // ongoing.
  DCHECK(!observers_.might_have_observers());

  auth_manager_.reset();

  if (sync_thread_)
    sync_thread_->Stop();

  DCHECK(!data_type_manager_);
  data_type_controllers_.clear();
}

void ProfileSyncService::ShutdownImpl(syncer::ShutdownReason reason) {
  if (!engine_) {
    if (reason == syncer::ShutdownReason::DISABLE_SYNC && sync_thread_) {
      // If the engine is already shut down when a DISABLE_SYNC happens,
      // the data directory needs to be cleaned up here.
      sync_thread_->task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&syncer::syncable::Directory::DeleteDirectoryFiles,
                         sync_client_->GetSyncDataPath()));
    }
    return;
  }

  if (reason == syncer::ShutdownReason::STOP_SYNC ||
      reason == syncer::ShutdownReason::DISABLE_SYNC) {
    RemoveClientFromServer();
  }

  // First, we spin down the engine to stop change processing as soon as
  // possible.
  base::Time shutdown_start_time = base::Time::Now();
  engine_->StopSyncingForShutdown();

  // Stop all data type controllers, if needed. Note that until Stop completes,
  // it is possible in theory to have a ChangeProcessor apply a change from a
  // native model. In that case, it will get applied to the sync database (which
  // doesn't get destroyed until we destroy the engine below) as an unsynced
  // change. That will be persisted, and committed on restart.
  if (data_type_manager_) {
    if (data_type_manager_->state() != DataTypeManager::STOPPED) {
      // When aborting as part of shutdown, we should expect an aborted sync
      // configure result, else we'll dcheck when we try to read the sync error.
      expect_sync_configuration_aborted_ = true;
      data_type_manager_->Stop(reason);
    }
    data_type_manager_.reset();
  }

  // Shutdown the migrator before the engine to ensure it doesn't pull a null
  // snapshot.
  migrator_.reset();
  sync_js_controller_.AttachJsBackend(syncer::WeakHandle<syncer::JsBackend>());

  engine_->Shutdown(reason);
  engine_.reset();

  base::TimeDelta shutdown_time = base::Time::Now() - shutdown_start_time;
  UMA_HISTOGRAM_TIMES("Sync.Shutdown.BackendDestroyedTime", shutdown_time);

  sync_enabled_weak_factory_.InvalidateWeakPtrs();

  startup_controller_->Reset();

  // If the sync DB is getting destroyed, the local DeviceInfo is no longer
  // valid and should be cleared from the cache.
  if (reason == syncer::ShutdownReason::DISABLE_SYNC) {
    sync_client_->GetDeviceInfoSyncService()->ClearLocalCacheGuid();
  }

  // Clear various state.
  crypto_.Reset();
  expect_sync_configuration_aborted_ = false;
  engine_initialized_ = false;
  last_snapshot_ = syncer::SyncCycleSnapshot();
  auth_manager_->ConnectionClosed();

  NotifyObservers();

  // Mark this as a clean shutdown(without crash).
  sync_prefs_.SetCleanShutdown(true);
}

void ProfileSyncService::StopImpl(SyncStopDataFate data_fate) {
  switch (data_fate) {
    case KEEP_DATA:
      ShutdownImpl(syncer::STOP_SYNC);
      break;
    case CLEAR_DATA:
      ClearUnrecoverableError();
      ShutdownImpl(syncer::DISABLE_SYNC);
      // Clear prefs (including SyncSetupHasCompleted) before shutting down so
      // PSS clients don't think we're set up while we're shutting down.
      // Note: We do this after shutting down, so that notifications about the
      // changed pref values don't mess up our state.
      sync_prefs_.ClearPreferences();
      break;
  }
}

syncer::SyncUserSettings* ProfileSyncService::GetUserSettings() {
  return user_settings_.get();
}

const syncer::SyncUserSettings* ProfileSyncService::GetUserSettings() const {
  return user_settings_.get();
}

int ProfileSyncService::GetDisableReasons() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If Sync is disabled via command line flag, then ProfileSyncService
  // shouldn't even be instantiated.
  DCHECK(switches::IsSyncAllowedByFlag());

  int result = DISABLE_REASON_NONE;
  if (!user_settings_->IsSyncAllowedByPlatform()) {
    result = result | DISABLE_REASON_PLATFORM_OVERRIDE;
  }
  if (sync_prefs_.IsManaged() || sync_disabled_by_admin_) {
    result = result | DISABLE_REASON_ENTERPRISE_POLICY;
  }
  // Local sync doesn't require sign-in.
  if (!IsSignedIn() && !IsLocalSyncEnabled()) {
    result = result | DISABLE_REASON_NOT_SIGNED_IN;
  }
  // When local sync is on sync should be considered requsted or otherwise it
  // will not resume after the policy or the flag has been removed.
  if (!user_settings_->IsSyncRequested() && !IsLocalSyncEnabled()) {
    result = result | DISABLE_REASON_USER_CHOICE;
  }
  if (unrecoverable_error_reason_ != ERROR_REASON_UNSET) {
    result = result | DISABLE_REASON_UNRECOVERABLE_ERROR;
  }
  if (base::FeatureList::IsEnabled(switches::kStopSyncInPausedState)) {
    if (auth_manager_->IsSyncPaused()) {
      result = result | DISABLE_REASON_PAUSED;
    }
  }
  return result;
}

syncer::SyncService::TransportState ProfileSyncService::GetTransportState()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsEngineAllowedToStart()) {
    // We shouldn't have an engine while in a disabled state.
    DCHECK(!engine_);
    return TransportState::DISABLED;
  }

  if (!engine_initialized_) {
    switch (startup_controller_->GetState()) {
        // TODO(crbug.com/935523): If the engine is allowed to start, then we
        // should generally have kicked off the startup process already, so
        // NOT_STARTED should be impossible. But we can temporarily be in this
        // state between shutting down and starting up again (e.g. during the
        // NotifyObservers() call in ShutdownImpl()).
      case syncer::StartupController::State::NOT_STARTED:
      case syncer::StartupController::State::STARTING_DEFERRED:
        DCHECK(!engine_);
        return TransportState::START_DEFERRED;
      case syncer::StartupController::State::STARTED:
        DCHECK(engine_);
        return TransportState::INITIALIZING;
    }
    NOTREACHED();
  }
  DCHECK(engine_);
  // The DataTypeManager gets created once the engine is initialized.
  DCHECK(data_type_manager_);

  // At this point we should usually be able to configure our data types (and
  // once the data types can be configured, they must actually get configured).
  // However, if the initial setup hasn't been completed, then we can't
  // configure the data types. Also if a later (non-initial) setup happens to be
  // in progress, we won't configure them right now.
  if (data_type_manager_->state() == DataTypeManager::STOPPED) {
    DCHECK(!CanConfigureDataTypes(/*bypass_setup_in_progress_check=*/false));
    return TransportState::PENDING_DESIRED_CONFIGURATION;
  }

  // Note that if a setup is started after the data types have been configured,
  // then they'll stay configured even though CanConfigureDataTypes will be
  // false.
  DCHECK(CanConfigureDataTypes(/*bypass_setup_in_progress_check=*/false) ||
         IsSetupInProgress());

  if (data_type_manager_->state() != DataTypeManager::CONFIGURED) {
    return TransportState::CONFIGURING;
  }

  return TransportState::ACTIVE;
}

bool ProfileSyncService::IsFirstSetupComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_settings_->IsFirstSetupComplete();
}

void ProfileSyncService::UpdateLastSyncedTime() {
  sync_prefs_.SetLastSyncedTime(base::Time::Now());
}

void ProfileSyncService::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnStateChanged(this);
  }
}

void ProfileSyncService::NotifySyncCycleCompleted() {
  for (auto& observer : observers_)
    observer.OnSyncCycleCompleted(this);
}

void ProfileSyncService::NotifyShutdown() {
  for (auto& observer : observers_)
    observer.OnSyncShutdown(this);
}

void ProfileSyncService::ClearStaleErrors() {
  ClearUnrecoverableError();
  last_actionable_error_ = syncer::SyncProtocolError();
  // Clear the data type errors as well.
  if (data_type_manager_)
    data_type_manager_->ResetDataTypeErrors();
}

void ProfileSyncService::ClearUnrecoverableError() {
  unrecoverable_error_reason_ = ERROR_REASON_UNSET;
  unrecoverable_error_message_.clear();
  unrecoverable_error_location_ = base::Location();
}

// An invariant has been violated.  Transition to an error state where we try
// to do as little work as possible, to avoid further corruption or crashes.
void ProfileSyncService::OnUnrecoverableError(const base::Location& from_here,
                                              const std::string& message) {
  // TODO(crbug.com/840720): Get rid of the UnrecoverableErrorHandler interface
  // and instead pass a callback.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Unrecoverable errors that arrive via the syncer::UnrecoverableErrorHandler
  // interface are assumed to originate within the syncer.
  OnUnrecoverableErrorImpl(from_here, message, ERROR_REASON_SYNCER);
}

void ProfileSyncService::OnUnrecoverableErrorImpl(
    const base::Location& from_here,
    const std::string& message,
    UnrecoverableErrorReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(reason, ERROR_REASON_UNSET);
  unrecoverable_error_reason_ = reason;
  unrecoverable_error_message_ = message;
  unrecoverable_error_location_ = from_here;

  UMA_HISTOGRAM_ENUMERATION(kSyncUnrecoverableErrorHistogram,
                            unrecoverable_error_reason_, ERROR_REASON_LIMIT);
  LOG(ERROR) << "Unrecoverable error detected at " << from_here.ToString()
             << " -- ProfileSyncService unusable: " << message;

  // Shut all data types down.
  ShutdownImpl(syncer::DISABLE_SYNC);
}

void ProfileSyncService::ReenableDatatype(syncer::ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_initialized_ || !data_type_manager_)
    return;
  data_type_manager_->ReenableType(type);
}

void ProfileSyncService::ReadyForStartChanged(syncer::ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_initialized_ || !data_type_manager_)
    return;
  data_type_manager_->ReadyForStartChanged(type);
}

void ProfileSyncService::UpdateEngineInitUMA(bool success) const {
  if (is_first_time_sync_configure_) {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeFirstTimeSuccess", success);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Sync.BackendInitializeRestoreSuccess", success);
  }

  base::Time on_engine_initialized_time = base::Time::Now();
  base::TimeDelta delta =
      on_engine_initialized_time - startup_controller_->start_engine_time();
  if (is_first_time_sync_configure_) {
    UMA_HISTOGRAM_LONG_TIMES("Sync.BackendInitializeFirstTime", delta);
  } else {
    UMA_HISTOGRAM_LONG_TIMES("Sync.BackendInitializeRestoreTime", delta);
  }
}

void ProfileSyncService::OnEngineInitialized(
    syncer::ModelTypeSet initial_types,
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const std::string& cache_guid,
    const std::string& session_name,
    const std::string& birthday,
    const std::string& bag_of_chips,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!cache_guid.empty());

  // TODO(treib): Based on some crash reports, it seems like the user could have
  // signed out already at this point, so many of the steps below, including
  // datatype reconfiguration, should not be triggered.
  DCHECK(IsEngineAllowedToStart());

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".  LastSyncedTime will only be null in
  // this case, because the pref wasn't restored on StartUp.
  is_first_time_sync_configure_ = sync_prefs_.GetLastSyncedTime().is_null();

  UpdateEngineInitUMA(success);

  if (!success) {
    // Something went unexpectedly wrong.  Play it safe: stop syncing at once
    // and surface error UI to alert the user sync has stopped.
    OnUnrecoverableErrorImpl(FROM_HERE, "BackendInitialize failure",
                             ERROR_REASON_ENGINE_INIT_FAILURE);
    return;
  }

  engine_initialized_ = true;

  sync_js_controller_.AttachJsBackend(js_backend);

  // Initialize local device info.
  sync_client_->GetDeviceInfoSyncService()->InitLocalCacheGuid(cache_guid,
                                                               session_name);

  // Copy some data to preferences to be able to one day migrate away from the
  // directory.
  sync_prefs_.SetCacheGuid(cache_guid);
  sync_prefs_.SetBirthday(birthday);
  sync_prefs_.SetBagOfChips(bag_of_chips);

  if (protocol_event_observers_.might_have_observers()) {
    engine_->RequestBufferedProtocolEventsAndEnableForwarding();
  }

  if (type_debug_info_observers_.might_have_observers()) {
    engine_->EnableDirectoryTypeDebugInfoForwarding();
  }

  if (is_first_time_sync_configure_) {
    UpdateLastSyncedTime();
  }

  data_type_manager_ =
      sync_client_->GetSyncApiComponentFactory()->CreateDataTypeManager(
          initial_types, debug_info_listener, &data_type_controllers_,
          user_settings_.get(), engine_.get(), this);

  crypto_.SetSyncEngine(engine_.get());

  // Auto-start means IsFirstSetupComplete gets set automatically.
  if (start_behavior_ == AUTO_START && !IsFirstSetupComplete()) {
    // This will trigger a configure if it completes setup.
    user_settings_->SetFirstSetupComplete();
  } else if (CanConfigureDataTypes(/*bypass_setup_in_progress_check=*/false)) {
    // Datatype downloads on restart are generally due to newly supported
    // datatypes (although it's also possible we're picking up where a failed
    // previous configuration left off).
    // TODO(sync): consider detecting configuration recovery and setting
    // the reason here appropriately.
    ConfigureDataTypeManager(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE);
  }

  // Check for a cookie jar mismatch.
  if (identity_manager_) {
    identity::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
        identity_manager_->GetAccountsInCookieJar();
    if (accounts_in_cookie_jar_info.accounts_are_fresh) {
      OnAccountsInCookieUpdated(accounts_in_cookie_jar_info,
                                GoogleServiceAuthError::AuthErrorNone());
    }
  }

  NotifyObservers();
}

void ProfileSyncService::OnSyncCycleCompleted(
    const syncer::SyncCycleSnapshot& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  last_snapshot_ = snapshot;

  UpdateLastSyncedTime();
  if (!snapshot.poll_finish_time().is_null())
    sync_prefs_.SetLastPollTime(snapshot.poll_finish_time());
  DCHECK(!snapshot.short_poll_interval().is_zero());
  sync_prefs_.SetShortPollInterval(snapshot.short_poll_interval());

  DCHECK(!snapshot.long_poll_interval().is_zero());
  sync_prefs_.SetLongPollInterval(snapshot.long_poll_interval());

  syncer::UserShare* user_share = GetUserShare();
  if (user_share) {
    sync_prefs_.SetBirthday(user_share->directory->store_birthday());
    sync_prefs_.SetBagOfChips(user_share->directory->bag_of_chips());
  }

  DVLOG(2) << "Notifying observers sync cycle completed";
  NotifySyncCycleCompleted();
}

void ProfileSyncService::OnExperimentsChanged(
    const syncer::Experiments& experiments) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_experiments_.Matches(experiments))
    return;

  current_experiments_ = experiments;
}

void ProfileSyncService::OnConnectionStatusChange(
    syncer::ConnectionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auth_manager_->ConnectionStatusChanged(status);
  NotifyObservers();
}

void ProfileSyncService::OnMigrationNeededForTypes(syncer::ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(engine_initialized_);
  DCHECK(data_type_manager_);

  // Migrator must be valid, because we don't sync until it is created and this
  // callback originates from a sync cycle.
  migrator_->MigrateTypes(types);
}

void ProfileSyncService::OnActionableError(
    const syncer::SyncProtocolError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_actionable_error_ = error;
  DCHECK_NE(last_actionable_error_.action, syncer::UNKNOWN_ACTION);
  switch (error.action) {
    case syncer::UPGRADE_CLIENT:
    case syncer::CLEAR_USER_DATA_AND_RESYNC:
    case syncer::ENABLE_SYNC_ON_ACCOUNT:
    case syncer::STOP_AND_RESTART_SYNC:
      // TODO(lipalani) : if setup in progress we want to display these
      // actions in the popup. The current experience might not be optimal for
      // the user. We just dismiss the dialog.
      if (IsSetupInProgress()) {
        StopImpl(CLEAR_DATA);
        expect_sync_configuration_aborted_ = true;
      }
      // Trigger an unrecoverable error to stop syncing.
      OnUnrecoverableErrorImpl(FROM_HERE,
                               last_actionable_error_.error_description,
                               ERROR_REASON_ACTIONABLE_ERROR);
      break;
    case syncer::DISABLE_SYNC_ON_CLIENT:
      if (error.error_type == syncer::NOT_MY_BIRTHDAY) {
        UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", syncer::BIRTHDAY_ERROR,
                                  syncer::STOP_SOURCE_LIMIT);
      }
      // Note: Here we explicitly want StopAndClear (rather than StopImpl), so
      // that IsSyncRequested gets set to false, and Sync won't start again on
      // the next browser startup.
      StopAndClear();
#if !defined(OS_CHROMEOS)
      // On every platform except ChromeOS, sign out the user after a dashboard
      // clear.
      if (!IsLocalSyncEnabled()) {
        auto* account_mutator = identity_manager_->GetPrimaryAccountMutator();

        // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
        DCHECK(account_mutator);
        account_mutator->ClearPrimaryAccount(
            identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
            signin_metrics::SERVER_FORCED_DISABLE,
            signin_metrics::SignoutDelete::IGNORE_METRIC);
      }
#endif
      break;
    case syncer::STOP_SYNC_FOR_DISABLED_ACCOUNT:
      // Sync disabled by domain admin. we should stop syncing until next
      // restart.
      sync_disabled_by_admin_ = true;
      ShutdownImpl(syncer::DISABLE_SYNC);
      break;
    case syncer::RESET_LOCAL_SYNC_DATA:
      ShutdownImpl(syncer::DISABLE_SYNC);
      startup_controller_->TryStart(IsSetupInProgress());
      break;
    case syncer::UNKNOWN_ACTION:
      NOTREACHED();
  }
  NotifyObservers();
}

void ProfileSyncService::OnConfigureDone(
    const DataTypeManager::ConfigureResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_type_error_map_ = result.data_type_status_table.GetAllErrors();

  if (!sync_configure_start_time_.is_null()) {
    if (result.status == DataTypeManager::OK) {
      base::Time sync_configure_stop_time = base::Time::Now();
      base::TimeDelta delta =
          sync_configure_stop_time - sync_configure_start_time_;
      if (is_first_time_sync_configure_) {
        UMA_HISTOGRAM_LONG_TIMES("Sync.ServiceInitialConfigureTime", delta);
      } else {
        UMA_HISTOGRAM_LONG_TIMES("Sync.ServiceSubsequentConfigureTime", delta);
      }
    }
    sync_configure_start_time_ = base::Time();
  }

  DVLOG(1) << "PSS OnConfigureDone called with status: " << result.status;
  // The possible status values:
  //    ABORT - Configuration was aborted. This is not an error, if
  //            initiated by user.
  //    OK - Some or all types succeeded.
  //    Everything else is an UnrecoverableError. So treat it as such.

  // First handle the abort case.
  if (result.status == DataTypeManager::ABORTED &&
      expect_sync_configuration_aborted_) {
    DVLOG(0) << "ProfileSyncService::Observe Sync Configure aborted";
    expect_sync_configuration_aborted_ = false;
    return;
  }

  // Handle unrecoverable error.
  if (result.status != DataTypeManager::OK) {
    // Something catastrophic had happened. We should only have one
    // error representing it.
    syncer::SyncError error =
        result.data_type_status_table.GetUnrecoverableError();
    DCHECK(error.IsSet());
    std::string message =
        "Sync configuration failed with status " +
        DataTypeManager::ConfigureStatusToString(result.status) +
        " caused by " +
        syncer::ModelTypeSetToString(
            result.data_type_status_table.GetUnrecoverableErrorTypes()) +
        ": " + error.message();
    LOG(ERROR) << "ProfileSyncService error: " << message;
    OnUnrecoverableErrorImpl(error.location(), message,
                             ERROR_REASON_CONFIGURATION_FAILURE);
    return;
  }

  DCHECK_EQ(DataTypeManager::OK, result.status);

  // We should never get in a state where we have no encrypted datatypes
  // enabled, and yet we still think we require a passphrase for decryption.
  DCHECK(!GetUserSettings()->IsPassphraseRequiredForDecryption() ||
         IsEncryptedDatatypeEnabled());

  // Notify listeners that configuration is done.
  for (auto& observer : observers_)
    observer.OnSyncConfigurationCompleted(this);

  // This must be done before we start syncing with the server to avoid
  // sending unencrypted data up on a first time sync.
  if (user_settings_->IsEncryptionPending())
    engine_->EnableEncryptEverything();
  NotifyObservers();

  if (migrator_.get() && migrator_->state() != syncer::BackendMigrator::IDLE) {
    // Migration in progress.  Let the migrator know we just finished
    // configuring something.  It will be up to the migrator to call
    // StartSyncingWithServer() if migration is now finished.
    migrator_->OnConfigureDone(result);
    return;
  }

  RecordMemoryUsageHistograms();

  StartSyncingWithServer();
}

void ProfileSyncService::OnConfigureStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_configure_start_time_ = base::Time::Now();
  engine_->StartConfiguration();
  NotifyObservers();
}

bool ProfileSyncService::IsSetupInProgress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return outstanding_setup_in_progress_handles_ > 0;
}

bool ProfileSyncService::QueryDetailedSyncStatus(
    syncer::SyncEngine::Status* result) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_ && engine_initialized_) {
    *result = engine_->GetDetailedStatus();
    return true;
  }
  syncer::SyncEngine::Status status;
  status.sync_protocol_error = last_actionable_error_;
  *result = status;
  return false;
}

GoogleServiceAuthError ProfileSyncService::GetAuthError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->GetLastAuthError();
}

bool ProfileSyncService::CanConfigureDataTypes(
    bool bypass_setup_in_progress_check) const {
  // TODO(crbug.com/856179): Arguably, IsSetupInProgress() shouldn't prevent
  // configuring data types in transport mode, but at least for now, it's
  // easier to keep it like this. Changing this will likely require changes to
  // the setup UI flow.
  return data_type_manager_ &&
         (bypass_setup_in_progress_check || !IsSetupInProgress());
}

std::unique_ptr<syncer::SyncSetupInProgressHandle>
ProfileSyncService::GetSetupInProgressHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (++outstanding_setup_in_progress_handles_ == 1) {
    startup_controller_->TryStart(/*force_immediate=*/true);

    NotifyObservers();
  }

  return std::make_unique<syncer::SyncSetupInProgressHandle>(
      base::BindRepeating(&ProfileSyncService::OnSetupInProgressHandleDestroyed,
                          weak_factory_.GetWeakPtr()));
}

bool ProfileSyncService::IsLocalSyncEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_prefs_.IsLocalSyncEnabled();
}

void ProfileSyncService::TriggerRefresh(const syncer::ModelTypeSet& types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_initialized_)
    engine_->TriggerRefresh(types);
}

bool ProfileSyncService::IsSignedIn() const {
  // Sync is logged in if there is a non-empty account id.
  return !GetAuthenticatedAccountInfo().account_id.empty();
}

bool ProfileSyncService::IsPassphraseRequired() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_settings_->IsPassphraseRequired();
}

base::Time ProfileSyncService::GetLastSyncedTime() const {
  return sync_prefs_.GetLastSyncedTime();
}

void ProfileSyncService::OnPreferredDataTypesPrefChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!engine_ && !HasDisableReason(DISABLE_REASON_UNRECOVERABLE_ERROR)) {
    return;
  }

  if (data_type_manager_)
    data_type_manager_->ResetDataTypeErrors();

  ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/false);
}

syncer::SyncClient* ProfileSyncService::GetSyncClientForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_client_.get();
}

void ProfileSyncService::AddObserver(syncer::SyncServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void ProfileSyncService::RemoveObserver(syncer::SyncServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

bool ProfileSyncService::HasObserver(
    const syncer::SyncServiceObserver* observer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return observers_.HasObserver(observer);
}

syncer::ModelTypeSet ProfileSyncService::GetRegisteredDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  syncer::ModelTypeSet registered_types;
  // The |data_type_controllers_| are determined by command-line flags;
  // that's effectively what controls the values returned here.
  for (const std::pair<const syncer::ModelType,
                       std::unique_ptr<DataTypeController>>&
           type_and_controller : data_type_controllers_) {
    registered_types.Put(type_and_controller.first);
  }
  return registered_types;
}

syncer::ModelTypeSet ProfileSyncService::GetForcedDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  syncer::ModelTypeSet forced_types;
  for (const syncer::SyncTypePreferenceProvider* provider :
       preference_providers_) {
    forced_types.PutAll(provider->GetForcedDataTypes());
  }
  return Intersection(forced_types, GetRegisteredDataTypes());
}

syncer::ModelTypeSet ProfileSyncService::GetPreferredDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Union(user_settings_->GetPreferredDataTypes(), GetForcedDataTypes());
}

syncer::ModelTypeSet ProfileSyncService::GetActiveDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data_type_manager_ || GetAuthError().IsPersistentError())
    return syncer::ModelTypeSet();
  return data_type_manager_->GetActiveDataTypes();
}

void ProfileSyncService::SyncAllowedByPlatformChanged(bool allowed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!allowed) {
    StopImpl(KEEP_DATA);
    // TODO(crbug.com/856179): Evaluate whether we can get away without a full
    // restart (i.e. just reconfigure plus whatever cleanup is necessary). See
    // also similar comment in OnSyncRequestedPrefChange().
    startup_controller_->TryStart(/*force_immediate=*/false);
  }
}

void ProfileSyncService::ConfigureDataTypeManager(
    syncer::ConfigureReason reason) {
  syncer::ConfigureContext configure_context;
  configure_context.authenticated_account_id =
      GetAuthenticatedAccountInfo().account_id;
  configure_context.cache_guid = sync_prefs_.GetCacheGuid();
  configure_context.storage_option = syncer::STORAGE_ON_DISK;
  configure_context.reason = reason;
  configure_context.configuration_start_time = base::Time::Now();

  DCHECK(!configure_context.cache_guid.empty());

  if (!migrator_) {
    // We create the migrator at the same time.
    migrator_ = std::make_unique<syncer::BackendMigrator>(
        debug_identifier_, GetUserShare(), data_type_manager_.get(),
        base::BindRepeating(&ProfileSyncService::ConfigureDataTypeManager,
                            base::Unretained(this),
                            syncer::CONFIGURE_REASON_MIGRATION),
        base::BindRepeating(&ProfileSyncService::StartSyncingWithServer,
                            base::Unretained(this)));

    // Override reason if no configuration has completed ever.
    if (is_first_time_sync_configure_) {
      configure_context.reason = syncer::CONFIGURE_REASON_NEW_CLIENT;
    }
  }

  DCHECK(!configure_context.authenticated_account_id.empty() ||
         IsLocalSyncEnabled());
  DCHECK(!configure_context.cache_guid.empty());
  DCHECK_NE(configure_context.reason, syncer::CONFIGURE_REASON_UNKNOWN);

  // Note: When local Sync is enabled, then we want full-sync mode (not just
  // transport), even though Sync-the-feature is not considered enabled.
  bool use_transport_only_mode =
      !IsSyncFeatureEnabled() && !IsLocalSyncEnabled();

  syncer::ModelTypeSet types = GetPreferredDataTypes();
  // In transport-only mode, only a subset of data types is supported.
  if (use_transport_only_mode) {
    syncer::ModelTypeSet allowed_types = {syncer::USER_CONSENTS};

    if (base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableAccountWalletStorage) &&
        base::FeatureList::IsEnabled(switches::kSyncUSSAutofillWalletData)) {
      if (!GetUserSettings()->IsUsingSecondaryPassphrase() ||
          base::FeatureList::IsEnabled(
              switches::
                  kSyncAllowWalletDataInTransportModeWithCustomPassphrase)) {
        allowed_types.Put(syncer::AUTOFILL_WALLET_DATA);
      }
    }

    types = Intersection(types, allowed_types);
    configure_context.storage_option = syncer::STORAGE_IN_MEMORY;
  }
  data_type_manager_->Configure(types, configure_context);

  // Record in UMA whether we're configuring the full Sync feature or only the
  // transport.
  enum class ConfigureDataTypeManagerOption {
    kFeature = 0,
    kTransport = 1,
    kMaxValue = kTransport
  };
  UMA_HISTOGRAM_ENUMERATION("Sync.ConfigureDataTypeManagerOption",
                            use_transport_only_mode
                                ? ConfigureDataTypeManagerOption::kTransport
                                : ConfigureDataTypeManagerOption::kFeature);

  // Only if it's the full Sync feature, also record the user's choice of data
  // types.
  if (!use_transport_only_mode) {
    bool sync_everything = sync_prefs_.HasKeepEverythingSynced();
    UMA_HISTOGRAM_BOOLEAN("Sync.SyncEverything2", sync_everything);

    if (!sync_everything) {
      syncer::ModelTypeSet chosen_types = GetPreferredDataTypes();
      chosen_types.RetainAll(syncer::UserSelectableTypes());

      for (syncer::ModelType type : chosen_types) {
        UMA_HISTOGRAM_ENUMERATION("Sync.CustomSync2",
                                  syncer::ModelTypeToHistogramInt(type),
                                  static_cast<int>(syncer::MODEL_TYPE_COUNT));
      }
    }
  }
}

syncer::UserShare* ProfileSyncService::GetUserShare() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_ && engine_initialized_) {
    return engine_->GetUserShare();
  }
  NOTREACHED();
  return nullptr;
}

syncer::SyncCycleSnapshot ProfileSyncService::GetLastCycleSnapshot() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_snapshot_;
}

void ProfileSyncService::HasUnsyncedItemsForTest(
    base::OnceCallback<void(bool)> cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(engine_);
  DCHECK(engine_initialized_);
  engine_->HasUnsyncedItemsForTest(std::move(cb));
}

syncer::BackendMigrator* ProfileSyncService::GetBackendMigratorForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return migrator_.get();
}

std::unique_ptr<base::Value> ProfileSyncService::GetTypeStatusMap() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto result = std::make_unique<base::ListValue>();

  if (!engine_ || !engine_initialized_) {
    return std::move(result);
  }

  syncer::SyncEngine::Status detailed_status = engine_->GetDetailedStatus();
  const syncer::ModelTypeSet& throttled_types(detailed_status.throttled_types);
  const syncer::ModelTypeSet& backed_off_types(
      detailed_status.backed_off_types);

  std::unique_ptr<base::DictionaryValue> type_status_header(
      new base::DictionaryValue());
  type_status_header->SetString("status", "header");
  type_status_header->SetString("name", "Model Type");
  type_status_header->SetString("num_entries", "Total Entries");
  type_status_header->SetString("num_live", "Live Entries");
  type_status_header->SetString("message", "Message");
  type_status_header->SetString("state", "State");
  type_status_header->SetString("group_type", "Group Type");
  result->Append(std::move(type_status_header));

  syncer::ModelSafeRoutingInfo routing_info;
  engine_->GetModelSafeRoutingInfo(&routing_info);
  const syncer::ModelTypeSet registered = GetRegisteredDataTypes();
  for (syncer::ModelType type : registered) {
    auto type_status = std::make_unique<base::DictionaryValue>();
    type_status->SetString("name", ModelTypeToString(type));
    type_status->SetString("group_type",
                           ModelSafeGroupToString(routing_info[type]));

    if (data_type_error_map_.find(type) != data_type_error_map_.end()) {
      const syncer::SyncError& error = data_type_error_map_.find(type)->second;
      DCHECK(error.IsSet());
      switch (error.GetSeverity()) {
        case syncer::SyncError::SYNC_ERROR_SEVERITY_ERROR:
          type_status->SetString("status", "error");
          type_status->SetString(
              "message", "Error: " + error.location().ToString() + ", " +
                             error.GetMessagePrefix() + error.message());
          break;
        case syncer::SyncError::SYNC_ERROR_SEVERITY_INFO:
          type_status->SetString("status", "disabled");
          type_status->SetString("message", error.message());
          break;
      }
    } else if (throttled_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("message", " Throttled");
    } else if (backed_off_types.Has(type)) {
      type_status->SetString("status", "warning");
      type_status->SetString("message", "Backed off");
    } else if (routing_info.find(type) != routing_info.end()) {
      type_status->SetString("status", "ok");
      type_status->SetString("message", "");
    } else {
      type_status->SetString("status", "warning");
      type_status->SetString("message", "Disabled by User");
    }

    const auto& dtc_iter = data_type_controllers_.find(type);
    if (dtc_iter != data_type_controllers_.end()) {
      type_status->SetString("state", DataTypeController::StateToString(
                                          dtc_iter->second->state()));
      if (dtc_iter->second->state() !=
          syncer::DataTypeController::NOT_RUNNING) {
        // We use BindToCurrentSequence() to make sure observers (i.e.
        // |type_debug_info_observers_|) are not notified synchronously, which
        // the UI code (chrome://sync-internals) doesn't handle well.
        dtc_iter->second->GetStatusCounters(
            BindToCurrentSequence(base::BindRepeating(
                &ProfileSyncService::OnDatatypeStatusCounterUpdated,
                base::Unretained(this))));
      }
    }

    result->Append(std::move(type_status));
  }
  return std::move(result);
}

bool ProfileSyncService::encryption_pending() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We may be called during the setup process before we're initialized (via
  // IsEncryptedDatatypeEnabled and IsPassphraseRequiredForDecryption).
  return user_settings_->IsEncryptionPending();
}

syncer::ModelTypeSet ProfileSyncService::GetEncryptedDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_settings_->GetEncryptedDataTypes();
}

void ProfileSyncService::OnSyncManagedPrefChange(bool is_sync_managed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_sync_managed) {
    StopImpl(CLEAR_DATA);
  } else {
    // Sync is no longer disabled by policy. Try starting it up if appropriate.
    DCHECK(!engine_);
    startup_controller_->TryStart(IsSetupInProgress());
  }
}

void ProfileSyncService::OnFirstSetupCompletePrefChange(
    bool is_first_setup_complete) {
  if (engine_initialized_) {
    ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/false);
  }
}

void ProfileSyncService::OnSyncRequestedPrefChange(bool is_sync_requested) {
  if (is_sync_requested) {
    // If the Sync engine was already initialized (probably running in transport
    // mode), just reconfigure.
    if (engine_initialized_) {
      ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/false);
    } else {
      // Otherwise try to start up. Note that there might still be other disable
      // reasons remaining, in which case this will effectively do nothing.
      startup_controller_->TryStart(/*force_immediate=*/true);
    }

    NotifyObservers();
  } else {
    // This will notify the observers.
    if (is_stopping_and_clearing_) {
      is_stopping_and_clearing_ = false;
      StopImpl(CLEAR_DATA);
    } else {
      StopImpl(KEEP_DATA);
    }

    // TODO(crbug.com/856179): Evaluate whether we can get away without a full
    // restart (i.e. just reconfigure plus whatever cleanup is necessary).
    // Especially in the CLEAR_DATA case, StopImpl does a lot of cleanup that
    // might still be required.
    startup_controller_->TryStart(/*force_immediate=*/false);
  }
}

void ProfileSyncService::OnAccountsInCookieUpdated(
    const identity::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  OnAccountsInCookieUpdatedWithCallback(
      accounts_in_cookie_jar_info.signed_in_accounts, base::Closure());
}

void ProfileSyncService::OnAccountsInCookieUpdatedWithCallback(
    const std::vector<gaia::ListedAccount>& signed_in_accounts,
    const base::Closure& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_initialized_)
    return;

  bool cookie_jar_mismatch = HasCookieJarMismatch(signed_in_accounts);
  bool cookie_jar_empty = signed_in_accounts.empty();

  DVLOG(1) << "Cookie jar mismatch: " << cookie_jar_mismatch;
  DVLOG(1) << "Cookie jar empty: " << cookie_jar_empty;
  engine_->OnCookieJarChanged(cookie_jar_mismatch, cookie_jar_empty, callback);
}

bool ProfileSyncService::HasCookieJarMismatch(
    const std::vector<gaia::ListedAccount>& cookie_jar_accounts) {
  std::string account_id = GetAuthenticatedAccountInfo().account_id;
  // Iterate through list of accounts, looking for current sync account.
  for (const auto& account : cookie_jar_accounts) {
    if (account.id == account_id)
      return false;
  }
  return true;
}

void ProfileSyncService::AddProtocolEventObserver(
    syncer::ProtocolEventObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  protocol_event_observers_.AddObserver(observer);
  if (engine_) {
    engine_->RequestBufferedProtocolEventsAndEnableForwarding();
  }
}

void ProfileSyncService::RemoveProtocolEventObserver(
    syncer::ProtocolEventObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  protocol_event_observers_.RemoveObserver(observer);
  if (engine_ && !protocol_event_observers_.might_have_observers()) {
    engine_->DisableProtocolEventForwarding();
  }
}

void ProfileSyncService::AddTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* type_debug_info_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  type_debug_info_observers_.AddObserver(type_debug_info_observer);
  if (type_debug_info_observers_.might_have_observers() &&
      engine_initialized_) {
    engine_->EnableDirectoryTypeDebugInfoForwarding();
  }
}

void ProfileSyncService::RemoveTypeDebugInfoObserver(
    syncer::TypeDebugInfoObserver* type_debug_info_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  type_debug_info_observers_.RemoveObserver(type_debug_info_observer);
  if (!type_debug_info_observers_.might_have_observers() &&
      engine_initialized_) {
    engine_->DisableDirectoryTypeDebugInfoForwarding();
  }
}

void ProfileSyncService::AddPreferenceProvider(
    syncer::SyncTypePreferenceProvider* provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!HasPreferenceProvider(provider))
      << "Providers may only be added once!";
  preference_providers_.insert(provider);
}

void ProfileSyncService::RemovePreferenceProvider(
    syncer::SyncTypePreferenceProvider* provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(HasPreferenceProvider(provider))
      << "Only providers that have been added before can be removed!";
  preference_providers_.erase(provider);
}

bool ProfileSyncService::HasPreferenceProvider(
    syncer::SyncTypePreferenceProvider* provider) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return preference_providers_.count(provider) > 0;
}

namespace {

class GetAllNodesRequestHelper
    : public base::RefCountedThreadSafe<GetAllNodesRequestHelper> {
 public:
  GetAllNodesRequestHelper(
      syncer::ModelTypeSet requested_types,
      base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback);

  void OnReceivedNodesForType(const syncer::ModelType type,
                              std::unique_ptr<base::ListValue> node_list);

 private:
  friend class base::RefCountedThreadSafe<GetAllNodesRequestHelper>;
  virtual ~GetAllNodesRequestHelper();

  std::unique_ptr<base::ListValue> result_accumulator_;
  syncer::ModelTypeSet awaiting_types_;
  base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(GetAllNodesRequestHelper);
};

GetAllNodesRequestHelper::GetAllNodesRequestHelper(
    syncer::ModelTypeSet requested_types,
    base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback)
    : result_accumulator_(std::make_unique<base::ListValue>()),
      awaiting_types_(requested_types),
      callback_(std::move(callback)) {}

GetAllNodesRequestHelper::~GetAllNodesRequestHelper() {
  if (!awaiting_types_.Empty()) {
    DLOG(WARNING)
        << "GetAllNodesRequest deleted before request was fulfilled.  "
        << "Missing types are: " << ModelTypeSetToString(awaiting_types_);
  }
}

// Called when the set of nodes for a type has been returned.
// Only return one type of nodes each time.
void GetAllNodesRequestHelper::OnReceivedNodesForType(
    const syncer::ModelType type,
    std::unique_ptr<base::ListValue> node_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Add these results to our list.
  base::DictionaryValue type_dict;
  type_dict.SetKey("type", base::Value(ModelTypeToString(type)));
  type_dict.SetKey("nodes",
                   base::Value::FromUniquePtrValue(std::move(node_list)));
  result_accumulator_->GetList().push_back(std::move(type_dict));

  // Remember that this part of the request is satisfied.
  awaiting_types_.Remove(type);

  if (awaiting_types_.Empty()) {
    std::move(callback_).Run(std::move(result_accumulator_));
  }
}

}  // namespace

void ProfileSyncService::GetAllNodes(
    const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the engine isn't initialized yet, then there are no nodes to return.
  if (!engine_initialized_) {
    callback.Run(std::make_unique<base::ListValue>());
    return;
  }

  syncer::ModelTypeSet all_types = GetActiveDataTypes();
  all_types.PutAll(syncer::ControlTypes());
  scoped_refptr<GetAllNodesRequestHelper> helper =
      new GetAllNodesRequestHelper(all_types, callback);

  for (syncer::ModelType type : all_types) {
    const auto dtc_iter = data_type_controllers_.find(type);
    if (dtc_iter != data_type_controllers_.end()) {
      if (dtc_iter->second->state() ==
          syncer::DataTypeController::NOT_RUNNING) {
        // In the NOT_RUNNING state it's not allowed to call GetAllNodes on the
        // DataTypeController, so just return an empty result.
        // This can happen e.g. if we're waiting for a custom passphrase to be
        // entered - the data types are already considered active in this case,
        // but their DataTypeControllers are still NOT_RUNNING.
        helper->OnReceivedNodesForType(type,
                                       std::make_unique<base::ListValue>());
      } else {
        dtc_iter->second->GetAllNodes(base::BindRepeating(
            &GetAllNodesRequestHelper::OnReceivedNodesForType, helper));
      }
    } else {
      // Control Types.
      helper->OnReceivedNodesForType(
          type,
          syncer::DirectoryDataTypeController::GetAllNodesForTypeFromDirectory(
              type, GetUserShare()->directory.get()));
    }
  }
}

CoreAccountInfo ProfileSyncService::GetAuthenticatedAccountInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->GetActiveAccountInfo().account_info;
}

bool ProfileSyncService::IsAuthenticatedAccountPrimary() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->GetActiveAccountInfo().is_primary;
}

void ProfileSyncService::SetInvalidationsForSessionsEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_initialized_)
    engine_->SetInvalidationsForSessionsEnabled(enabled);
}

base::WeakPtr<syncer::JsController> ProfileSyncService::GetJsController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_js_controller_.AsWeakPtr();
}

void ProfileSyncService::StopAndClear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This can happen if the user had disabled sync before and is now setting up
  // sync again but hits the "Cancel" button on the confirmation dialog.
  // TODO(crbug.com/906034): Maybe we can streamline the defaults and the
  // behavior on setting up sync so that either this whole early return goes
  // away or it treats all "Cancel the confirmation" cases?
  if (!user_settings_->IsSyncRequested()) {
    StopImpl(CLEAR_DATA);
    return;
  }

  // We need to remember that clearing of data is needed when sync will be
  // stopped. This flag is cleared in OnSyncRequestedPrefChange() where sync
  // gets stopped. This happens synchronously when |user_settings_| get changed
  // below.
  DCHECK(!is_stopping_and_clearing_);
  is_stopping_and_clearing_ = true;
  user_settings_->SetSyncRequested(false);
  DCHECK(!is_stopping_and_clearing_);
}

void ProfileSyncService::ReconfigureDatatypeManager(
    bool bypass_setup_in_progress_check) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If we haven't initialized yet, don't configure the DTM as it could cause
  // association to start before a Directory has even been created.
  if (engine_initialized_) {
    DCHECK(engine_);
    // Don't configure datatypes if the setup UI is still on the screen - this
    // is to help multi-screen setting UIs (like iOS) where they don't want to
    // start syncing data until the user is done configuring encryption options,
    // etc. ReconfigureDatatypeManager() will get called again once the last
    // SyncSetupInProgressHandle is released.
    if (CanConfigureDataTypes(bypass_setup_in_progress_check)) {
      ConfigureDataTypeManager(syncer::CONFIGURE_REASON_RECONFIGURATION);
    } else {
      DVLOG(0) << "ConfigureDataTypeManager not invoked because datatypes "
               << "cannot be configured now";
      // If we can't configure the data type manager yet, we should still notify
      // observers. This is to support multiple setup UIs being open at once.
      NotifyObservers();
    }
  } else if (HasDisableReason(DISABLE_REASON_UNRECOVERABLE_ERROR)) {
    // There is nothing more to configure. So inform the listeners,
    NotifyObservers();

    DVLOG(1) << "ConfigureDataTypeManager not invoked because of an "
             << "Unrecoverable error.";
  } else {
    DVLOG(0) << "ConfigureDataTypeManager not invoked because engine is not "
             << "initialized";
  }
}

bool ProfileSyncService::IsRetryingAccessTokenFetchForTest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->IsRetryingAccessTokenFetchForTest();
}

std::string ProfileSyncService::GetAccessTokenForTest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->access_token();
}

syncer::SyncTokenStatus ProfileSyncService::GetSyncTokenStatus() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->GetSyncTokenStatus();
}

void ProfileSyncService::OverrideNetworkResourcesForTest(
    std::unique_ptr<syncer::NetworkResources> network_resources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the engine has already been created, then it holds a pointer to the
  // previous |network_resources_| which will become invalid. In that case, shut
  // down and recreate the engine, so that it gets the correct (overridden)
  // NetworkResources.
  // This is a horrible hack; the proper fix would be to inject the
  // NetworkResources in the ctor instead of adding them retroactively.
  bool restart = false;
  if (engine_) {
    StopImpl(KEEP_DATA);
    restart = true;
  }
  DCHECK(!engine_);

  // If a previous request (with the wrong network resources) already failed,
  // the next one would be backed off, which breaks tests. So reset the backoff.
  auth_manager_->ResetRequestAccessTokenBackoffForTest();

  network_resources_ = std::move(network_resources);

  if (restart) {
    startup_controller_->TryStart(/*force_immediate=*/true);
    DCHECK(engine_);
  }
}

void ProfileSyncService::UpdateFirstSyncTimePref() {
  if (!IsLocalSyncEnabled() && !IsSignedIn()) {
    sync_prefs_.ClearFirstSyncTime();
  } else if (sync_prefs_.GetFirstSyncTime().is_null()) {
    // Set if not set before and it's syncing now.
    sync_prefs_.SetFirstSyncTime(base::Time::Now());
  }
}

void ProfileSyncService::FlushDirectory() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // engine_initialized_ implies engine_ isn't null and the manager exists.
  // If sync is not initialized yet, we fail silently.
  if (engine_initialized_)
    engine_->FlushDirectory();
}

bool ProfileSyncService::IsPassphrasePrompted() const {
  return sync_prefs_.IsPassphrasePrompted();
}

void ProfileSyncService::SetPassphrasePrompted(bool prompted) {
  sync_prefs_.SetPassphrasePrompted(prompted);
}

scoped_refptr<base::SingleThreadTaskRunner>
ProfileSyncService::GetSyncThreadTaskRunnerForTest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_thread_ ? sync_thread_->task_runner() : nullptr;
}

syncer::SyncEncryptionHandler::Observer*
ProfileSyncService::GetEncryptionObserverForTest() {
  return &crypto_;
}

void ProfileSyncService::RemoveClientFromServer() const {
  if (!engine_initialized_)
    return;
  const std::string cache_guid = sync_prefs_.GetCacheGuid();
  DCHECK(!cache_guid.empty());
  std::string birthday;
  syncer::UserShare* user_share = GetUserShare();
  if (user_share && user_share->directory.get()) {
    birthday = user_share->directory->store_birthday();
  }
  const std::string& access_token = auth_manager_->access_token();
  if (!access_token.empty() && !birthday.empty()) {
    sync_stopped_reporter_->ReportSyncStopped(access_token, cache_guid,
                                              birthday);
  }
}

void ProfileSyncService::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    sync_prefs_.SetMemoryPressureWarningCount(
        sync_prefs_.GetMemoryPressureWarningCount() + 1);
  }
}

void ProfileSyncService::ReportPreviousSessionMemoryWarningCount() {
  int warning_received = sync_prefs_.GetMemoryPressureWarningCount();

  if (-1 != warning_received) {
    // -1 means it is new client.
    if (!sync_prefs_.DidSyncShutdownCleanly()) {
      UMA_HISTOGRAM_COUNTS_1M("Sync.MemoryPressureWarningBeforeUncleanShutdown",
                              warning_received);
    } else {
      UMA_HISTOGRAM_COUNTS_1M("Sync.MemoryPressureWarningBeforeCleanShutdown",
                              warning_received);
    }
  }
  sync_prefs_.SetMemoryPressureWarningCount(0);
  // Will set to true during a clean shutdown, so crash or something else will
  // remain this as false.
  sync_prefs_.SetCleanShutdown(false);
}

void ProfileSyncService::RecordMemoryUsageHistograms() {
  syncer::ModelTypeSet active_types = GetActiveDataTypes();
  for (syncer::ModelType type : active_types) {
    auto dtc_it = data_type_controllers_.find(type);
    if (dtc_it != data_type_controllers_.end() &&
        dtc_it->second->state() != syncer::DataTypeController::NOT_RUNNING) {
      // It's possible that a data type is considered active, but its
      // DataTypeController is still NOT_RUNNING, in the case where we're
      // waiting for a custom passphrase.
      dtc_it->second->RecordMemoryUsageAndCountsHistograms();
    }
  }
}

const GURL& ProfileSyncService::sync_service_url() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_service_url_;
}

std::string ProfileSyncService::unrecoverable_error_message() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return unrecoverable_error_message_;
}

base::Location ProfileSyncService::unrecoverable_error_location() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return unrecoverable_error_location_;
}

void ProfileSyncService::OnSetupInProgressHandleDestroyed() {
  DCHECK_GT(outstanding_setup_in_progress_handles_, 0);

  --outstanding_setup_in_progress_handles_;

  if (engine_initialized_) {
    // The user closed a setup UI, and will expect their changes to actually
    // take effect now. So we reconfigure here even if another setup UI happens
    // to be open right now.
    ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/true);
  }

  NotifyObservers();
}

void ProfileSyncService::ReconfigureDueToPassphrase(
    syncer::ConfigureReason reason) {
  if (CanConfigureDataTypes(/*bypass_setup_in_progress_check=*/false)) {
    DCHECK(data_type_manager_->IsNigoriEnabled());
    ConfigureDataTypeManager(reason);
  }
  // Notify observers that the passphrase status may have changed, regardless of
  // whether we triggered configuration or not. This is needed for the
  // IsSetupInProgress() case where the UI needs to be updated to reflect that
  // the passphrase was accepted (https://crbug.com/870256).
  NotifyObservers();
}

}  // namespace browser_sync
