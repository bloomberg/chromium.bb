// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/service.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/components/audio/cras_audio_handler.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_alarm_timer_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_notification_controller.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/services/assistant/assistant_interaction_logger.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/assistant_manager_service_impl.h"
#include "chromeos/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/device_actions.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/service_context.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/user_manager/known_user.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace assistant {

namespace {

constexpr char kScopeAssistant[] =
    "https://www.googleapis.com/auth/assistant-sdk-prototype";

constexpr base::TimeDelta kMinTokenRefreshDelay = base::Milliseconds(1000);
constexpr base::TimeDelta kMaxTokenRefreshDelay = base::Milliseconds(60 * 1000);

// Testing override for the URI used to contact the s3 server.
const char* g_s3_server_uri_override = nullptr;
// Testing override for the device-id used by Libassistant to identify this
// device.
const char* g_device_id_override = nullptr;

AssistantStatus ToAssistantStatus(AssistantManagerService::State state) {
  using State = AssistantManagerService::State;

  switch (state) {
    case State::STOPPED:
    case State::STARTING:
    case State::STARTED:
      return AssistantStatus::NOT_READY;
    case State::RUNNING:
      return AssistantStatus::READY;
  }
}

absl::optional<std::string> GetS3ServerUriOverride() {
  if (g_s3_server_uri_override)
    return g_s3_server_uri_override;
  return absl::nullopt;
}

absl::optional<std::string> GetDeviceIdOverride() {
  if (g_device_id_override)
    return g_device_id_override;
  return absl::nullopt;
}

// In the signed-out mode, we are going to run Assistant service without
// using user's signed in account information.
bool IsSignedOutMode() {
  // One example of using fake gaia login is in our automation tests, i.e.
  // Assistant Tast tests.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableGaiaServices);
}

}  // namespace

// Scoped observer that will subscribe |Service| as an Ash session observer,
// and will unsubscribe in its destructor.
class ScopedAshSessionObserver {
 public:
  ScopedAshSessionObserver(ash::SessionActivationObserver* observer,
                           const AccountId& account_id)
      : observer_(observer), account_id_(account_id) {
    DCHECK(account_id_.is_valid());
    DCHECK(controller());
    controller()->AddSessionActivationObserverForAccountId(account_id_,
                                                           observer_);
  }

  ~ScopedAshSessionObserver() {
    if (controller())
      controller()->RemoveSessionActivationObserverForAccountId(account_id_,
                                                                observer_);
  }

 private:
  ash::SessionController* controller() const {
    return ash::SessionController::Get();
  }

  ash::SessionActivationObserver* const observer_;
  const AccountId account_id_;
};

class Service::Context : public ServiceContext {
 public:
  explicit Context(Service* parent) : parent_(parent) {}

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  ~Context() override = default;

  // ServiceContext:
  ash::AssistantAlarmTimerController* assistant_alarm_timer_controller()
      override {
    return ash::AssistantAlarmTimerController::Get();
  }

  ash::AssistantController* assistant_controller() override {
    return ash::AssistantController::Get();
  }

  ash::AssistantNotificationController* assistant_notification_controller()
      override {
    return ash::AssistantNotificationController::Get();
  }

  ash::AssistantScreenContextController* assistant_screen_context_controller()
      override {
    return ash::AssistantScreenContextController::Get();
  }

  ash::AssistantStateBase* assistant_state() override {
    return ash::AssistantState::Get();
  }

  CrasAudioHandler* cras_audio_handler() override {
    return CrasAudioHandler::Get();
  }

  DeviceActions* device_actions() override { return DeviceActions::Get(); }

  scoped_refptr<base::SequencedTaskRunner> main_task_runner() override {
    return parent_->main_task_runner_;
  }

  PowerManagerClient* power_manager_client() override {
    return PowerManagerClient::Get();
  }

  std::string primary_account_gaia_id() override {
    return parent_->RetrievePrimaryAccountInfo().gaia;
  }

 private:
  Service* const parent_;  // |this| is owned by |parent_|.
};

Service::Service(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                     pending_url_loader_factory,
                 signin::IdentityManager* identity_manager)
    : context_(std::make_unique<Context>(this)),
      identity_manager_(identity_manager),
      token_refresh_timer_(std::make_unique<base::OneShotTimer>()),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      pending_url_loader_factory_(std::move(pending_url_loader_factory)) {
  DCHECK(identity_manager_);
  chromeos::PowerManagerClient* power_manager_client =
      context_->power_manager_client();
  power_manager_observation_.Observe(power_manager_client);
  power_manager_client->RequestStatusUpdate();
}

Service::~Service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ash::AssistantState::Get()->RemoveObserver(this);
  ash::AssistantController::Get()->SetAssistant(nullptr);
}

// static
void Service::OverrideS3ServerUriForTesting(const char* uri) {
  g_s3_server_uri_override = uri;
}

// static
void Service::OverrideDeviceIdForTesting(const char* device_id) {
  g_device_id_override = device_id;
}

void Service::SetAssistantManagerServiceForTesting(
    std::unique_ptr<AssistantManagerService> assistant_manager_service) {
  DCHECK(assistant_manager_service_ == nullptr);
  assistant_manager_service_for_testing_ = std::move(assistant_manager_service);
}

void Service::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ash::AssistantState::Get()->AddObserver(this);

  DCHECK(!assistant_manager_service_);

  RequestAccessToken();
}

void Service::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (assistant_manager_service_)
    StopAssistantManagerService();
}

Assistant* Service::GetAssistant() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(assistant_manager_service_);
  return assistant_manager_service_.get();
}

void Service::PowerChanged(const power_manager::PowerSupplyProperties& prop) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool power_source_connected =
      prop.external_power() == power_manager::PowerSupplyProperties::AC;
  if (power_source_connected == power_source_connected_)
    return;

  power_source_connected_ = power_source_connected;
  UpdateAssistantManagerState();
}

void Service::SuspendDone(base::TimeDelta sleep_duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |token_refresh_timer_| may become stale during sleeping, so we immediately
  // request a new token to make sure it is fresh.
  if (token_refresh_timer_->IsRunning()) {
    token_refresh_timer_->AbandonAndStop();
    RequestAccessToken();
  }
}

void Service::OnSessionActivated(bool activated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_active_ = activated;

  AssistantBrowserDelegate::Get()->OnAssistantStatusChanged(
      ToAssistantStatus(assistant_manager_service_->GetState()));
  UpdateListeningState();
}

void Service::OnLockStateChanged(bool locked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  locked_ = locked;
  UpdateListeningState();
}

void Service::OnAssistantConsentStatusChanged(int consent_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Notify device apps status when user accepts activity control.
  if (assistant_manager_service_ &&
      assistant_manager_service_->GetState() ==
          AssistantManagerService::State::RUNNING) {
    assistant_manager_service_->SyncDeviceAppsStatus();
  }
}

void Service::OnAssistantContextEnabled(bool enabled) {
  UpdateAssistantManagerState();
}

void Service::OnAssistantHotwordAlwaysOn(bool hotword_always_on) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // No need to update hotword status if power source is connected.
  if (power_source_connected_)
    return;

  UpdateAssistantManagerState();
}

void Service::OnAssistantSettingsEnabled(bool enabled) {
  UpdateAssistantManagerState();
}

void Service::OnAssistantHotwordEnabled(bool enabled) {
  UpdateAssistantManagerState();
}

void Service::OnLocaleChanged(const std::string& locale) {
  UpdateAssistantManagerState();
}

void Service::OnArcPlayStoreEnabledChanged(bool enabled) {
  UpdateAssistantManagerState();
}

void Service::OnLockedFullScreenStateChanged(bool enabled) {
  UpdateListeningState();
}

void Service::OnAuthenticationError() {
  RequestAccessToken();
}

void Service::OnStateChanged(AssistantManagerService::State new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (new_state == AssistantManagerService::State::STARTED)
    FinalizeAssistantManagerService();
  if (new_state == AssistantManagerService::State::RUNNING)
    DVLOG(1) << "Assistant is running";

  AssistantBrowserDelegate::Get()->OnAssistantStatusChanged(
      ToAssistantStatus(new_state));
  UpdateListeningState();
}

void Service::UpdateAssistantManagerState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* assistant_state = ash::AssistantState::Get();

  if (!assistant_state->hotword_enabled().has_value() ||
      !assistant_state->settings_enabled().has_value() ||
      !assistant_state->locale().has_value() ||
      (!access_token_.has_value() && !IsSignedOutMode()) ||
      !assistant_state->arc_play_store_enabled().has_value()) {
    // Assistant state has not finished initialization, let's wait.
    return;
  }

  if (IsSignedOutMode()) {
    // Clear |access_token_| in signed-out mode to keep it synced with what we
    // will pass to the |assistant_manager_service_|.
    access_token_ = absl::nullopt;
  }

  if (!assistant_manager_service_)
    CreateAssistantManagerService();

  auto state = assistant_manager_service_->GetState();
  switch (state) {
    case AssistantManagerService::State::STOPPED:
      if (assistant_state->settings_enabled().value()) {
        assistant_manager_service_->Start(GetUserInfo(), ShouldEnableHotword());
        DVLOG(1) << "Request Assistant start";
      }
      break;
    case AssistantManagerService::State::STARTING:
    case AssistantManagerService::State::STARTED:
      // If the Assistant is disabled by domain policy, the libassistant will
      // never becomes ready. Stop waiting for the state change and stop the
      // service.
      if (assistant_state->allowed_state() ==
          AssistantAllowedState::DISALLOWED_BY_POLICY) {
        StopAssistantManagerService();
        return;
      }
      // Wait if |assistant_manager_service_| is not at a stable state.
      update_assistant_manager_callback_.Cancel();
      update_assistant_manager_callback_.Reset(
          base::BindOnce(&Service::UpdateAssistantManagerState,
                         weak_ptr_factory_.GetWeakPtr()));
      main_task_runner_->PostDelayedTask(
          FROM_HERE, update_assistant_manager_callback_.callback(),
          kUpdateAssistantManagerDelay);
      break;
    case AssistantManagerService::State::RUNNING:
      if (assistant_state->settings_enabled().value()) {
        assistant_manager_service_->SetUser(GetUserInfo());
        assistant_manager_service_->EnableHotword(ShouldEnableHotword());
        assistant_manager_service_->SetArcPlayStoreEnabled(
            assistant_state->arc_play_store_enabled().value());
        assistant_manager_service_->SetAssistantContextEnabled(
            assistant_state->IsScreenContextAllowed());
      } else {
        StopAssistantManagerService();
      }
      break;
  }
}

CoreAccountInfo Service::RetrievePrimaryAccountInfo() const {
  CoreAccountInfo account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  CHECK(!account_info.account_id.empty());
  CHECK(!account_info.gaia.empty());
  return account_info;
}

void Service::RequestAccessToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Bypass access token fetching when service is running in signed-out mode.
  if (IsSignedOutMode()) {
    VLOG(1) << "Signed out mode detected, bypass access token fetching.";
    return;
  }

  if (access_token_fetcher_) {
    LOG(WARNING) << "Access token already requested.";
    return;
  }

  VLOG(1) << "Start requesting access token.";
  CoreAccountInfo account_info = RetrievePrimaryAccountInfo();
  if (!identity_manager_->HasAccountWithRefreshToken(account_info.account_id)) {
    LOG(ERROR) << "Failed to retrieve primary account info. Retrying.";
    RetryRefreshToken();
    return;
  }

  signin::ScopeSet scopes;
  scopes.insert(kScopeAssistant);
  scopes.insert(GaiaConstants::kGCMGroupServerOAuth2Scope);

  access_token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account_info.account_id, "cros_assistant", scopes,
      base::BindOnce(&Service::GetAccessTokenCallback, base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void Service::GetAccessTokenCallback(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // It's safe to delete AccessTokenFetcher from inside its own callback.
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Failed to retrieve token, error: " << error.ToString();
    RetryRefreshToken();
    return;
  }

  access_token_ = access_token_info.token;
  UpdateAssistantManagerState();
  token_refresh_timer_->Start(
      FROM_HERE, access_token_info.expiration_time - base::Time::Now(), this,
      &Service::RequestAccessToken);
}

void Service::RetryRefreshToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeDelta backoff_delay =
      std::min(kMinTokenRefreshDelay *
                   (1 << (token_refresh_error_backoff_factor - 1)),
               kMaxTokenRefreshDelay) +
      base::RandDouble() * kMinTokenRefreshDelay;
  if (backoff_delay < kMaxTokenRefreshDelay)
    ++token_refresh_error_backoff_factor;
  token_refresh_timer_->Start(FROM_HERE, backoff_delay, this,
                              &Service::RequestAccessToken);
}

void Service::CreateAssistantManagerService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  assistant_manager_service_ = CreateAndReturnAssistantManagerService();
  assistant_manager_service_->AddAuthenticationStateObserver(this);
  assistant_manager_service_->AddAndFireStateObserver(this);

  if (AssistantInteractionLogger::IsLoggingEnabled()) {
    interaction_logger_ = std::make_unique<AssistantInteractionLogger>();
    assistant_manager_service_->AddAssistantInteractionSubscriber(
        interaction_logger_.get());
  }
}

std::unique_ptr<AssistantManagerService>
Service::CreateAndReturnAssistantManagerService() {
  if (assistant_manager_service_for_testing_)
    return std::move(assistant_manager_service_for_testing_);

  // |assistant_manager_service_| is only created once.
  DCHECK(pending_url_loader_factory_);
  return std::make_unique<AssistantManagerServiceImpl>(
      context(), std::move(pending_url_loader_factory_),
      GetS3ServerUriOverride(), GetDeviceIdOverride());
}

void Service::FinalizeAssistantManagerService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(assistant_manager_service_->GetState() ==
             AssistantManagerService::STARTED ||
         assistant_manager_service_->GetState() ==
             AssistantManagerService::RUNNING);

  // Ensure one-time mojom initialization.
  if (is_assistant_manager_service_finalized_)
    return;
  is_assistant_manager_service_finalized_ = true;

  AddAshSessionObserver();

  ash::AssistantController::Get()->SetAssistant(
      assistant_manager_service_.get());
}

void Service::StopAssistantManagerService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  assistant_manager_service_->Stop();
  weak_ptr_factory_.InvalidateWeakPtrs();
  AssistantBrowserDelegate::Get()->OnAssistantStatusChanged(
      AssistantStatus::NOT_READY);
}

void Service::AddAshSessionObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No session controller in unittest.
  if (ash::SessionController::Get()) {
    // Note that this account can either be a regular account using real gaia,
    // or a fake gaia account.
    CoreAccountInfo account_info = RetrievePrimaryAccountInfo();
    AccountId account_id = user_manager::known_user::GetAccountId(
        account_info.email, account_info.gaia, AccountType::GOOGLE);
    scoped_ash_session_observer_ =
        std::make_unique<ScopedAshSessionObserver>(this, account_id);
  }
}

void Service::UpdateListeningState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool should_listen =
      !locked_ &&
      !ash::AssistantState::Get()->locked_full_screen_enabled().value_or(
          false) &&
      session_active_;
  DVLOG(1) << "Update assistant listening state: " << should_listen;
  assistant_manager_service_->EnableListening(should_listen);
  assistant_manager_service_->EnableHotword(should_listen &&
                                            ShouldEnableHotword());
}

absl::optional<AssistantManagerService::UserInfo> Service::GetUserInfo() const {
  if (access_token_) {
    return AssistantManagerService::UserInfo(RetrievePrimaryAccountInfo().gaia,
                                             access_token_.value());
  }
  return absl::nullopt;
}

bool Service::ShouldEnableHotword() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool dsp_available = context()->cras_audio_handler()->HasHotwordDevice();
  auto* assistant_state = ash::AssistantState::Get();

  // Disable hotword if hotword is not set to always on and power source is not
  // connected.
  if (!dsp_available && !assistant_state->hotword_always_on().value_or(false) &&
      !power_source_connected_) {
    return false;
  }

  return assistant_state->hotword_enabled().value();
}

}  // namespace assistant
}  // namespace chromeos
