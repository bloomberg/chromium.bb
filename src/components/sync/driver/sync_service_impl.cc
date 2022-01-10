// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_impl.h"

#include <cstddef>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/driver/backend_migrator.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_auth_manager.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_type_preference_provider.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/engine_components_factory_impl.h"
#include "components/sync/engine/net/http_bridge.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/invalidations/switches.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/type_entities_count.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

namespace {

// Allows refreshing the recoverability state from the server when a persistent
// auth error is resolved.
const base::Feature kTrustedVaultUpdateRecoverabilityUponResolvedAuthError{
    "TrustedVaultUpdateRecoverabilityUponResolvedAuthError",
    base::FEATURE_ENABLED_BY_DEFAULT};

// The initial state of sync, for the Sync.InitialState histogram. Even if
// this value is CAN_START, sync startup might fail for reasons that we may
// want to consider logging in the future, such as a passphrase needed for
// decryption, or the version of Chrome being too old. This enum is used to
// back a UMA histogram, and should therefore be treated as append-only.
enum SyncInitialState {
  CAN_START = 0,                // Sync can attempt to start up.
  NOT_SIGNED_IN = 1,            // There is no signed in user.
  NOT_REQUESTED = 2,            // The user turned off sync.
  NOT_REQUESTED_NOT_SETUP = 3,  // The user turned off sync and setup completed
                                // is false. Might indicate a stop-and-clear.
  NEEDS_CONFIRMATION = 4,       // The user must confirm sync settings.
  NOT_ALLOWED_BY_POLICY = 5,    // Sync is disallowed by enterprise policy.
  OBSOLETE_NOT_ALLOWED_BY_PLATFORM = 6,
  kMaxValue = OBSOLETE_NOT_ALLOWED_BY_PLATFORM
};

void RecordSyncInitialState(SyncService::DisableReasonSet disable_reasons,
                            bool first_setup_complete) {
  SyncInitialState sync_state = CAN_START;
  if (disable_reasons.Has(SyncService::DISABLE_REASON_NOT_SIGNED_IN)) {
    sync_state = NOT_SIGNED_IN;
  } else if (disable_reasons.Has(
                 SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    sync_state = NOT_ALLOWED_BY_POLICY;
  } else if (disable_reasons.Has(SyncService::DISABLE_REASON_USER_CHOICE)) {
    if (first_setup_complete) {
      sync_state = NOT_REQUESTED;
    } else {
      sync_state = NOT_REQUESTED_NOT_SETUP;
    }
  } else if (!first_setup_complete) {
    sync_state = NEEDS_CONFIRMATION;
  }
  base::UmaHistogramEnumeration("Sync.InitialState", sync_state);
}

EngineComponentsFactory::Switches EngineSwitchesFromCommandLine() {
  EngineComponentsFactory::Switches factory_switches = {
      EngineComponentsFactory::BACKOFF_NORMAL};

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kSyncShortInitialRetryOverride)) {
    factory_switches.backoff_override =
        EngineComponentsFactory::BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE;
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
    ModelType type = controller->type();
    DCHECK_EQ(0U, type_map.count(type));
    type_map[type] = std::move(controller);
  }
  return type_map;
}

std::unique_ptr<HttpPostProviderFactory> CreateHttpBridgeFactory(
    const std::string& user_agent,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory) {
  return std::make_unique<HttpBridgeFactory>(
      user_agent, std::move(pending_url_loader_factory));
}

}  // namespace

SyncServiceImpl::InitParams::InitParams() = default;
SyncServiceImpl::InitParams::InitParams(InitParams&& other) = default;
SyncServiceImpl::InitParams::~InitParams() = default;

SyncServiceImpl::SyncServiceImpl(InitParams init_params)
    : sync_client_(std::move(init_params.sync_client)),
      sync_prefs_(sync_client_->GetPrefService()),
      identity_manager_(init_params.identity_manager),
      auth_manager_(std::make_unique<SyncAuthManager>(
          identity_manager_,
          base::BindRepeating(&SyncServiceImpl::AccountStateChanged,
                              base::Unretained(this)),
          base::BindRepeating(&SyncServiceImpl::CredentialsChanged,
                              base::Unretained(this)))),
      channel_(init_params.channel),
      debug_identifier_(init_params.debug_identifier),
      sync_service_url_(
          GetSyncServiceURL(*base::CommandLine::ForCurrentProcess(), channel_)),
      crypto_(this, sync_client_->GetTrustedVaultClient()),
      url_loader_factory_(std::move(init_params.url_loader_factory)),
      network_connection_tracker_(init_params.network_connection_tracker),
      is_first_time_sync_configure_(false),
      sync_disabled_by_admin_(false),
      expect_sync_configuration_aborted_(false),
      create_http_post_provider_factory_cb_(
          base::BindRepeating(&CreateHttpBridgeFactory)),
      start_behavior_(init_params.start_behavior),
      is_setting_sync_requested_(false),
      should_record_trusted_vault_error_shown_on_startup_(true),
#if defined(OS_ANDROID)
      sessions_invalidations_enabled_(false) {
#else
      sessions_invalidations_enabled_(true) {
#endif
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sync_client_);
  DCHECK(IsLocalSyncEnabled() || identity_manager_ != nullptr);

  // If Sync is disabled via command line flag, then SyncServiceImpl
  // shouldn't be instantiated.
  DCHECK(switches::IsSyncAllowedByFlag());

  bool should_wait_for_policies =
      base::FeatureList::IsEnabled(switches::kSyncRequiresPoliciesLoaded);

  startup_controller_ = std::make_unique<StartupController>(
      base::BindRepeating(&SyncServiceImpl::GetPreferredDataTypes,
                          base::Unretained(this)),
      base::BindRepeating(&SyncServiceImpl::IsEngineAllowedToRun,
                          base::Unretained(this)),
      base::BindRepeating(&SyncServiceImpl::StartUpSlowEngineComponents,
                          base::Unretained(this)),
      should_wait_for_policies ? init_params.policy_service.get() : nullptr);

  sync_stopped_reporter_ = std::make_unique<SyncStoppedReporter>(
      sync_service_url_, MakeUserAgentForSync(channel_), url_loader_factory_,
      SyncStoppedReporter::ResultCallback());

  if (identity_manager_)
    identity_manager_->AddObserver(this);
}

SyncServiceImpl::~SyncServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (identity_manager_)
    identity_manager_->RemoveObserver(this);
  sync_prefs_.RemoveSyncPrefObserver(this);
  // Shutdown() should have been called before destruction.
  DCHECK(!engine_);
}

void SyncServiceImpl::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.emplace();

  // TODO(mastiz): The controllers map should be provided as argument.
  data_type_controllers_ =
      BuildDataTypeControllerMap(sync_client_->CreateDataTypeControllers(this));

  user_settings_ = std::make_unique<SyncUserSettingsImpl>(
      &crypto_, &sync_prefs_, sync_client_->GetPreferenceProvider(),
      GetRegisteredDataTypes());

  sync_prefs_.AddSyncPrefObserver(this);

  if (!IsLocalSyncEnabled()) {
    auth_manager_->RegisterForAuthNotifications();

    SyncInvalidationsService* sync_invalidations_service =
        sync_client_->GetSyncInvalidationsService();
    if (sync_invalidations_service) {
      sync_invalidations_service->SetActive(IsSignedIn());
      // Trigger a refresh when additional data types get enabled for
      // invalidations. This is needed to get the latest data after subscribing
      // for the updates.
      sync_invalidations_service
          ->SetCommittedAdditionalInterestedDataTypesCallback(
              base::BindRepeating(&SyncServiceImpl::TriggerRefresh,
                                  weak_factory_.GetWeakPtr()));
    }
  }

  // If sync is disabled permanently, clean up old data that may be around (e.g.
  // crash during signout).
  if (HasDisableReason(DISABLE_REASON_ENTERPRISE_POLICY) ||
      (HasDisableReason(DISABLE_REASON_NOT_SIGNED_IN) &&
       auth_manager_->IsActiveAccountInfoFullyLoaded())) {
    StopAndClear();
  }

  // Note: We need to record the initial state *after* calling
  // RegisterForAuthNotifications(), because before that the authenticated
  // account isn't initialized.
  RecordSyncInitialState(GetDisableReasons(),
                         user_settings_->IsFirstSetupComplete());

  if (!HasSyncConsent()) {
    // Remove after 11/2021. Migration logic to set SyncRequested to false if
    // the user is signed-out or signed-in but not syncing (crbug.com/1147026).
    user_settings_->SetSyncRequested(false);

#if defined(OS_ANDROID)
    // If Sync gets turned on, it should be in the decoupled state.
    sync_prefs_.SetDecoupledFromAndroidMasterSync();
#endif  // defined(OS_ANDROID)
  }

  // Auto-start means the first time the profile starts up, sync should start up
  // immediately. Since IsSyncRequested() is false by default and nobody else
  // will set it, we need to set it here.
  // Local Sync bypasses the IsSyncRequested() check, so no need to set it in
  // that case.
  // TODO(crbug.com/920158): Get rid of AUTO_START and remove this workaround.
  if (start_behavior_ == AUTO_START && !IsLocalSyncEnabled()) {
    user_settings_->SetSyncRequestedIfNotSetExplicitly();
  }
  bool force_immediate = (start_behavior_ == AUTO_START &&
                          !HasDisableReason(DISABLE_REASON_USER_CHOICE) &&
                          !user_settings_->IsFirstSetupComplete());
  startup_controller_->TryStart(force_immediate);
}

void SyncServiceImpl::StartSyncingWithServer() {
  DCHECK(startup_controller_->ArePoliciesReady());
  if (engine_)
    engine_->StartSyncingWithServer();
  if (IsLocalSyncEnabled()) {
    TriggerRefresh(ModelTypeSet::All());
  }
}

ModelTypeSet SyncServiceImpl::GetRegisteredDataTypesForTest() const {
  return GetRegisteredDataTypes();
}

void SyncServiceImpl::GetThrottledDataTypesForTest(
    base::OnceCallback<void(ModelTypeSet)> cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_ || !engine_->IsInitialized()) {
    std::move(cb).Run(ModelTypeSet());
    return;
  }

  engine_->GetThrottledDataTypesForTest(std::move(cb));
}

void SyncServiceImpl::TriggerPoliciesLoadedForTest() {
  if (!startup_controller_->ArePoliciesReady()) {
    startup_controller_->OnFirstPoliciesLoaded(
        policy::PolicyDomain::POLICY_DOMAIN_CHROME);
  }
}

bool SyncServiceImpl::IsDataTypeControllerRunningForTest(ModelType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = data_type_controllers_.find(type);
  if (iter == data_type_controllers_.end()) {
    return false;
  }
  return iter->second->state() == DataTypeController::RUNNING;
}

void SyncServiceImpl::AccountStateChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if defined(OS_ANDROID)
  // Sync and master sync should only remain coupled if the former stays enabled
  // and the latter disabled. Upon sign-out set the pref so they are decoupled
  // on the next time Sync is turned on.
  if (!HasSyncConsent()) {
    sync_prefs_.SetDecoupledFromAndroidMasterSync();
  }
#endif  // defined(OS_ANDROID)

  if (!IsSignedIn()) {
    // The account was signed out, so shut down.
    sync_disabled_by_admin_ = false;
    StopAndClear();
    DCHECK(!engine_);
  } else {
    // Either a new account was signed in, or the existing account's
    // |is_sync_consented| bit was changed. Start up or reconfigure.
    if (!engine_) {
      // Note: We only get here after an actual sign-in (not during browser
      // startup with an existing signed-in account), so no need for deferred
      // startup.
      startup_controller_->TryStart(/*force_immediate=*/true);
    } else {
      ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/false);
    }
  }

  // Propagate the (potentially) changed account state to the invalidations
  // system.
  SyncInvalidationsService* sync_invalidations_service =
      sync_client_->GetSyncInvalidationsService();
  if (sync_invalidations_service) {
    sync_invalidations_service->SetActive(IsSignedIn());
  }
}

void SyncServiceImpl::CredentialsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the engine isn't allowed to start anymore due to the credentials change,
  // then shut down. This happens when the user signs out on the web, i.e. we're
  // in the "Sync paused" state.
  if (!IsEngineAllowedToRun()) {
    // If the engine currently exists, then ResetEngine() will notify observers
    // anyway. Otherwise, notify them here. (One relevant case is when entering
    // the PAUSED state before the engine was created, e.g. during deferred
    // startup.)
    if (!engine_) {
      NotifyObservers();
    }
    ResetEngine(ShutdownReason::STOP_SYNC_AND_KEEP_DATA,
                ResetEngineReason::kCredentialsChanged);
    return;
  }

  if (!engine_) {
    startup_controller_->TryStart(/*force_immediate=*/true);
  } else {
    // If the engine already exists, just propagate the new credentials.
    SyncCredentials credentials = auth_manager_->GetCredentials();
    if (credentials.access_token.empty()) {
      engine_->InvalidateCredentials();
    } else {
      engine_->UpdateCredentials(credentials);
    }
  }

  NotifyObservers();
}

bool SyncServiceImpl::IsEngineAllowedToRun() const {
  // USER_CHOICE (i.e. the Sync feature toggle) and PLATFORM_OVERRIDE (i.e.
  // Android's "MasterSync" toggle) do not prevent starting up the Sync
  // transport.
  auto disable_reasons = GetDisableReasons();
  disable_reasons.RemoveAll(SyncService::DisableReasonSet(
      DISABLE_REASON_USER_CHOICE, DISABLE_REASON_PLATFORM_OVERRIDE));
  return disable_reasons.Empty() && !auth_manager_->IsSyncPaused();
}

void SyncServiceImpl::OnProtocolEvent(const ProtocolEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : protocol_event_observers_)
    observer.OnProtocolEvent(event);
}

void SyncServiceImpl::OnDataTypeRequestsSyncStartup(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(UserTypes().Has(type));

  if (!GetPreferredDataTypes().Has(type)) {
    // We can get here as datatype SyncableServices are typically wired up
    // to the native datatype even if sync isn't enabled.
    DVLOG(1) << "Dropping sync startup request because type "
             << ModelTypeToString(type) << "not enabled.";
    return;
  }

  if (engine_) {
    DVLOG(1) << "A data type requested sync startup, but it looks like "
                "something else beat it to the punch.";
    return;
  }

  startup_controller_->OnDataTypeRequestsSyncStartup(type);
}

void SyncServiceImpl::StartUpSlowEngineComponents() {
  DCHECK(IsEngineAllowedToRun());

  const CoreAccountInfo authenticated_account_info = GetAccountInfo();

  if (IsLocalSyncEnabled()) {
    // With local sync (roaming profiles) there is no identity manager and hence
    // |authenticated_account_info| is empty. This is required for
    // IsLocalSyncTransportDataValid() to work properly.
    DCHECK(authenticated_account_info.gaia.empty());
    DCHECK(authenticated_account_info.account_id.empty());
  } else {
    // Except for local sync (roaming profiles), the user must be signed in for
    // sync to start.
    DCHECK(!authenticated_account_info.gaia.empty());
    DCHECK(!authenticated_account_info.account_id.empty());
  }

  engine_ = sync_client_->GetSyncApiComponentFactory()->CreateSyncEngine(
      debug_identifier_, sync_client_->GetInvalidationService(),
      sync_client_->GetSyncInvalidationsService());
  DCHECK(engine_);

  // Clear any old errors the first time sync starts.
  if (!user_settings_->IsFirstSetupComplete()) {
    last_actionable_error_ = SyncProtocolError();
  }

  SyncEngine::InitParams params;
  params.host = this;
  params.encryption_observer_proxy = crypto_.GetEncryptionObserverProxy();

  params.extensions_activity = sync_client_->GetExtensionsActivity();
  params.service_url = sync_service_url_;
  params.http_factory_getter = base::BindOnce(
      create_http_post_provider_factory_cb_, MakeUserAgentForSync(channel_),
      url_loader_factory_->Clone());
  params.authenticated_account_info = authenticated_account_info;

  invalidation::InvalidationService* invalidator =
      sync_client_->GetInvalidationService();
  params.invalidator_client_id =
      invalidator ? invalidator->GetInvalidatorClientId() : std::string();

  params.sync_manager_factory =
      std::make_unique<SyncManagerFactory>(network_connection_tracker_);
  if (sync_prefs_.IsLocalSyncEnabled()) {
    params.enable_local_sync_backend = true;
    params.local_sync_backend_folder =
        sync_client_->GetLocalSyncBackendFolder();
  }
  params.engine_components_factory =
      std::make_unique<EngineComponentsFactoryImpl>(
          EngineSwitchesFromCommandLine());
  params.encryption_bootstrap_token = sync_prefs_.GetEncryptionBootstrapToken();

  if (!IsLocalSyncEnabled()) {
    auth_manager_->ConnectionOpened();
  }

  engine_->Initialize(std::move(params));
}

void SyncServiceImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyShutdown();
  ResetEngine(ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA,
              ResetEngineReason::kShutdown);

  DCHECK(!data_type_manager_);
  data_type_controllers_.clear();

  // All observers must be gone now: All KeyedServices should have unregistered
  // their observers already before, in their own Shutdown(), and all others
  // should have done it now when they got the shutdown notification.
  // (Note that destroying the ObserverList triggers its "check_empty" check.)
  observers_.reset();

  // TODO(crbug.com/1182175): Recreating the ObserverList here shouldn't be
  // necessary (it's not allowed to add observers after Shutdown()), but some
  // tests call Shutdown() twice, which breaks in NotifyShutdown() if the
  // ObserverList doesn't exist.
  observers_.emplace();

  auth_manager_.reset();
}

void SyncServiceImpl::ResetEngine(ShutdownReason shutdown_reason,
                                  ResetEngineReason reset_reason) {
  if (!engine_) {
    // If the engine hasn't started or is already shut down when a DISABLE_SYNC
    // happens, the Directory needs to be cleaned up here.
    if (shutdown_reason == ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA) {
      sync_client_->GetSyncApiComponentFactory()->ClearAllTransportData();
    }
    return;
  }

  base::UmaHistogramEnumeration("Sync.ResetEngineReason", reset_reason);
  switch (shutdown_reason) {
    case ShutdownReason::STOP_SYNC_AND_KEEP_DATA:
    case ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA:
      RemoveClientFromServer();
      break;
    case ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA:
      break;
  }

  // First, we spin down the engine to stop change processing as soon as
  // possible.
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
      data_type_manager_->Stop(shutdown_reason);
    }
    data_type_manager_.reset();
  }

  // Shutdown the migrator before the engine to ensure it doesn't pull a null
  // snapshot.
  migrator_.reset();

  engine_->Shutdown(shutdown_reason);
  engine_.reset();

  sync_enabled_weak_factory_.InvalidateWeakPtrs();

  startup_controller_->Reset();

  // Clear various state.
  crypto_.Reset();
  expect_sync_configuration_aborted_ = false;
  last_snapshot_ = SyncCycleSnapshot();

  if (!IsLocalSyncEnabled()) {
    auth_manager_->ConnectionClosed();
  }

  NotifyObservers();

  // Now that everything is shut down, try to start up again.
  switch (shutdown_reason) {
    case ShutdownReason::STOP_SYNC_AND_KEEP_DATA:
    case ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA:
      // If Sync is being stopped (either temporarily or permanently),
      // immediately try to start up again. Note that this might start only the
      // transport mode, or it might not start anything at all if something is
      // preventing Sync startup (e.g. the user signed out).
      // Note that TryStart() is guaranteed to *not* have a synchronous effect
      // (it posts a task).
      startup_controller_->TryStart(/*force_immediate=*/true);
      break;
    case ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA:
      // The only exception is browser shutdown: In this case, there's clearly
      // no point in starting up again.
      break;
  }
}

SyncUserSettings* SyncServiceImpl::GetUserSettings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_settings_.get();
}

const SyncUserSettings* SyncServiceImpl::GetUserSettings() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_settings_.get();
}

SyncService::DisableReasonSet SyncServiceImpl::GetDisableReasons() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If Sync is disabled via command line flag, then SyncServiceImpl
  // shouldn't even be instantiated.
  DCHECK(switches::IsSyncAllowedByFlag());
  DisableReasonSet result;
  if (!sync_allowed_by_platform_) {
    result.Put(DISABLE_REASON_PLATFORM_OVERRIDE);
  }

  // If local sync is enabled, most disable reasons don't apply.
  if (!IsLocalSyncEnabled()) {
    if (sync_prefs_.IsManaged() || sync_disabled_by_admin_) {
      result.Put(DISABLE_REASON_ENTERPRISE_POLICY);
    }
    if (!IsSignedIn()) {
      result.Put(DISABLE_REASON_NOT_SIGNED_IN);
    }
    if (!user_settings_->IsSyncRequested()) {
      result.Put(DISABLE_REASON_USER_CHOICE);
    }
  }

  if (unrecoverable_error_reason_) {
    result.Put(DISABLE_REASON_UNRECOVERABLE_ERROR);
  }
  return result;
}

SyncService::TransportState SyncServiceImpl::GetTransportState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsEngineAllowedToRun()) {
    // We generally shouldn't have an engine while in a disabled state, but it
    // can happen if this method gets called during ResetEngine().
    return auth_manager_->IsSyncPaused() ? TransportState::PAUSED
                                         : TransportState::DISABLED;
  }

  if (!engine_ || !engine_->IsInitialized()) {
    switch (startup_controller_->GetState()) {
        // Note: If the engine is allowed to run, then we should generally have
        // kicked off the startup process already, so NOT_STARTED should be
        // impossible here. But it can happen during browser shutdown.
      case StartupController::State::NOT_STARTED:
      case StartupController::State::STARTING_DEFERRED:
        DCHECK(!engine_);
        return TransportState::START_DEFERRED;
      case StartupController::State::STARTED:
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

void SyncServiceImpl::NotifyObservers() {
  for (auto& observer : *observers_) {
    observer.OnStateChanged(this);
  }
}

void SyncServiceImpl::NotifySyncCycleCompleted() {
  for (auto& observer : *observers_)
    observer.OnSyncCycleCompleted(this);
}

void SyncServiceImpl::NotifyShutdown() {
  for (auto& observer : *observers_)
    observer.OnSyncShutdown(this);
}

void SyncServiceImpl::ClearUnrecoverableError() {
  unrecoverable_error_reason_ = absl::nullopt;
  unrecoverable_error_message_.clear();
  unrecoverable_error_location_ = base::Location();
}

void SyncServiceImpl::OnUnrecoverableErrorImpl(
    const base::Location& from_here,
    const std::string& message,
    UnrecoverableErrorReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  unrecoverable_error_reason_ = reason;
  unrecoverable_error_message_ = message;
  unrecoverable_error_location_ = from_here;

  LOG(ERROR) << "Unrecoverable error detected at " << from_here.ToString()
             << " -- SyncServiceImpl unusable: " << message;

  // Shut the Sync machinery down. The existence of
  // |unrecoverable_error_reason_| and thus |DISABLE_REASON_UNRECOVERABLE_ERROR|
  // will prevent Sync from starting up again (even in transport-only mode).
  ResetEngine(ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA,
              ResetEngineReason::kUnrecoverableError);
}

void SyncServiceImpl::DataTypePreconditionChanged(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_ || !engine_->IsInitialized() || !data_type_manager_)
    return;
  data_type_manager_->DataTypePreconditionChanged(type);
}

void SyncServiceImpl::OnEngineInitialized(
    const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
    bool success,
    bool is_first_time_sync_configure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(treib): Based on some crash reports, it seems like the user could have
  // signed out already at this point, so many of the steps below, including
  // datatype reconfiguration, should not be triggered.
  DCHECK(IsEngineAllowedToRun());

  // The very first time the backend initializes is effectively the first time
  // we can say we successfully "synced".
  is_first_time_sync_configure_ = is_first_time_sync_configure;

  if (!success) {
    // Something went unexpectedly wrong.  Play it safe: stop syncing at once
    // and surface error UI to alert the user sync has stopped.
    OnUnrecoverableErrorImpl(FROM_HERE, "BackendInitialize failure",
                             ERROR_REASON_ENGINE_INIT_FAILURE);
    return;
  }

  if (!protocol_event_observers_.empty()) {
    engine_->RequestBufferedProtocolEventsAndEnableForwarding();
  }

  data_type_manager_ =
      sync_client_->GetSyncApiComponentFactory()->CreateDataTypeManager(
          debug_info_listener, &data_type_controllers_, &crypto_, engine_.get(),
          this);

  crypto_.SetSyncEngine(GetAccountInfo(), engine_.get());

  // Auto-start means IsFirstSetupComplete gets set automatically.
  if (start_behavior_ == AUTO_START &&
      !user_settings_->IsFirstSetupComplete()) {
    // This will trigger a configure if it completes setup.
    user_settings_->SetFirstSetupComplete(
        SyncFirstSetupCompleteSource::ENGINE_INITIALIZED_WITH_AUTO_START);
  } else if (CanConfigureDataTypes(/*bypass_setup_in_progress_check=*/false)) {
    // Datatype downloads on restart are generally due to newly supported
    // datatypes (although it's also possible we're picking up where a failed
    // previous configuration left off).
    // TODO(sync): consider detecting configuration recovery and setting
    // the reason here appropriately.
    ConfigureDataTypeManager(CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE);
  }

  // Check for a cookie jar mismatch.
  if (identity_manager_) {
    signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
        identity_manager_->GetAccountsInCookieJar();
    if (accounts_in_cookie_jar_info.accounts_are_fresh) {
      OnAccountsInCookieUpdated(accounts_in_cookie_jar_info,
                                GoogleServiceAuthError::AuthErrorNone());
    }
  }

  NotifyObservers();
}

void SyncServiceImpl::OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  last_snapshot_ = snapshot;

  DVLOG(2) << "Notifying observers sync cycle completed";
  NotifySyncCycleCompleted();
}

void SyncServiceImpl::OnConnectionStatusChange(ConnectionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsLocalSyncEnabled()) {
    auth_manager_->ConnectionStatusChanged(status);
  }
  NotifyObservers();
}

void SyncServiceImpl::OnMigrationNeededForTypes(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(engine_);
  DCHECK(engine_->IsInitialized());
  DCHECK(data_type_manager_);

  // Migrator must be valid, because we don't sync until it is created and this
  // callback originates from a sync cycle.
  migrator_->MigrateTypes(types);
}

void SyncServiceImpl::OnActionableError(const SyncProtocolError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_actionable_error_ = error;
  DCHECK_NE(last_actionable_error_.action, UNKNOWN_ACTION);
  switch (error.action) {
    case UPGRADE_CLIENT:
      // TODO(lipalani) : if setup in progress we want to display these
      // actions in the popup. The current experience might not be optimal for
      // the user. We just dismiss the dialog.
      if (IsSetupInProgress()) {
        StopAndClear();
        expect_sync_configuration_aborted_ = true;
      }
      // Trigger an unrecoverable error to stop syncing.
      OnUnrecoverableErrorImpl(FROM_HERE,
                               last_actionable_error_.error_description,
                               ERROR_REASON_ACTIONABLE_ERROR);
      break;
    case DISABLE_SYNC_ON_CLIENT:
      if (error.error_type == NOT_MY_BIRTHDAY) {
        UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", BIRTHDAY_ERROR,
                                  STOP_SOURCE_LIMIT);
      }

      // Security domain state might be reset, reset local state as well.
      sync_client_->GetTrustedVaultClient()->ClearDataForAccount(
          GetAccountInfo());

      // Note: StopAndClear sets IsSyncRequested to false, which ensures that
      // Sync-the-feature remains off.
      StopAndClear();

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
      // TODO(https://crbug.com/1233933): Update this when Lacros profiles
      //     support signed-in-but-not-consented-to-sync state.

      // On every platform except ChromeOS, revoke the Sync consent in
      // IdentityManager after a dashboard clear.
      if (!IsLocalSyncEnabled() &&
          identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
        auto* account_mutator = identity_manager_->GetPrimaryAccountMutator();
        // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
        DCHECK(account_mutator);

        // Note: On some platforms, revoking the sync consent will also clear
        // the primary account as transitioning from ConsentLevel::kSync to
        // ConsentLevel::kSignin is not supported.
        account_mutator->RevokeSyncConsent(
            signin_metrics::SERVER_FORCED_DISABLE,
            signin_metrics::SignoutDelete::kIgnoreMetric);
      }
#endif
      break;
    case STOP_SYNC_FOR_DISABLED_ACCOUNT:
      // Sync disabled by domain admin. Stop syncing until next restart.
      sync_disabled_by_admin_ = true;
      ResetEngine(ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA,
                  ResetEngineReason::kDisabledAccount);
      break;
    case RESET_LOCAL_SYNC_DATA:
      ResetEngine(ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA,
                  ResetEngineReason::kResetLocalData);
      break;
    case UNKNOWN_ACTION:
      NOTREACHED();
  }
  NotifyObservers();
}

void SyncServiceImpl::OnBackedOffTypesChanged() {
  NotifyObservers();
}

void SyncServiceImpl::OnConfigureDone(
    const DataTypeManager::ConfigureResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_type_error_map_ = result.data_type_status_table.GetAllErrors();

  DVLOG(1) << "SyncServiceImpl::OnConfigureDone called with status: "
           << result.status;
  // The possible status values:
  //    ABORT - Configuration was aborted. This is not an error, if
  //            initiated by user.
  //    OK - Some or all types succeeded.

  // First handle the abort case.
  if (result.status == DataTypeManager::ABORTED) {
    DCHECK(expect_sync_configuration_aborted_);
    DVLOG(0) << "SyncServiceImpl sync configuration aborted";
    expect_sync_configuration_aborted_ = false;
    return;
  }

  DCHECK_EQ(DataTypeManager::OK, result.status);

  // We should never get in a state where we have no encrypted datatypes
  // enabled, and yet we still think we require a passphrase for decryption.
  DCHECK(!user_settings_->IsPassphraseRequiredForPreferredDataTypes() ||
         user_settings_->IsEncryptedDatatypeEnabled());

  // Notify listeners that configuration is done.
  for (auto& observer : *observers_)
    observer.OnSyncConfigurationCompleted(this);

  NotifyObservers();

  if (migrator_.get() && migrator_->state() != BackendMigrator::IDLE) {
    // Migration in progress.  Let the migrator know we just finished
    // configuring something.  It will be up to the migrator to call
    // StartSyncingWithServer() if migration is now finished.
    migrator_->OnConfigureDone(result);
    return;
  }

  RecordMemoryUsageAndCountsHistograms();

  StartSyncingWithServer();
}

void SyncServiceImpl::OnConfigureStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  engine_->StartConfiguration();
  NotifyObservers();
}

void SyncServiceImpl::CryptoStateChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyObservers();
}

void SyncServiceImpl::CryptoRequiredUserActionChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (should_record_trusted_vault_error_shown_on_startup_ &&
      crypto_.IsTrustedVaultKeyRequiredStateKnown() && IsSyncFeatureEnabled()) {
    should_record_trusted_vault_error_shown_on_startup_ = false;
    if (crypto_.GetPassphraseType() ==
        PassphraseType::kTrustedVaultPassphrase) {
      base::UmaHistogramBoolean(
          "Sync.TrustedVaultErrorShownOnStartup",
          user_settings_->IsTrustedVaultKeyRequiredForPreferredDataTypes());
    }
  }
}

void SyncServiceImpl::ReconfigureDataTypesDueToCrypto() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (CanConfigureDataTypes(/*bypass_setup_in_progress_check=*/false)) {
    ConfigureDataTypeManager(CONFIGURE_REASON_CRYPTO);
  }

  // Notify observers that the passphrase status may have changed, regardless of
  // whether we triggered configuration or not. This is needed for the
  // IsSetupInProgress() case where the UI needs to be updated to reflect that
  // the passphrase was accepted (https://crbug.com/870256).
  NotifyObservers();
}

void SyncServiceImpl::EncryptionBootstrapTokenChanged(
    const std::string& bootstrap_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_prefs_.SetEncryptionBootstrapToken(bootstrap_token);
}

bool SyncServiceImpl::IsSetupInProgress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return outstanding_setup_in_progress_handles_ > 0;
}

bool SyncServiceImpl::QueryDetailedSyncStatusForDebugging(
    SyncStatus* result) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_ && engine_->IsInitialized()) {
    *result = engine_->GetDetailedStatus();
    return true;
  }
  SyncStatus status;
  status.sync_protocol_error = last_actionable_error_;
  *result = status;
  return false;
}

GoogleServiceAuthError SyncServiceImpl::GetAuthError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->GetLastAuthError();
}

base::Time SyncServiceImpl::GetAuthErrorTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->GetLastAuthErrorTime();
}

bool SyncServiceImpl::RequiresClientUpgrade() const {
  return last_actionable_error_.action == UPGRADE_CLIENT;
}

bool SyncServiceImpl::CanConfigureDataTypes(
    bool bypass_setup_in_progress_check) const {
  // TODO(crbug.com/856179): Arguably, IsSetupInProgress() shouldn't prevent
  // configuring data types in transport mode, but at least for now, it's
  // easier to keep it like this. Changing this will likely require changes to
  // the setup UI flow.
  return data_type_manager_ &&
         (bypass_setup_in_progress_check || !IsSetupInProgress());
}

std::unique_ptr<SyncSetupInProgressHandle>
SyncServiceImpl::GetSetupInProgressHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (++outstanding_setup_in_progress_handles_ == 1) {
    startup_controller_->TryStart(/*force_immediate=*/true);

    NotifyObservers();
  }

  return std::make_unique<SyncSetupInProgressHandle>(
      base::BindRepeating(&SyncServiceImpl::OnSetupInProgressHandleDestroyed,
                          weak_factory_.GetWeakPtr()));
}

bool SyncServiceImpl::IsLocalSyncEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_prefs_.IsLocalSyncEnabled();
}

void SyncServiceImpl::TriggerRefresh(const ModelTypeSet& types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_ && engine_->IsInitialized()) {
    engine_->TriggerRefresh(types);
  }
}

bool SyncServiceImpl::IsSignedIn() const {
  // Sync is logged in if there is a non-empty account id.
  return !GetAccountInfo().account_id.empty();
}

base::Time SyncServiceImpl::GetLastSyncedTimeForDebugging() const {
  if (!engine_ || !engine_->IsInitialized()) {
    return base::Time();
  }

  return engine_->GetLastSyncedTimeForDebugging();
}

void SyncServiceImpl::OnPreferredDataTypesPrefChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!engine_ && !HasDisableReason(DISABLE_REASON_UNRECOVERABLE_ERROR)) {
    return;
  }

  if (data_type_manager_)
    data_type_manager_->ResetDataTypeErrors();

  ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/false);
}

SyncClient* SyncServiceImpl::GetSyncClientForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_client_.get();
}

void SyncServiceImpl::AddObserver(SyncServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_->AddObserver(observer);
}

void SyncServiceImpl::RemoveObserver(SyncServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_->RemoveObserver(observer);
}

bool SyncServiceImpl::HasObserver(const SyncServiceObserver* observer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return observers_->HasObserver(observer);
}

ModelTypeSet SyncServiceImpl::GetPreferredDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_settings_->GetPreferredDataTypes();
}

ModelTypeSet SyncServiceImpl::GetActiveDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data_type_manager_ || GetAuthError().IsPersistentError())
    return ModelTypeSet();
  return data_type_manager_->GetActiveDataTypes();
}

void SyncServiceImpl::SetSyncRequestedAndIgnoreNotification(bool is_requested) {
  // For a no-op, OnSyncRequestedPrefChange() wouldn't be called and
  // |is_setting_sync_requested_| wouldn't get reset, so check.
  if (is_requested != user_settings_->IsSyncRequested()) {
    DCHECK(!is_setting_sync_requested_);
    is_setting_sync_requested_ = true;
    user_settings_->SetSyncRequested(is_requested);
    // OnSyncRequestedPrefChange() should have cleared the flag.
    DCHECK(!is_setting_sync_requested_);
  }
}

void SyncServiceImpl::ConfigureDataTypeManager(ConfigureReason reason) {
  DCHECK(engine_);
  DCHECK(engine_->IsInitialized());
  DCHECK(!engine_->GetCacheGuid().empty());

  ConfigureContext configure_context;
  configure_context.authenticated_account_id = GetAccountInfo().account_id;
  configure_context.cache_guid = engine_->GetCacheGuid();
  configure_context.sync_mode = SyncMode::kFull;
  configure_context.reason = reason;
  configure_context.configuration_start_time = base::Time::Now();

  DCHECK(!configure_context.cache_guid.empty());

  if (!migrator_) {
    // We create the migrator at the same time.
    migrator_ = std::make_unique<BackendMigrator>(
        debug_identifier_, data_type_manager_.get(),
        base::BindRepeating(&SyncServiceImpl::ConfigureDataTypeManager,
                            base::Unretained(this), CONFIGURE_REASON_MIGRATION),
        base::BindRepeating(&SyncServiceImpl::StartSyncingWithServer,
                            base::Unretained(this)));

    // Override reason if no configuration has completed ever.
    if (is_first_time_sync_configure_) {
      configure_context.reason = CONFIGURE_REASON_NEW_CLIENT;
    }
  }

  DCHECK(!configure_context.authenticated_account_id.empty() ||
         IsLocalSyncEnabled());
  DCHECK(!configure_context.cache_guid.empty());
  DCHECK_NE(configure_context.reason, CONFIGURE_REASON_UNKNOWN);

  const bool use_transport_only_mode = UseTransportOnlyMode();

  if (use_transport_only_mode) {
    configure_context.sync_mode = SyncMode::kTransportOnly;
  }
  data_type_manager_->Configure(GetDataTypesToConfigure(), configure_context);

  UpdateDataTypesForInvalidations();

  // Record in UMA whether we're configuring the full Sync feature or only the
  // transport.
  enum class ConfigureDataTypeManagerOption {
    kFeature = 0,
    kTransport = 1,
    kMaxValue = kTransport
  };
  base::UmaHistogramEnumeration("Sync.ConfigureDataTypeManagerOption",
                                use_transport_only_mode
                                    ? ConfigureDataTypeManagerOption::kTransport
                                    : ConfigureDataTypeManagerOption::kFeature);

  // Only if it's the full Sync feature, also record the user's choice of data
  // types.
  if (!use_transport_only_mode) {
    bool sync_everything = sync_prefs_.HasKeepEverythingSynced();
    base::UmaHistogramBoolean("Sync.SyncEverything2", sync_everything);

    if (!sync_everything) {
      for (UserSelectableType type : user_settings_->GetSelectedTypes()) {
        ModelTypeForHistograms canonical_model_type = ModelTypeHistogramValue(
            UserSelectableTypeToCanonicalModelType(type));
        base::UmaHistogramEnumeration("Sync.CustomSync3", canonical_model_type);
      }
    }
  }
}

bool SyncServiceImpl::UseTransportOnlyMode() const {
  // Note: When local Sync is enabled, then we want full-sync mode (not just
  // transport), even though Sync-the-feature is not considered enabled.
  return !IsSyncFeatureEnabled() && !IsLocalSyncEnabled();
}

ModelTypeSet SyncServiceImpl::GetRegisteredDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ModelTypeSet registered_types;
  // The |data_type_controllers_| are determined by command-line flags;
  // that's effectively what controls the values returned here.
  for (const std::pair<const ModelType, std::unique_ptr<DataTypeController>>&
           type_and_controller : data_type_controllers_) {
    registered_types.Put(type_and_controller.first);
  }
  return registered_types;
}

ModelTypeSet SyncServiceImpl::GetModelTypesForTransportOnlyMode() const {
  // Collect the types from all controllers that support transport-only mode.
  ModelTypeSet allowed_types;
  for (const auto& type_and_controller : data_type_controllers_) {
    ModelType type = type_and_controller.first;
    const DataTypeController* controller = type_and_controller.second.get();
    if (controller->ShouldRunInTransportOnlyMode()) {
      allowed_types.Put(type);
    }
  }
  return allowed_types;
}

ModelTypeSet SyncServiceImpl::GetDataTypesToConfigure() const {
  ModelTypeSet types = GetPreferredDataTypes();
  // In transport-only mode, only a subset of data types is supported.
  if (UseTransportOnlyMode()) {
    types = Intersection(types, GetModelTypesForTransportOnlyMode());
  }
  return types;
}

void SyncServiceImpl::UpdateDataTypesForInvalidations() {
  SyncInvalidationsService* invalidations_service =
      sync_client_->GetSyncInvalidationsService();
  if (!invalidations_service) {
    return;
  }

  // No need to register invalidations for non-protocol or commit-only types.
  // TODO(crbug.com/1260836): This could break with dynamic proxy types.
  ModelTypeSet types = Intersection(GetDataTypesToConfigure(), ProtocolTypes());
  types.RemoveAll(CommitOnlyTypes());
  if (!sessions_invalidations_enabled_) {
    types.Remove(SESSIONS);
  }
  if (!(base::FeatureList::IsEnabled(switches::kUseSyncInvalidations) &&
        base::FeatureList::IsEnabled(
            switches::kUseSyncInvalidationsForWalletAndOffer))) {
    types.RemoveAll({AUTOFILL_WALLET_DATA, AUTOFILL_WALLET_OFFER});
  }
  invalidations_service->SetInterestedDataTypes(types);
}

SyncCycleSnapshot SyncServiceImpl::GetLastCycleSnapshotForDebugging() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_snapshot_;
}

void SyncServiceImpl::HasUnsyncedItemsForTest(
    base::OnceCallback<void(bool)> cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(engine_);
  DCHECK(engine_->IsInitialized());
  engine_->HasUnsyncedItemsForTest(std::move(cb));
}

BackendMigrator* SyncServiceImpl::GetBackendMigratorForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return migrator_.get();
}

std::unique_ptr<base::Value> SyncServiceImpl::GetTypeStatusMapForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto result = std::make_unique<base::ListValue>();

  if (!engine_ || !engine_->IsInitialized()) {
    return result;
  }

  const SyncStatus& detailed_status = engine_->GetDetailedStatus();
  const ModelTypeSet& throttled_types(detailed_status.throttled_types);
  const ModelTypeSet& backed_off_types(detailed_status.backed_off_types);

  std::unique_ptr<base::DictionaryValue> type_status_header(
      new base::DictionaryValue());
  type_status_header->SetString("status", "header");
  type_status_header->SetString("name", "Model Type");
  type_status_header->SetString("num_entries", "Total Entries");
  type_status_header->SetString("num_live", "Live Entries");
  type_status_header->SetString("message", "Message");
  type_status_header->SetString("state", "State");
  result->Append(std::move(type_status_header));

  for (const std::pair<const ModelType, std::unique_ptr<DataTypeController>>&
           type_and_controller : data_type_controllers_) {
    const ModelType type = type_and_controller.first;
    const DataTypeController* const controller =
        type_and_controller.second.get();

    auto type_status = std::make_unique<base::DictionaryValue>();
    type_status->SetString("name", ModelTypeToString(type));

    if (data_type_error_map_.find(type) != data_type_error_map_.end()) {
      const SyncError& error = data_type_error_map_.find(type)->second;
      DCHECK(error.IsSet());
      switch (error.GetSeverity()) {
        case SyncError::SYNC_ERROR_SEVERITY_ERROR:
          type_status->SetString("status", "severity_error");
          type_status->SetString(
              "message", "Error: " + error.location().ToString() + ", " +
                             error.GetMessagePrefix() + error.message());
          break;
        case SyncError::SYNC_ERROR_SEVERITY_INFO:
          type_status->SetString("status", "severity_info");
          type_status->SetString("message", error.message());
          break;
      }
    } else if (throttled_types.Has(type)) {
      type_status->SetString("status", "severity_warning");
      type_status->SetString("message", " Throttled");
    } else if (backed_off_types.Has(type)) {
      type_status->SetString("status", "severity_warning");
      type_status->SetString("message", "Backed off");
    } else {
      type_status->SetString("message", "");

      // Determine the row color based on the controller's state.
      switch (controller->state()) {
        case DataTypeController::NOT_RUNNING:
          // One common case is that the sync was just disabled by the user,
          // which is not very different to certain SYNC_ERROR_SEVERITY_INFO
          // cases like preconditions not having been met due to user
          // configuration.
          type_status->SetString("status", "severity_info");
          break;
        case DataTypeController::MODEL_STARTING:
        case DataTypeController::MODEL_LOADED:
        case DataTypeController::STOPPING:
          // These are all transitional states that should be rare to observe.
          type_status->SetString("status", "transitioning");
          break;
        case DataTypeController::RUNNING:
          type_status->SetString("status", "ok");
          break;
        case DataTypeController::FAILED:
          // Note that most of the errors (possibly all) should have been
          // handled earlier via |data_type_error_map_|.
          type_status->SetString("status", "severity_error");
          break;
      }
    }

    type_status->SetString(
        "state", DataTypeController::StateToString(controller->state()));

    result->Append(std::move(type_status));
  }
  return result;
}

void SyncServiceImpl::GetEntityCountsForDebugging(
    base::OnceCallback<void(const std::vector<TypeEntitiesCount>&)> callback)
    const {
  // The method must respond with the TypeEntitiesCount of all data types, but
  // each count request is async. The strategy is to use base::BarrierClosure()
  // to only send the final response once all types are done.
  using EntityCountsVector = std::vector<TypeEntitiesCount>;
  auto all_types_counts = std::make_unique<EntityCountsVector>();
  EntityCountsVector* all_types_counts_ptr = all_types_counts.get();
  // |respond_all_counts_callback| owns |all_types_counts|.
  auto respond_all_counts_callback = base::BindOnce(
      [](base::OnceCallback<void(const EntityCountsVector&)> callback,
         std::unique_ptr<EntityCountsVector> all_types_counts) {
        std::move(callback).Run(*all_types_counts);
      },
      std::move(callback), std::move(all_types_counts));

  // |all_types_done_barrier| runs |respond_all_counts_callback| once it's been
  // called for all types.
  base::RepeatingClosure all_types_done_barrier = base::BarrierClosure(
      data_type_controllers_.size(), std::move(respond_all_counts_callback));

  // Callbacks passed to the controllers get a non-owning reference to the
  // counts vector, which they use to push the count for their individual type.
  for (const auto& type_and_controller : data_type_controllers_) {
    type_and_controller.second->GetTypeEntitiesCount(base::BindOnce(
        [](const base::RepeatingClosure& all_types_done_barrier,
           EntityCountsVector* all_types_counts_ptr,
           const TypeEntitiesCount& count) {
          all_types_counts_ptr->push_back(count);
          all_types_done_barrier.Run();
        },
        all_types_done_barrier, all_types_counts_ptr));
  }
}

void SyncServiceImpl::OnSyncManagedPrefChange(bool is_sync_managed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Local sync is not controlled by the "sync managed" policy, so these pref
  // changes make no difference to the service state.
  if (IsLocalSyncEnabled()) {
    return;
  }

  if (is_sync_managed) {
    StopAndClear();
  } else {
    // Sync is no longer disabled by policy. Try starting it up if appropriate.
    DCHECK(!engine_);
    startup_controller_->TryStart(/*force_immediate=*/true);
  }
}

void SyncServiceImpl::OnFirstSetupCompletePrefChange(
    bool is_first_setup_complete) {
  if (engine_ && engine_->IsInitialized()) {
    ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/false);
  }
}

void SyncServiceImpl::OnSyncRequestedPrefChange(bool is_sync_requested) {
  // Ignore the notification if the service itself set the pref.
  if (is_setting_sync_requested_) {
    is_setting_sync_requested_ = false;
    return;
  }

  if (is_sync_requested) {
    // If the Sync engine was already initialized (probably running in transport
    // mode), just reconfigure.
    if (engine_ && engine_->IsInitialized()) {
      ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/false);
    } else {
      // Otherwise try to start up. Note that there might still be other disable
      // reasons remaining, in which case this will effectively do nothing.
      startup_controller_->TryStart(/*force_immediate=*/true);
    }

    NotifyObservers();
  } else {
    // This will notify the observers.
    // TODO(crbug.com/856179): Evaluate whether we can get away without a
    // full restart in this case (i.e. just reconfigure).
    ResetEngine(ShutdownReason::STOP_SYNC_AND_KEEP_DATA,
                ResetEngineReason::kRequestedPrefChange);
  }
}

void SyncServiceImpl::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  OnAccountsInCookieUpdatedWithCallback(
      accounts_in_cookie_jar_info.signed_in_accounts, base::NullCallback());
}

void SyncServiceImpl::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  if (error.state() == GoogleServiceAuthError::NONE &&
      last_error_state_of_refresh_token_.IsPersistentError() &&
      base::FeatureList::IsEnabled(
          kTrustedVaultUpdateRecoverabilityUponResolvedAuthError)) {
    // Resolving a persistent error may or may not necessarily imply changes in
    // the recoverability state. However, over-triggering
    // OnTrustedVaultRecoverabilityChanged() is totally fine and should lead to
    // no user-facing changes, either because it's a strict no-op, e.g. if the
    // passphrase type != kTrustedVaultPassphrase, or because the recoverability
    // hasn't changed.
    // TODO(crbug.com/1156584): This is only needed because currently not all
    // persistent errors stop the sync engine.
    crypto_.OnTrustedVaultRecoverabilityChanged();
  }

  last_error_state_of_refresh_token_ = error;
}

void SyncServiceImpl::OnAccountsInCookieUpdatedWithCallback(
    const std::vector<gaia::ListedAccount>& signed_in_accounts,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!engine_ || !engine_->IsInitialized())
    return;

  bool cookie_jar_mismatch = HasCookieJarMismatch(signed_in_accounts);
  bool cookie_jar_empty = signed_in_accounts.empty();

  DVLOG(1) << "Cookie jar mismatch: " << cookie_jar_mismatch;
  DVLOG(1) << "Cookie jar empty: " << cookie_jar_empty;
  engine_->OnCookieJarChanged(cookie_jar_mismatch, std::move(callback));
}

bool SyncServiceImpl::HasCookieJarMismatch(
    const std::vector<gaia::ListedAccount>& cookie_jar_accounts) {
  CoreAccountId account_id = GetAccountInfo().account_id;
  // Iterate through list of accounts, looking for current sync account.
  for (const auto& account : cookie_jar_accounts) {
    if (account.id == account_id)
      return false;
  }
  return true;
}

void SyncServiceImpl::AddProtocolEventObserver(
    ProtocolEventObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  protocol_event_observers_.AddObserver(observer);
  if (engine_) {
    engine_->RequestBufferedProtocolEventsAndEnableForwarding();
  }
}

void SyncServiceImpl::RemoveProtocolEventObserver(
    ProtocolEventObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  protocol_event_observers_.RemoveObserver(observer);
  if (engine_ && protocol_event_observers_.empty()) {
    engine_->DisableProtocolEventForwarding();
  }
}

namespace {

class GetAllNodesRequestHelper
    : public base::RefCountedThreadSafe<GetAllNodesRequestHelper> {
 public:
  GetAllNodesRequestHelper(
      ModelTypeSet requested_types,
      base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback);

  GetAllNodesRequestHelper(const GetAllNodesRequestHelper&) = delete;
  GetAllNodesRequestHelper& operator=(const GetAllNodesRequestHelper&) = delete;

  void OnReceivedNodesForType(const ModelType type,
                              std::unique_ptr<base::ListValue> node_list);

 private:
  friend class base::RefCountedThreadSafe<GetAllNodesRequestHelper>;
  virtual ~GetAllNodesRequestHelper();

  std::unique_ptr<base::ListValue> result_accumulator_;
  ModelTypeSet awaiting_types_;
  base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback_;
  SEQUENCE_CHECKER(sequence_checker_);
};

GetAllNodesRequestHelper::GetAllNodesRequestHelper(
    ModelTypeSet requested_types,
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
    const ModelType type,
    std::unique_ptr<base::ListValue> node_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Add these results to our list.
  base::DictionaryValue type_dict;
  type_dict.SetKey("type", base::Value(ModelTypeToString(type)));
  type_dict.SetKey("nodes",
                   base::Value::FromUniquePtrValue(std::move(node_list)));
  result_accumulator_->Append(std::move(type_dict));

  // Remember that this part of the request is satisfied.
  awaiting_types_.Remove(type);

  if (awaiting_types_.Empty()) {
    std::move(callback_).Run(std::move(result_accumulator_));
  }
}

}  // namespace

void SyncServiceImpl::GetAllNodesForDebugging(
    base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the engine isn't initialized yet, then there are no nodes to return.
  if (!engine_ || !engine_->IsInitialized()) {
    std::move(callback).Run(std::make_unique<base::ListValue>());
    return;
  }

  ModelTypeSet all_types = GetActiveDataTypes();
  all_types.PutAll(ControlTypes());
  scoped_refptr<GetAllNodesRequestHelper> helper =
      new GetAllNodesRequestHelper(all_types, std::move(callback));

  for (ModelType type : all_types) {
    const auto dtc_iter = data_type_controllers_.find(type);
    if (dtc_iter != data_type_controllers_.end()) {
      if (dtc_iter->second->state() == DataTypeController::NOT_RUNNING) {
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
      // We should have no data type controller only for Nigori.
      DCHECK_EQ(type, NIGORI);
      engine_->GetNigoriNodeForDebugging(base::BindOnce(
          &GetAllNodesRequestHelper::OnReceivedNodesForType, helper));
    }
  }
}

CoreAccountInfo SyncServiceImpl::GetAccountInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!auth_manager_) {
    // Some crashes on iOS (crbug.com/962384) suggest that SyncServiceImpl
    // gets called after it has been already shutdown. It's not clear why this
    // actually happens. We add this null check here to protect against such
    // crashes.
    return CoreAccountInfo();
  }
  return auth_manager_->GetActiveAccountInfo().account_info;
}

bool SyncServiceImpl::HasSyncConsent() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!auth_manager_) {
    // This is a precautionary check to be consistent with the check in
    // GetAccountInfo().
    return false;
  }
  return auth_manager_->GetActiveAccountInfo().is_sync_consented;
}

void SyncServiceImpl::SetInvalidationsForSessionsEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_ && engine_->IsInitialized()) {
    engine_->SetInvalidationsForSessionsEnabled(enabled);
  }

  sessions_invalidations_enabled_ = enabled;
  UpdateDataTypesForInvalidations();
}

void SyncServiceImpl::AddTrustedVaultDecryptionKeysFromWeb(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  sync_client_->GetTrustedVaultClient()->StoreKeys(gaia_id, keys,
                                                   last_key_version);
}

void SyncServiceImpl::AddTrustedVaultRecoveryMethodFromWeb(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure callback) {
  sync_client_->GetTrustedVaultClient()->AddTrustedRecoveryMethod(
      gaia_id, public_key, method_type_hint, std::move(callback));
}

void SyncServiceImpl::StopAndClear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ClearUnrecoverableError();
  ResetEngine(ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA,
              ResetEngineReason::kStopAndClear);
  // Note: ResetEngine(DISABLE_SYNC_AND_CLEAR_DATA) does *not* clear prefs which
  // are directly user-controlled such as the set of selected types here, so
  // that if the user ever chooses to enable Sync again, they start off with
  // their previous settings by default. We do however require going through
  // first-time setup again and set SyncRequested to false.
  sync_prefs_.ClearFirstSetupComplete();
  sync_prefs_.ClearPassphrasePromptMutedProductVersion();
  // For explicit passphrase users, clear the encryption key, such that they
  // will need to reenter it if sync gets re-enabled.
  sync_prefs_.ClearEncryptionBootstrapToken();
  SetSyncRequestedAndIgnoreNotification(false);
  // Also let observers know that Sync-the-feature is now fully disabled
  // (before it possibly starts up again in transport-only mode).
  NotifyObservers();
}

void SyncServiceImpl::SetSyncAllowedByPlatform(bool allowed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (allowed == sync_allowed_by_platform_) {
    return;
  }

  sync_allowed_by_platform_ = allowed;
  if (!sync_allowed_by_platform_) {
    // TODO(crbug.com/856179): Evaluate whether we can get away without a full
    // restart (i.e. just reconfigure). See also similar comment in
    // OnSyncRequestedPrefChange().
    ResetEngine(ShutdownReason::STOP_SYNC_AND_KEEP_DATA,
                ResetEngineReason::kSetSyncAllowedByPlatform);
  }
}

void SyncServiceImpl::ReconfigureDatatypeManager(
    bool bypass_setup_in_progress_check) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (engine_ && engine_->IsInitialized()) {
    DCHECK(engine_);
    // Don't configure datatypes if the setup UI is still on the screen - this
    // is to help multi-screen setting UIs (like iOS) where they don't want to
    // start syncing data until the user is done configuring encryption options,
    // etc. ReconfigureDatatypeManager() will get called again once the last
    // SyncSetupInProgressHandle is released.
    if (CanConfigureDataTypes(bypass_setup_in_progress_check)) {
      ConfigureDataTypeManager(CONFIGURE_REASON_RECONFIGURATION);
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

bool SyncServiceImpl::IsRetryingAccessTokenFetchForTest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->IsRetryingAccessTokenFetchForTest();
}

std::string SyncServiceImpl::GetAccessTokenForTest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->access_token();
}

SyncTokenStatus SyncServiceImpl::GetSyncTokenStatusForDebugging() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return auth_manager_->GetSyncTokenStatus();
}

void SyncServiceImpl::OverrideNetworkForTest(
    const CreateHttpPostProviderFactory& create_http_post_provider_factory_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the engine has already been created, then it has a copy of the previous
  // HttpPostProviderFactory creation callback. In that case, shut down and
  // recreate the engine, so that it uses the correct (overridden) callback.
  // This is a horrible hack; the proper fix would be to inject the
  // callback in the ctor instead of adding it retroactively.
  bool restart = false;
  if (engine_) {
    // Use BROWSER_SHUTDOWN_AND_KEEP_DATA to prevent the engine from immediately
    // restarting.
    ResetEngine(ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA,
                ResetEngineReason::kShutdown);
    restart = true;
  }
  DCHECK(!engine_);

  // If a previous request (with the wrong callback) already failed, the next
  // one would be backed off, which breaks tests. So reset the backoff.
  auth_manager_->ResetRequestAccessTokenBackoffForTest();

  create_http_post_provider_factory_cb_ = create_http_post_provider_factory_cb;

  // For allowing tests to easily reset to the default (real) callback.
  if (!create_http_post_provider_factory_cb_) {
    create_http_post_provider_factory_cb_ =
        base::BindRepeating(&CreateHttpBridgeFactory);
  }

  if (restart) {
    startup_controller_->TryStart(/*force_immediate=*/true);
  }
}

#if defined(OS_ANDROID)
void SyncServiceImpl::SetDecoupledFromAndroidMasterSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_prefs_.SetDecoupledFromAndroidMasterSync();
}

bool SyncServiceImpl::GetDecoupledFromAndroidMasterSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_prefs_.GetDecoupledFromAndroidMasterSync();
}
#endif  // defined(OS_ANDROID)

SyncEncryptionHandler::Observer*
SyncServiceImpl::GetEncryptionObserverForTest() {
  return &crypto_;
}

void SyncServiceImpl::RemoveClientFromServer() const {
  if (!engine_ || !engine_->IsInitialized()) {
    return;
  }
  const std::string cache_guid = engine_->GetCacheGuid();
  const std::string birthday = engine_->GetBirthday();
  DCHECK(!cache_guid.empty());
  const std::string& access_token = auth_manager_->access_token();
  const bool report_sync_stopped = !access_token.empty() && !birthday.empty();
  base::UmaHistogramBoolean("Sync.SyncStoppedReported", report_sync_stopped);
  if (report_sync_stopped) {
    sync_stopped_reporter_->ReportSyncStopped(access_token, cache_guid,
                                              birthday);
  }
}

void SyncServiceImpl::RecordMemoryUsageAndCountsHistograms() {
  ModelTypeSet active_types = GetActiveDataTypes();
  for (ModelType type : active_types) {
    auto dtc_it = data_type_controllers_.find(type);
    if (dtc_it != data_type_controllers_.end() &&
        dtc_it->second->state() != DataTypeController::NOT_RUNNING) {
      // It's possible that a data type is considered active, but its
      // DataTypeController is still NOT_RUNNING, in the case where we're
      // waiting for a custom passphrase.
      dtc_it->second->RecordMemoryUsageAndCountsHistograms();
    }
  }
}

const GURL& SyncServiceImpl::GetSyncServiceUrlForDebugging() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_service_url_;
}

std::string SyncServiceImpl::GetUnrecoverableErrorMessageForDebugging() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return unrecoverable_error_message_;
}

base::Location SyncServiceImpl::GetUnrecoverableErrorLocationForDebugging()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return unrecoverable_error_location_;
}

void SyncServiceImpl::OnSetupInProgressHandleDestroyed() {
  DCHECK_GT(outstanding_setup_in_progress_handles_, 0);

  --outstanding_setup_in_progress_handles_;

  if (engine_ && engine_->IsInitialized()) {
    // The user closed a setup UI, and will expect their changes to actually
    // take effect now. So we reconfigure here even if another setup UI happens
    // to be open right now.
    ReconfigureDatatypeManager(/*bypass_setup_in_progress_check=*/true);
  }

  NotifyObservers();
}

}  // namespace syncer
