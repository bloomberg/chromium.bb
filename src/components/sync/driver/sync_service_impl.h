// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/data_type_manager.h"
#include "components/sync/driver/data_type_manager_observer.h"
#include "components/sync/driver/data_type_status_table.h"
#include "components/sync/driver/startup_controller.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_crypto.h"
#include "components/sync/driver/sync_stopped_reporter.h"
#include "components/sync/driver/sync_user_settings_impl.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/events/protocol_event_observer.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/version_info/channel.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace syncer {

class BackendMigrator;
class SyncAuthManager;

// Look at the SyncService interface for information on how to use this class.
// You should not need to know about SyncServiceImpl directly.
class SyncServiceImpl : public SyncService,
                        public SyncEngineHost,
                        public SyncPrefObserver,
                        public DataTypeManagerObserver,
                        public SyncServiceCrypto::Delegate,
                        public signin::IdentityManager::Observer {
 public:
  // If AUTO_START, sync will set IsFirstSetupComplete() automatically and sync
  // will begin syncing without the user needing to confirm sync settings.
  enum StartBehavior {
    AUTO_START,
    MANUAL_START,
  };

  // Bundles the arguments for SyncServiceImpl construction. This is a
  // movable struct. Because of the non-POD data members, it needs out-of-line
  // constructors, so in particular the move constructor needs to be
  // explicitly defined.
  struct InitParams {
    InitParams();
    InitParams(InitParams&& other);
    ~InitParams();

    std::unique_ptr<SyncClient> sync_client;
    // TODO(treib): Remove this and instead retrieve it via
    // SyncClient::GetIdentityManager (but mind LocalSync).
    signin::IdentityManager* identity_manager = nullptr;
    StartBehavior start_behavior = MANUAL_START;
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
    network::NetworkConnectionTracker* network_connection_tracker = nullptr;
    version_info::Channel channel = version_info::Channel::UNKNOWN;
    std::string debug_identifier;
    policy::PolicyService* policy_service = nullptr;

   private:
    DISALLOW_COPY_AND_ASSIGN(InitParams);
  };

  explicit SyncServiceImpl(InitParams init_params);

  ~SyncServiceImpl() override;

  // Initializes the object. This must be called at most once, and
  // immediately after an object of this class is constructed.
  // TODO(mastiz): Rename this to Start().
  void Initialize();

  // SyncService implementation
  SyncUserSettings* GetUserSettings() override;
  const SyncUserSettings* GetUserSettings() const override;
  DisableReasonSet GetDisableReasons() const override;
  TransportState GetTransportState() const override;
  bool IsLocalSyncEnabled() const override;
  CoreAccountInfo GetAuthenticatedAccountInfo() const override;
  bool IsAuthenticatedAccountPrimary() const override;
  GoogleServiceAuthError GetAuthError() const override;
  base::Time GetAuthErrorTime() const override;
  bool RequiresClientUpgrade() const override;
  std::unique_ptr<SyncSetupInProgressHandle> GetSetupInProgressHandle()
      override;
  bool IsSetupInProgress() const override;
  ModelTypeSet GetPreferredDataTypes() const override;
  ModelTypeSet GetActiveDataTypes() const override;
  void StopAndClear() override;
  void SetSyncAllowedByPlatform(bool allowed) override;
  void OnDataTypeRequestsSyncStartup(ModelType type) override;
  void TriggerRefresh(const ModelTypeSet& types) override;
  void DataTypePreconditionChanged(ModelType type) override;
  void SetInvalidationsForSessionsEnabled(bool enabled) override;
  void AddTrustedVaultDecryptionKeysFromWeb(
      const std::string& gaia_id,
      const std::vector<std::vector<uint8_t>>& keys,
      int last_key_version) override;
  void AddTrustedVaultRecoveryMethodFromWeb(
      const std::string& gaia_id,
      const std::vector<uint8_t>& public_key,
      int method_type_hint,
      base::OnceClosure callback) override;
  void AddObserver(SyncServiceObserver* observer) override;
  void RemoveObserver(SyncServiceObserver* observer) override;
  bool HasObserver(const SyncServiceObserver* observer) const override;
  SyncTokenStatus GetSyncTokenStatusForDebugging() const override;
  bool QueryDetailedSyncStatusForDebugging(SyncStatus* result) const override;
  base::Time GetLastSyncedTimeForDebugging() const override;
  SyncCycleSnapshot GetLastCycleSnapshotForDebugging() const override;
  std::unique_ptr<base::Value> GetTypeStatusMapForDebugging() override;
  void GetEntityCountsForDebugging(
      base::OnceCallback<void(const std::vector<TypeEntitiesCount>&)> callback)
      const override;
  const GURL& GetSyncServiceUrlForDebugging() const override;
  std::string GetUnrecoverableErrorMessageForDebugging() const override;
  base::Location GetUnrecoverableErrorLocationForDebugging() const override;
  void AddProtocolEventObserver(ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(ProtocolEventObserver* observer) override;
  void GetAllNodesForDebugging(
      base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback)
      override;

  // SyncEngineHost implementation.
  void OnEngineInitialized(
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      bool success,
      bool is_first_time_sync_configure) override;
  void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;
  void OnConnectionStatusChange(ConnectionStatus status) override;
  void OnMigrationNeededForTypes(ModelTypeSet types) override;
  void OnActionableError(const SyncProtocolError& error) override;
  void OnBackedOffTypesChanged() override;

  // DataTypeManagerObserver implementation.
  void OnConfigureDone(const DataTypeManager::ConfigureResult& result) override;
  void OnConfigureStart() override;

  // SyncServiceCrypto::Delegate implementation.
  void CryptoStateChanged() override;
  void CryptoRequiredUserActionChanged() override;
  void ReconfigureDataTypesDueToCrypto() override;
  void EncryptionBootstrapTokenChanged(
      const std::string& bootstrap_token) override;

  // IdentityManager::Observer implementation.
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // Similar to above but with a callback that will be invoked on completion.
  void OnAccountsInCookieUpdatedWithCallback(
      const std::vector<gaia::ListedAccount>& signed_in_accounts,
      base::OnceClosure callback);

  // Returns true if currently signed in account is not present in the list of
  // accounts from cookie jar.
  bool HasCookieJarMismatch(
      const std::vector<gaia::ListedAccount>& cookie_jar_accounts);

  // SyncPrefObserver implementation.
  void OnSyncManagedPrefChange(bool is_sync_managed) override;
  void OnFirstSetupCompletePrefChange(bool is_first_setup_complete) override;
  void OnSyncRequestedPrefChange(bool is_sync_requested) override;
  void OnPreferredDataTypesPrefChange() override;

  // KeyedService implementation.  This must be called exactly
  // once (before this object is destroyed).
  void Shutdown() override;

#if defined(OS_ANDROID)
  // Persists the fact that sync should no longer respect whether Android master
  // sync is enabled. This will be respected for the current syncing account
  // (if one exists) and any future ones. Only called on Android.
  void SetDecoupledFromAndroidMasterSync();

  // Gets the persisted information of whether sync should no longer respect
  // if Android master sync is enabled. Only called on Android.
  bool GetDecoupledFromAndroidMasterSync();
#endif  // defined(OS_ANDROID)

  // Returns whether or not the underlying sync engine has made any
  // local changes to items that have not yet been synced with the
  // server.
  void HasUnsyncedItemsForTest(base::OnceCallback<void(bool)> cb) const;

  // Used by MigrationWatcher.  May return null.
  BackendMigrator* GetBackendMigratorForTest();

  // Used by tests to inspect interaction with the access token fetcher.
  bool IsRetryingAccessTokenFetchForTest() const;

  // Used by tests to inspect the OAuth2 access tokens used by PSS.
  std::string GetAccessTokenForTest() const;

  // Overrides the callback used to create network connections.
  // TODO(crbug.com/949504): Inject this in the ctor instead. As it is, it's
  // possible that the real callback was already used before the test had a
  // chance to call this.
  void OverrideNetworkForTest(const CreateHttpPostProviderFactory&
                                  create_http_post_provider_factory_cb);

  ModelTypeSet GetRegisteredDataTypesForTest() const;

  void GetThrottledDataTypesForTest(
      base::OnceCallback<void(ModelTypeSet)> cb) const;

  // Simulates that all policies just got loaded. This does nothing if the
  // policies were already loaded.
  void TriggerPoliciesLoadedForTest();

  bool IsDataTypeControllerRunningForTest(ModelType type) const;

  // Some tests rely on injecting calls to the encryption observer.
  SyncEncryptionHandler::Observer* GetEncryptionObserverForTest();

  SyncClient* GetSyncClientForTest();

 private:
  enum UnrecoverableErrorReason {
    ERROR_REASON_ENGINE_INIT_FAILURE,
    ERROR_REASON_ACTIONABLE_ERROR,
  };

  // Callbacks for SyncAuthManager.
  void AccountStateChanged();
  void CredentialsChanged();

  // A wrapper around SyncUserSettings::SetSyncRequested(), such that the
  // notification which is synchronously triggered will be ignored in the
  // implementation of OnSyncRequestedPrefChange().
  void SetSyncRequestedAndIgnoreNotification(bool is_requested);

  bool IsEngineAllowedToRun() const;

  // Reconfigures the data type manager with the latest enabled types.
  // Note: Does not initialize the engine if it is not already initialized.
  // If a Sync setup is currently in progress (i.e. a settings UI is open), then
  // the reconfiguration will only happen if |bypass_setup_in_progress_check| is
  // set to true.
  void ReconfigureDatatypeManager(bool bypass_setup_in_progress_check);

  // Helper to install and configure a data type manager.
  void ConfigureDataTypeManager(ConfigureReason reason);

  bool UseTransportOnlyMode() const;

  // Returns the set of data types that are supported in principle, possibly
  // influenced by command-line options.
  ModelTypeSet GetRegisteredDataTypes() const;

  // Returns the ModelTypes allowed in transport-only mode (i.e. those that are
  // not tied to sync-the-feature).
  ModelTypeSet GetModelTypesForTransportOnlyMode() const;

  // If in transport-only mode, returns only preferred data types which are
  // allowed in transport-only mode. Otherwise, returns all preferred data
  // types.
  ModelTypeSet GetDataTypesToConfigure() const;

  void UpdateDataTypesForInvalidations();

  // Shuts down the engine sync components.
  // |reason| dictates if syncing is being disabled or not.
  void ShutdownImpl(ShutdownReason reason);

  // Helper for OnUnrecoverableError.
  void OnUnrecoverableErrorImpl(const base::Location& from_here,
                                const std::string& message,
                                UnrecoverableErrorReason reason);

  // Stops the sync engine and clears the local sync data.
  // Does NOT set IsSyncRequested to false, use StopAndClear() or
  // SyncUserSettings::SetSyncRequested() for that.
  // TODO(crbug.com/1229171): Either rename this method, or merge it with
  // StopAndClear.
  void StopAndClearImpl();

  // Puts the engine's sync scheduler into NORMAL mode.
  // Called when configuration is complete.
  void StartSyncingWithServer();

  // Notify all observers that a change has occurred.
  void NotifyObservers();

  void NotifySyncCycleCompleted();
  void NotifyShutdown();

  void ClearUnrecoverableError();

  // Kicks off asynchronous initialization of the SyncEngine.
  void StartUpSlowEngineComponents();

  // Update UMA for syncing engine.
  void UpdateEngineInitUMA(bool success) const;

  // Whether sync has been authenticated with an account ID.
  bool IsSignedIn() const;

  // Tell the sync server that this client has disabled sync.
  void RemoveClientFromServer() const;

  // Records per type histograms for estimated memory usage and number of
  // entities.
  void RecordMemoryUsageAndCountsHistograms();

  // True if setup has been completed at least once and is not in progress.
  bool CanConfigureDataTypes(bool bypass_setup_in_progress_check) const;

  // Called when a SetupInProgressHandle issued by this instance is destroyed.
  void OnSetupInProgressHandleDestroyed();

  // This profile's SyncClient, which abstracts away non-Sync dependencies and
  // the Sync API component factory.
  const std::unique_ptr<SyncClient> sync_client_;

  // The class that handles getting, setting, and persisting sync preferences.
  SyncPrefs sync_prefs_;

  // Encapsulates user signin - used to set/get the user's authenticated
  // email address and sign-out upon error.
  // May be null (if local Sync is enabled).
  signin::IdentityManager* const identity_manager_;

  // The user-configurable knobs. Non-null between Initialize() and Shutdown().
  std::unique_ptr<SyncUserSettingsImpl> user_settings_;

  // Handles tracking of the authenticated account and acquiring access tokens.
  // Only null after Shutdown().
  std::unique_ptr<SyncAuthManager> auth_manager_;

  const version_info::Channel channel_;

  // An identifier representing this instance for debugging purposes.
  const std::string debug_identifier_;

  // This specifies where to find the sync server.
  const GURL sync_service_url_;

  // A utility object containing logic and state relating to encryption.
  SyncServiceCrypto crypto_;

  // Our asynchronous engine to communicate with sync components living on
  // other threads.
  std::unique_ptr<SyncEngine> engine_;

  // Used to ensure that certain operations are performed on the sequence that
  // this object was created on.
  SEQUENCE_CHECKER(sequence_checker_);

  // Cache of the last SyncCycleSnapshot received from the sync engine.
  SyncCycleSnapshot last_snapshot_;

  // The URL loader factory for the sync.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The global NetworkConnectionTracker instance.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // Indicates if this is the first time sync is being configured.
  // This is set to true if last synced time is not set at the time of
  // OnEngineInitialized().
  bool is_first_time_sync_configure_;

  // Number of UIs currently configuring the Sync service. When this number
  // is decremented back to zero, Sync setup is marked no longer in progress.
  int outstanding_setup_in_progress_handles_ = 0;

  // Set when sync receives STOP_SYNC_FOR_DISABLED_ACCOUNT error from server.
  // Prevents SyncServiceImpl from starting engine till browser restarted
  // or user signed out.
  bool sync_disabled_by_admin_;

  // Whether Sync is allowed at the platform level (e.g. Android's "MasterSync"
  // toggle). Maps to DISABLE_REASON_PLATFORM_OVERRIDE.
  bool sync_allowed_by_platform_ = true;

  // Information describing an unrecoverable error.
  absl::optional<UnrecoverableErrorReason> unrecoverable_error_reason_ =
      absl::nullopt;
  std::string unrecoverable_error_message_;
  base::Location unrecoverable_error_location_;

  // Manages the start and stop of the data types.
  std::unique_ptr<DataTypeManager> data_type_manager_;

  // Note: This is an Optional so that we can control its destruction - in
  // particular, to trigger the "check_empty" test in Shutdown().
  absl::optional<base::ObserverList<SyncServiceObserver,
                                    /*check_empty=*/true>::Unchecked>
      observers_;

  base::ObserverList<ProtocolEventObserver>::Unchecked
      protocol_event_observers_;

  // This allows us to gracefully handle an ABORTED return code from the
  // DataTypeManager in the event that the server informed us to cease and
  // desist syncing immediately.
  bool expect_sync_configuration_aborted_;

  std::unique_ptr<BackendMigrator> migrator_;

  // This is the last |SyncProtocolError| we received from the server that had
  // an action set on it.
  SyncProtocolError last_actionable_error_;

  // Tracks the set of failed data types (those that encounter an error
  // or must delay loading for some reason).
  DataTypeStatusTable::TypeErrorMap data_type_error_map_;

  // List of available data type controllers.
  DataTypeController::TypeMap data_type_controllers_;

  CreateHttpPostProviderFactory create_http_post_provider_factory_cb_;

  const StartBehavior start_behavior_;
  std::unique_ptr<StartupController> startup_controller_;

  std::unique_ptr<SyncStoppedReporter> sync_stopped_reporter_;

  // Used in OnSyncRequestedPrefChange() to know whether the notification was
  // caused by the service itself setting the pref.
  bool is_setting_sync_requested_;

  // Used for UMA to determine whether TrustedVaultErrorShownOnStartup
  // histogram needs to recorded. Set to false iff histogram was already
  // recorded or trusted vault passphrase type wasn't used on startup.
  bool should_record_trusted_vault_error_shown_on_startup_;

  // Whether we want to receive invalidations for the SESSIONS data type. This
  // is typically false on Android (to save network traffic), but true on all
  // other platforms.
  bool sessions_invalidations_enabled_;

  // This weak factory invalidates its issued pointers when Sync is disabled.
  base::WeakPtrFactory<SyncServiceImpl> sync_enabled_weak_factory_{this};

  base::WeakPtrFactory<SyncServiceImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncServiceImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_IMPL_H_
