// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"

#include <cstddef>
#include <sstream>
#include <utility>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/quiesce_status_change_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_signin_delegate.h"
#include "chrome/common/channel_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/driver/about_sync_util.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/engine_impl/net/url_translator.h"
#include "components/sync/engine_impl/traffic_logger.h"
#include "components/sync/protocol/sync.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/zlib/google/compression_utils.h"

using syncer::ProfileSyncService;
using syncer::SyncCycleSnapshot;

const char* kSyncUrlClearServerDataKey = "sync-url-clear-server-data";

namespace {

bool HasAuthError(ProfileSyncService* service) {
  return service->GetAuthError().state() ==
             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
         service->GetAuthError().state() ==
             GoogleServiceAuthError::SERVICE_ERROR ||
         service->GetAuthError().state() ==
             GoogleServiceAuthError::REQUEST_CANCELED;
}

class EngineInitializeChecker : public SingleClientStatusChangeChecker {
 public:
  explicit EngineInitializeChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync engine initialization to complete";
    if (service()->IsEngineInitialized())
      return true;
    // Engine initialization is blocked by an auth error.
    if (HasAuthError(service())) {
      LOG(WARNING) << "Sync engine initialization blocked by auth error";
      return true;
    }
    // Engine initialization is blocked by a failure to fetch Oauth2 tokens.
    if (service()->IsRetryingAccessTokenFetchForTest()) {
      LOG(WARNING) << "Sync engine initialization blocked by failure to fetch "
                      "access tokens";
      return true;
    }
    // Still waiting on engine initialization.
    return false;
  }
};

class SyncSetupChecker : public SingleClientStatusChangeChecker {
 public:
  enum class State { kTransportActive, kFeatureActive };

  SyncSetupChecker(ProfileSyncService* service, State wait_for_state)
      : SingleClientStatusChangeChecker(service),
        wait_for_state_(wait_for_state) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync setup to complete";

    syncer::SyncService::TransportState transport_state =
        service()->GetTransportState();
    if (transport_state == syncer::SyncService::TransportState::ACTIVE &&
        (wait_for_state_ != State::kFeatureActive ||
         service()->IsSyncFeatureActive())) {
      return true;
    }
    // Sync is blocked by an auth error.
    if (HasAuthError(service())) {
      return true;
    }
    // TODO(crbug.com/1010397): The verification of INITIALIZING is only needed
    // due to SyncEncryptionHandlerImpl issuing an unnecessary
    // OnPassphraseRequired() during initialization.
    if (service()
            ->GetUserSettings()
            ->IsPassphraseRequiredForPreferredDataTypes() &&
        transport_state != syncer::SyncService::TransportState::INITIALIZING) {
      LOG(FATAL)
          << "A passphrase is required for decryption but was not provided. "
             "Waiting for sync to become available won't succeed. Make sure "
             "to pass it when setting up sync.";
    }
    // Still waiting on sync setup.
    return false;
  }

 private:
  const State wait_for_state_;
};

}  // namespace

// static
std::unique_ptr<ProfileSyncServiceHarness> ProfileSyncServiceHarness::Create(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    SigninType signin_type) {
  return base::WrapUnique(
      new ProfileSyncServiceHarness(profile, username, password, signin_type));
}

ProfileSyncServiceHarness::ProfileSyncServiceHarness(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    SigninType signin_type)
    : profile_(profile),
      service_(ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(
          profile)),
      username_(username),
      password_(password),
      signin_type_(signin_type),
      profile_debug_name_(profile->GetDebugName()),
      signin_delegate_(CreateSyncSigninDelegate()) {}

ProfileSyncServiceHarness::~ProfileSyncServiceHarness() = default;

bool ProfileSyncServiceHarness::SignInPrimaryAccount() {
  // TODO(crbug.com/871221): This function should distinguish primary account
  // (aka sync account) from secondary accounts (content area signin). Let's
  // migrate tests that exercise transport-only sync to secondary accounts.
  DCHECK(!username_.empty());

  switch (signin_type_) {
    case SigninType::UI_SIGNIN: {
      return signin_delegate_->SigninUI(profile_, username_, password_);
    }

    case SigninType::FAKE_SIGNIN: {
      signin_delegate_->SigninFake(profile_, username_);
      return true;
    }
  }

  NOTREACHED();
  return false;
}

// Same as reset on chrome.google.com/sync.
// This function will wait until the reset is done. If error occurs,
// it will log error messages.
void ResetAccount(network::SharedURLLoaderFactory* url_loader_factory,
                  const std::string& access_token,
                  const GURL& url,
                  const std::string& username,
                  const std::string& birthday) {
  // Generate https POST payload.
  sync_pb::ClientToServerMessage message;
  message.set_share(username);
  message.set_message_contents(
      sync_pb::ClientToServerMessage::CLEAR_SERVER_DATA);
  message.set_store_birthday(birthday);
  message.set_api_key(google_apis::GetAPIKey());
  syncer::LogClientToServerMessage(message);
  std::string payload;
  message.SerializeToString(&payload);
  std::string request_to_send;
  compression::GzipCompress(payload, &request_to_send);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Authorization",
                                      "Bearer " + access_token);
  resource_request->headers.SetHeader("Content-Encoding", "gzip");
  resource_request->headers.SetHeader("Accept-Language", "en-US,en");
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->AttachStringForUpload(request_to_send,
                                       "application/octet-stream");
  simple_loader->SetTimeoutDuration(base::TimeDelta::FromSeconds(10));
  content::SimpleURLLoaderTestHelper url_loader_helper;
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory, url_loader_helper.GetCallback());
  url_loader_helper.WaitForCallback();
  if (simple_loader->NetError() != 0) {
    LOG(ERROR) << "Reset account failed with error "
               << net::ErrorToString(simple_loader->NetError())
               << ". The account will remain dirty and may cause test fail.";
  }
}

void ProfileSyncServiceHarness::ResetSyncForPrimaryAccount() {
  syncer::SyncPrefs sync_prefs(profile_->GetPrefs());
  // Generate the https url.
  // CLEAR_SERVER_DATA isn't enabled on the prod Sync server,
  // so --sync-url-clear-server-data can be used to specify an
  // alternative endpoint.
  // Note: Any OTA(Owned Test Account) tries to clear data need to be
  // whitelisted.
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line->HasSwitch(kSyncUrlClearServerDataKey))
      << "Missing switch " << kSyncUrlClearServerDataKey;
  std::string url =
      cmd_line->GetSwitchValueASCII(kSyncUrlClearServerDataKey) + "/command/?";
  url += syncer::MakeSyncQueryString(sync_prefs.GetCacheGuid());

  // Call sync server to clear sync data.
  std::string access_token = service()->GetAccessTokenForTest();
  DCHECK(access_token.size()) << "Access token is not available.";
  ResetAccount(profile_->GetURLLoaderFactory().get(), access_token, GURL(url),
               username_, sync_prefs.GetBirthday());
}

#if !defined(OS_CHROMEOS)
void ProfileSyncServiceHarness::SignOutPrimaryAccount() {
  DCHECK(!username_.empty());
  signin::ClearPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_),
      signin::ClearPrimaryAccountPolicy::REMOVE_ALL_ACCOUNTS);
}
#endif  // !OS_CHROMEOS

void ProfileSyncServiceHarness::EnterSyncPausedStateForPrimaryAccount() {
  DCHECK(service_->IsSyncFeatureActive());
  signin::SetInvalidRefreshTokenForPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_));
}

void ProfileSyncServiceHarness::ExitSyncPausedStateForPrimaryAccount() {
  signin::SetRefreshTokenForPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_));
}

bool ProfileSyncServiceHarness::SetupSync() {
  bool result =
      SetupSyncNoWaitForCompletion(
          service()->GetUserSettings()->GetRegisteredSelectableTypes()) &&
      AwaitSyncSetupCompletion();
  if (!result) {
    LOG(ERROR) << profile_debug_name_ << ": SetupSync failed. Syncer status:\n"
               << GetServiceStatus();
  } else {
    DVLOG(1) << profile_debug_name_ << ": SetupSync successful.";
  }
  return result;
}

bool ProfileSyncServiceHarness::SetupSyncNoWaitForCompletion(
    syncer::UserSelectableTypeSet selected_types) {
  return SetupSyncImpl(selected_types, EncryptionSetupMode::kNoEncryption,
                       /*encryption_passphrase=*/base::nullopt);
}

bool ProfileSyncServiceHarness::
    SetupSyncWithEncryptionPassphraseNoWaitForCompletion(
        syncer::UserSelectableTypeSet selected_types,
        const std::string& passphrase) {
  return SetupSyncImpl(selected_types, EncryptionSetupMode::kEncryption,
                       passphrase);
}

bool ProfileSyncServiceHarness::
    SetupSyncWithDecryptionPassphraseNoWaitForCompletion(
        syncer::UserSelectableTypeSet selected_types,
        const std::string& passphrase) {
  return SetupSyncImpl(selected_types, EncryptionSetupMode::kDecryption,
                       passphrase);
}

bool ProfileSyncServiceHarness::SetupSyncImpl(
    syncer::UserSelectableTypeSet selected_types,
    EncryptionSetupMode encryption_mode,
    const base::Optional<std::string>& passphrase) {
  DCHECK(encryption_mode == EncryptionSetupMode::kNoEncryption ||
         passphrase.has_value());
  DCHECK(!profile_->IsLegacySupervised())
      << "SetupSync should not be used for legacy supervised users.";

  if (service() == nullptr) {
    LOG(ERROR) << "SetupSync(): service() is null.";
    return false;
  }

  // Tell the sync service that setup is in progress so we don't start syncing
  // until we've finished configuration.
  sync_blocker_ = service()->GetSetupInProgressHandle();

  if (!SignInPrimaryAccount()) {
    return false;
  }

  // Now that auth is completed, request that sync actually start.
  service()->GetUserSettings()->SetSyncRequested(true);

  if (!AwaitEngineInitialization()) {
    return false;
  }
  // Choose the datatypes to be synced. If all registered datatypes are to be
  // synced, set sync_everything to true; otherwise, set it to false.
  bool sync_everything =
      (selected_types ==
       service()->GetUserSettings()->GetRegisteredSelectableTypes());
  service()->GetUserSettings()->SetSelectedTypes(sync_everything,
                                                 selected_types);

  if (encryption_mode == EncryptionSetupMode::kEncryption) {
    service()->GetUserSettings()->SetEncryptionPassphrase(passphrase.value());
  } else if (encryption_mode == EncryptionSetupMode::kDecryption) {
    if (!service()->GetUserSettings()->SetDecryptionPassphrase(
            passphrase.value())) {
      LOG(ERROR) << "WARNING: provided passphrase could not decrypt locally "
                    "present data.";
    }
  }
  // Notify ProfileSyncService that we are done with configuration.
  FinishSyncSetup();

  if (signin_type_ == SigninType::UI_SIGNIN)
    return signin_delegate_->ConfirmSigninUI(profile_);
  return true;
}

void ProfileSyncServiceHarness::FinishSyncSetup() {
  sync_blocker_.reset();
  service()->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
}

void ProfileSyncServiceHarness::StopSyncServiceAndClearData() {
  DVLOG(1) << "Requesting stop for service and clearing data.";
  service()->StopAndClear();
}

void ProfileSyncServiceHarness::StopSyncServiceWithoutClearingData() {
  DVLOG(1) << "Requesting stop for service without clearing data.";
  service()->GetUserSettings()->SetSyncRequested(false);
}

bool ProfileSyncServiceHarness::StartSyncService() {
  std::unique_ptr<syncer::SyncSetupInProgressHandle> blocker =
      service()->GetSetupInProgressHandle();
  DVLOG(1) << "Requesting start for service";
  service()->GetUserSettings()->SetSyncRequested(true);

  if (!AwaitEngineInitialization()) {
    LOG(ERROR) << "AwaitEngineInitialization failed.";
    return false;
  }
  DVLOG(1) << "Engine Initialized successfully.";

  if (service()->GetUserSettings()->IsUsingSecondaryPassphrase()) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetDecryptionPassphrase is called.";
    return false;
  }
  DVLOG(1) << "Passphrase decryption success.";

  blocker.reset();
  service()->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  if (!AwaitSyncSetupCompletion()) {
    LOG(FATAL) << "AwaitSyncSetupCompletion failed.";
    return false;
  }

  return true;
}

bool ProfileSyncServiceHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceHarness* partner) {
  std::vector<ProfileSyncServiceHarness*> harnesses;
  harnesses.push_back(this);
  harnesses.push_back(partner);
  return AwaitQuiescence(harnesses);
}

// static
bool ProfileSyncServiceHarness::AwaitQuiescence(
    const std::vector<ProfileSyncServiceHarness*>& clients) {
  if (clients.empty()) {
    return true;
  }

  std::vector<ProfileSyncService*> services;
  for (const ProfileSyncServiceHarness* harness : clients) {
    services.push_back(harness->service());
  }
  return QuiesceStatusChangeChecker(services).Wait();
}

bool ProfileSyncServiceHarness::AwaitEngineInitialization() {
  if (!EngineInitializeChecker(service()).Wait()) {
    LOG(ERROR) << "EngineInitializeChecker timed out.";
    return false;
  }

  if (HasAuthError(service())) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  if (service()->IsRetryingAccessTokenFetchForTest()) {
    LOG(ERROR) << "Failed to fetch access token. Sync cannot proceed.";
    return false;
  }

  if (!service()->IsEngineInitialized()) {
    LOG(ERROR) << "Service engine not initialized.";
    return false;
  }

  return true;
}

bool ProfileSyncServiceHarness::AwaitSyncSetupCompletion() {
  CHECK(service()->GetUserSettings()->IsFirstSetupComplete())
      << "Waiting for setup completion can only succeed after the first setup "
      << "got marked complete. Did you call SetupSync on this client?";
  if (!SyncSetupChecker(service(), SyncSetupChecker::State::kFeatureActive)
           .Wait()) {
    LOG(ERROR) << "SyncSetupChecker timed out.";
    return false;
  }
  // Signal an error if the initial sync wasn't successful.
  if (HasAuthError(service())) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  return true;
}

bool ProfileSyncServiceHarness::AwaitSyncTransportActive() {
  if (!SyncSetupChecker(service(), SyncSetupChecker::State::kTransportActive)
           .Wait()) {
    LOG(ERROR) << "SyncSetupChecker timed out.";
    return false;
  }
  // Signal an error if the initial sync wasn't successful.
  if (HasAuthError(service())) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  return true;
}

bool ProfileSyncServiceHarness::EnableSyncForType(
    syncer::UserSelectableType type) {
  DVLOG(1) << GetClientInfoString(
      "EnableSyncForType(" +
      std::string(syncer::GetUserSelectableTypeName(type)) + ")");

  if (!IsSyncEnabledByUser()) {
    bool result =
        SetupSyncNoWaitForCompletion({type}) && AwaitSyncSetupCompletion();
    // If SetupSync() succeeded, then Sync must now be enabled.
    DCHECK(!result || IsSyncEnabledByUser());
    return result;
  }

  if (service() == nullptr) {
    LOG(ERROR) << "EnableSyncForType(): service() is null.";
    return false;
  }

  syncer::UserSelectableTypeSet selected_types =
      service()->GetUserSettings()->GetSelectedTypes();
  if (selected_types.Has(type)) {
    DVLOG(1) << "EnableSyncForType(): Sync already enabled for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  selected_types.Put(type);
  service()->GetUserSettings()->SetSelectedTypes(false, selected_types);
  if (AwaitSyncSetupCompletion()) {
    DVLOG(1) << "EnableSyncForType(): Enabled sync for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("EnableSyncForType failed");
  return false;
}

bool ProfileSyncServiceHarness::DisableSyncForType(
    syncer::UserSelectableType type) {
  DVLOG(1) << GetClientInfoString(
      "DisableSyncForType(" +
      std::string(syncer::GetUserSelectableTypeName(type)) + ")");

  if (service() == nullptr) {
    LOG(ERROR) << "DisableSyncForType(): service() is null.";
    return false;
  }

  syncer::UserSelectableTypeSet selected_types =
      service()->GetUserSettings()->GetSelectedTypes();
  if (!selected_types.Has(type)) {
    DVLOG(1) << "DisableSyncForType(): Sync already disabled for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  selected_types.Remove(type);
  service()->GetUserSettings()->SetSelectedTypes(false, selected_types);
  if (AwaitSyncSetupCompletion()) {
    DVLOG(1) << "DisableSyncForType(): Disabled sync for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("DisableSyncForDatatype failed");
  return false;
}

bool ProfileSyncServiceHarness::EnableSyncForRegisteredDatatypes() {
  DVLOG(1) << GetClientInfoString("EnableSyncForRegisteredDatatypes");

  if (!IsSyncEnabledByUser()) {
    bool result = SetupSync();
    // If SetupSync() succeeded, then Sync must now be enabled.
    DCHECK(!result || IsSyncEnabledByUser());
    return result;
  }

  if (service() == nullptr) {
    LOG(ERROR) << "EnableSyncForRegisteredDatatypes(): service() is null.";
    return false;
  }

  service()->GetUserSettings()->SetSelectedTypes(
      true, service()->GetUserSettings()->GetRegisteredSelectableTypes());

  if (AwaitSyncSetupCompletion()) {
    DVLOG(1)
        << "EnableSyncForRegisteredDatatypes(): Enabled sync for all datatypes "
        << "on " << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("EnableSyncForRegisteredDatatypes failed");
  return false;
}

bool ProfileSyncServiceHarness::DisableSyncForAllDatatypes() {
  DVLOG(1) << GetClientInfoString("DisableSyncForAllDatatypes");

  if (service() == nullptr) {
    LOG(ERROR) << "DisableSyncForAllDatatypes(): service() is null.";
    return false;
  }

  service()->StopAndClear();

  DVLOG(1) << "DisableSyncForAllDatatypes(): Disabled sync for all "
           << "datatypes on " << profile_debug_name_;
  return true;
}

SyncCycleSnapshot ProfileSyncServiceHarness::GetLastCycleSnapshot() const {
  DCHECK(service() != nullptr) << "Sync service has not yet been set up.";
  if (service()->IsSyncFeatureActive()) {
    return service()->GetLastCycleSnapshotForDebugging();
  }
  return SyncCycleSnapshot();
}

std::string ProfileSyncServiceHarness::GetServiceStatus() {
  std::unique_ptr<base::DictionaryValue> value(
      syncer::sync_ui_util::ConstructAboutInformation(service(),
                                                      chrome::GetChannel()));
  std::string service_status;
  base::JSONWriter::WriteWithOptions(
      *value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &service_status);
  return service_status;
}

// TODO(sync): Clean up this method in a separate CL. Remove all snapshot fields
// and log shorter, more meaningful messages.
std::string ProfileSyncServiceHarness::GetClientInfoString(
    const std::string& message) const {
  std::stringstream os;
  os << profile_debug_name_ << ": " << message << ": ";
  if (service()) {
    const SyncCycleSnapshot& snap = GetLastCycleSnapshot();
    syncer::SyncStatus status;
    service()->QueryDetailedSyncStatusForDebugging(&status);
    // Capture select info from the sync session snapshot and syncer status.
    os << ", has_unsynced_items: " << snap.has_remaining_local_changes()
       << ", did_commit: "
       << (snap.model_neutral_state().num_successful_commits == 0 &&
           snap.model_neutral_state().commit_result.value() ==
               syncer::SyncerError::SYNCER_OK)
       << ", encryption conflicts: " << snap.num_encryption_conflicts()
       << ", hierarchy conflicts: " << snap.num_hierarchy_conflicts()
       << ", server conflicts: " << snap.num_server_conflicts()
       << ", num_updates_downloaded : "
       << snap.model_neutral_state().num_updates_downloaded_total
       << ", passphrase_required: "
       << service()->GetUserSettings()->IsPassphraseRequired()
       << ", notifications_enabled: " << status.notifications_enabled
       << ", service_is_active: " << service()->IsSyncFeatureActive();
  } else {
    os << "Sync service not available";
  }
  return os.str();
}

bool ProfileSyncServiceHarness::IsSyncEnabledByUser() const {
  return service()->GetUserSettings()->IsFirstSetupComplete() &&
         !service()->HasDisableReason(
             ProfileSyncService::DISABLE_REASON_USER_CHOICE);
}
