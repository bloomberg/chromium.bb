// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"

#include "ash/components/phonehub/util/histogram_util.h"
#include "ash/components/proximity_auth/proximity_auth_pref_names.h"
#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/ash/android_sms/android_sms_pairing_state_tracker_impl.h"
#include "chrome/browser/ash/android_sms/android_sms_urls.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/nearby_share_feature_status.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

namespace settings {

namespace {

const char kPageContentDataModeKey[] = "mode";
const char kPageContentDataHostDeviceNameKey[] = "hostDeviceName";
const char kPageContentDataBetterTogetherStateKey[] = "betterTogetherState";
const char kPageContentDataInstantTetheringStateKey[] = "instantTetheringState";
const char kPageContentDataMessagesStateKey[] = "messagesState";
const char kPageContentDataPhoneHubStateKey[] = "phoneHubState";
const char kPageContentDataPhoneHubCameraRollStateKey[] =
    "phoneHubCameraRollState";
const char kPageContentDataPhoneHubNotificationsStateKey[] =
    "phoneHubNotificationsState";
const char kPageContentDataPhoneHubTaskContinuationStateKey[] =
    "phoneHubTaskContinuationState";
const char kPageContentDataPhoneHubAppsStateKey[] = "phoneHubAppsState";
const char kPageContentDataWifiSyncStateKey[] = "wifiSyncState";
const char kPageContentDataSmartLockStateKey[] = "smartLockState";
const char kNotificationAccessStatus[] = "notificationAccessStatus";
const char kIsAndroidSmsPairingComplete[] = "isAndroidSmsPairingComplete";
const char kIsNearbyShareDisallowedByPolicy[] =
    "isNearbyShareDisallowedByPolicy";
const char kIsPhoneHubAppsAccessGranted[] = "isPhoneHubAppsAccessGranted";
const char kIsPhoneHubPermissionsDialogSupported[] =
    "isPhoneHubPermissionsDialogSupported";
const char kIsCameraRollFilePermissionGranted[] =
    "isCameraRollFilePermissionGranted";

constexpr char kAndroidSmsInfoOriginKey[] = "origin";
constexpr char kAndroidSmsInfoEnabledKey[] = "enabled";

void OnRetrySetHostNowResult(bool success) {
  if (success)
    return;

  PA_LOG(WARNING) << "OnRetrySetHostNowResult(): Attempt to retry setting the "
                  << "host device failed.";
}

}  // namespace

MultideviceHandler::MultideviceHandler(
    PrefService* prefs,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    phonehub::NotificationAccessManager* notification_access_manager,
    multidevice_setup::AndroidSmsPairingStateTracker*
        android_sms_pairing_state_tracker,
    android_sms::AndroidSmsAppManager* android_sms_app_manager,
    ash::eche_app::AppsAccessManager* apps_access_manager,
    ash::phonehub::CameraRollManager* camera_roll_manager)
    : prefs_(prefs),
      multidevice_setup_client_(multidevice_setup_client),
      notification_access_manager_(notification_access_manager),
      android_sms_pairing_state_tracker_(android_sms_pairing_state_tracker),
      android_sms_app_manager_(android_sms_app_manager),
      apps_access_manager_(apps_access_manager),
      camera_roll_manager_(camera_roll_manager) {
  CHECK((multidevice_setup_client_ != nullptr) ==
        multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(prefs_));
  pref_change_registrar_.Init(prefs_);
}

MultideviceHandler::~MultideviceHandler() {}

void MultideviceHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "showMultiDeviceSetupDialog",
      base::BindRepeating(&MultideviceHandler::HandleShowMultiDeviceSetupDialog,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getPageContentData",
      base::BindRepeating(&MultideviceHandler::HandleGetPageContent,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "setFeatureEnabledState",
      base::BindRepeating(&MultideviceHandler::HandleSetFeatureEnabledState,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "removeHostDevice",
      base::BindRepeating(&MultideviceHandler::HandleRemoveHostDevice,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "retryPendingHostSetup",
      base::BindRepeating(&MultideviceHandler::HandleRetryPendingHostSetup,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "setUpAndroidSms",
      base::BindRepeating(&MultideviceHandler::HandleSetUpAndroidSms,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getSmartLockSignInEnabled",
      base::BindRepeating(&MultideviceHandler::HandleGetSmartLockSignInEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "setSmartLockSignInEnabled",
      base::BindRepeating(&MultideviceHandler::HandleSetSmartLockSignInEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getSmartLockSignInAllowed",
      base::BindRepeating(&MultideviceHandler::HandleGetSmartLockSignInAllowed,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getAndroidSmsInfo",
      base::BindRepeating(&MultideviceHandler::HandleGetAndroidSmsInfo,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "attemptNotificationSetup",
      base::BindRepeating(&MultideviceHandler::HandleAttemptNotificationSetup,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "cancelNotificationSetup",
      base::BindRepeating(&MultideviceHandler::HandleCancelNotificationSetup,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "attemptAppsSetup",
      base::BindRepeating(&MultideviceHandler::HandleAttemptAppsSetup,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "cancelAppsSetup",
      base::BindRepeating(&MultideviceHandler::HandleCancelAppsSetup,
                          base::Unretained(this)));
}

void MultideviceHandler::OnJavascriptAllowed() {
  if (multidevice_setup_client_)
    multidevice_setup_observation_.Observe(multidevice_setup_client_);

  if (notification_access_manager_)
    notification_access_manager_observation_.Observe(
        notification_access_manager_);

  if (android_sms_pairing_state_tracker_) {
    android_sms_pairing_state_tracker_observation_.Observe(
        android_sms_pairing_state_tracker_);
  }

  if (android_sms_app_manager_)
    android_sms_app_manager_observation_.Observe(android_sms_app_manager_);

  if (apps_access_manager_)
    apps_access_manager_observation_.Observe(apps_access_manager_);

  if (camera_roll_manager_)
    camera_roll_manager_observation_.Observe(camera_roll_manager_);

  pref_change_registrar_.Add(
      proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled,
      base::BindRepeating(
          &MultideviceHandler::NotifySmartLockSignInEnabledChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      multidevice_setup::kSmartLockSigninAllowedPrefName,
      base::BindRepeating(
          &MultideviceHandler::NotifySmartLockSignInAllowedChanged,
          base::Unretained(this)));
  if (NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          Profile::FromWebUI(web_ui()))) {
    pref_change_registrar_.Add(
        ::prefs::kNearbySharingEnabledPrefName,
        base::BindRepeating(&MultideviceHandler::OnNearbySharingEnabledChanged,
                            base::Unretained(this)));
  }
}

void MultideviceHandler::OnJavascriptDisallowed() {
  pref_change_registrar_.RemoveAll();

  if (multidevice_setup_client_) {
    DCHECK(multidevice_setup_observation_.IsObservingSource(
        multidevice_setup_client_));
    multidevice_setup_observation_.Reset();
  }

  if (notification_access_manager_) {
    DCHECK(notification_access_manager_observation_.IsObservingSource(
        notification_access_manager_));
    notification_access_manager_observation_.Reset();
    notification_access_operation_.reset();
  }

  if (android_sms_pairing_state_tracker_) {
    DCHECK(android_sms_pairing_state_tracker_observation_.IsObservingSource(
        android_sms_pairing_state_tracker_));
    android_sms_pairing_state_tracker_observation_.Reset();
  }

  if (android_sms_app_manager_) {
    DCHECK(android_sms_app_manager_observation_.IsObservingSource(
        android_sms_app_manager_));
    android_sms_app_manager_observation_.Reset();
  }

  if (apps_access_manager_) {
    DCHECK(apps_access_manager_observation_.IsObservingSource(
        apps_access_manager_));
    apps_access_manager_observation_.Reset();
    apps_access_operation_.reset();
  }

  if (camera_roll_manager_) {
    DCHECK(camera_roll_manager_observation_.IsObservingSource(
        camera_roll_manager_));
    camera_roll_manager_observation_.Reset();
  }

  // Ensure that pending callbacks do not complete and cause JS to be evaluated.
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
}

void MultideviceHandler::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_status_with_device) {
  UpdatePageContent();
  NotifyAndroidSmsInfoChange();
}

void MultideviceHandler::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  PA_LOG(INFO) << "Feature states have changed: "
               << multidevice_setup::FeatureStatesMapToString(
                      feature_states_map);
  UpdatePageContent();
  NotifyAndroidSmsInfoChange();
}

void MultideviceHandler::OnNotificationAccessChanged() {
  UpdatePageContent();
}

void MultideviceHandler::OnPairingStateChanged() {
  UpdatePageContent();
  NotifyAndroidSmsInfoChange();
}

void MultideviceHandler::OnInstalledAppUrlChanged() {
  UpdatePageContent();
  NotifyAndroidSmsInfoChange();
}

void MultideviceHandler::OnAppsAccessChanged() {
  UpdatePageContent();
}

void MultideviceHandler::OnCameraRollViewUiStateUpdated() {
  UpdatePageContent();
}

void MultideviceHandler::OnNearbySharingEnabledChanged() {
  UpdatePageContent();
}

void MultideviceHandler::UpdatePageContent() {
  std::unique_ptr<base::DictionaryValue> page_content_dictionary =
      GeneratePageContentDataDictionary();
  DCHECK(page_content_dictionary);
  PA_LOG(INFO) << "Updating MultiDevice settings page content with: "
               << *page_content_dictionary << ".";
  FireWebUIListener("settings.updateMultidevicePageContentData",
                    *page_content_dictionary);
}

void MultideviceHandler::NotifyAndroidSmsInfoChange() {
  auto android_sms_info = GenerateAndroidSmsInfo();
  FireWebUIListener("settings.onAndroidSmsInfoChange", *android_sms_info);
}

void MultideviceHandler::HandleShowMultiDeviceSetupDialog(
    const base::ListValue* args) {
  DCHECK(args->GetList().empty());
  multidevice_setup::MultiDeviceSetupDialog::Show();
}

void MultideviceHandler::HandleGetPageContent(const base::ListValue* args) {
  // This callback is expected to be the first one executed when the page is
  // loaded, so it should be the one to allow JS calls.
  AllowJavascript();

  const base::Value& callback_id = args->GetList()[0];
  DCHECK(callback_id.is_string());

  std::unique_ptr<base::DictionaryValue> page_content_dictionary =
      GeneratePageContentDataDictionary();
  DCHECK(page_content_dictionary);
  PA_LOG(INFO) << "Responding to getPageContentData() request with: "
               << *page_content_dictionary << ".";

  ResolveJavascriptCallback(callback_id, *page_content_dictionary);
}

void MultideviceHandler::HandleSetFeatureEnabledState(
    const base::ListValue* args) {
  const auto& list = args->GetList();
  DCHECK_GE(list.size(), 3u);
  std::string callback_id = list[0].GetString();

  int feature_as_int = list[1].GetInt();

  auto feature = static_cast<multidevice_setup::mojom::Feature>(feature_as_int);
  DCHECK(multidevice_setup::mojom::IsKnownEnumValue(feature));

  bool enabled = list[2].GetBool();

  absl::optional<std::string> auth_token;
  if (list.size() >= 4 && list[3].is_string())
    auth_token = list[3].GetString();

  multidevice_setup_client_->SetFeatureEnabledState(
      feature, enabled, auth_token,
      base::BindOnce(&MultideviceHandler::OnSetFeatureStateEnabledResult,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));

  if (enabled && feature == multidevice_setup::mojom::Feature::kPhoneHub) {
    phonehub::util::LogFeatureOptInEntryPoint(
        phonehub::util::OptInEntryPoint::kSettings);
  }

  if (enabled &&
      feature == multidevice_setup::mojom::Feature::kPhoneHubCameraRoll) {
    phonehub::util::LogCameraRollFeatureOptInEntryPoint(
        phonehub::util::CameraRollOptInEntryPoint::kSettings);
  }
}

void MultideviceHandler::HandleRemoveHostDevice(const base::ListValue* args) {
  DCHECK(args->GetList().empty());
  multidevice_setup_client_->RemoveHostDevice();
}

void MultideviceHandler::HandleRetryPendingHostSetup(
    const base::ListValue* args) {
  DCHECK(args->GetList().empty());
  multidevice_setup_client_->RetrySetHostNow(
      base::BindOnce(&OnRetrySetHostNowResult));
}

void MultideviceHandler::HandleSetUpAndroidSms(const base::ListValue* args) {
  DCHECK(args->GetList().empty());
  android_sms_app_manager_->SetUpAndLaunchAndroidSmsApp();
}

void MultideviceHandler::HandleGetSmartLockSignInEnabled(
    const base::ListValue* args) {
  const base::Value& callback_id = args->GetList()[0];
  CHECK(callback_id.is_string());

  bool signInEnabled = prefs_->GetBoolean(
      proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled);
  ResolveJavascriptCallback(callback_id, base::Value(signInEnabled));
}

void MultideviceHandler::HandleSetSmartLockSignInEnabled(
    const base::ListValue* args) {
  bool enabled = false;
  if (args->GetList()[0].is_bool())
    enabled = args->GetList()[0].GetBool();

  const bool auth_token_present =
      args->GetList().size() >= 2 && args->GetList()[1].is_string();
  const std::string& auth_token =
      auth_token_present ? args->GetList()[1].GetString() : "";

  // Either the user is disabling sign-in, or they are enabling it and the auth
  // token must be present.
  CHECK(!enabled || auth_token_present);

  // Only check auth token if the user is attempting to enable sign-in.
  if (enabled && !IsAuthTokenValid(auth_token))
    return;

  prefs_->SetBoolean(
      proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled, enabled);
}

void MultideviceHandler::HandleGetSmartLockSignInAllowed(
    const base::ListValue* args) {
  const base::Value& callback_id = args->GetList()[0];
  CHECK(callback_id.is_string());

  bool sign_in_allowed =
      prefs_->GetBoolean(multidevice_setup::kSmartLockSigninAllowedPrefName);
  ResolveJavascriptCallback(callback_id, base::Value(sign_in_allowed));
}

std::unique_ptr<base::DictionaryValue>
MultideviceHandler::GenerateAndroidSmsInfo() {
  absl::optional<GURL> app_url;
  if (android_sms_app_manager_)
    app_url = android_sms_app_manager_->GetCurrentAppUrl();
  if (!app_url)
    app_url = android_sms::GetAndroidMessagesURL();

  auto android_sms_info = std::make_unique<base::DictionaryValue>();
  android_sms_info->SetString(
      kAndroidSmsInfoOriginKey,
      ContentSettingsPattern::FromURLNoWildcard(*app_url).ToString());

  chromeos::multidevice_setup::mojom::FeatureState messages_state =
      multidevice_setup_client_->GetFeatureState(
          chromeos::multidevice_setup::mojom::Feature::kMessages);
  bool enabled_state =
      messages_state ==
          chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser ||
      messages_state == chromeos::multidevice_setup::mojom::FeatureState::
                            kFurtherSetupRequired;
  android_sms_info->SetBoolean(kAndroidSmsInfoEnabledKey, enabled_state);

  return android_sms_info;
}

void MultideviceHandler::HandleGetAndroidSmsInfo(const base::ListValue* args) {
  const base::Value& callback_id = args->GetList()[0];

  ResolveJavascriptCallback(callback_id, *GenerateAndroidSmsInfo());
}

void MultideviceHandler::HandleAttemptNotificationSetup(
    const base::ListValue* args) {
  DCHECK(features::IsPhoneHubEnabled());
  DCHECK(!notification_access_operation_);

  phonehub::NotificationAccessManager::AccessStatus access_status =
      notification_access_manager_->GetAccessStatus();
  if (access_status != phonehub::NotificationAccessManager::AccessStatus::
                           kAvailableButNotGranted) {
    PA_LOG(WARNING) << "Cannot request notification access setup flow; current "
                    << "status: " << access_status;
    return;
  }

  notification_access_operation_ =
      notification_access_manager_->AttemptNotificationSetup(/*delegate=*/this);
  DCHECK(notification_access_operation_);
}

void MultideviceHandler::HandleCancelNotificationSetup(
    const base::ListValue* args) {
  DCHECK(features::IsPhoneHubEnabled());
  DCHECK(notification_access_operation_);

  notification_access_operation_.reset();
}

void MultideviceHandler::HandleAttemptAppsSetup(const base::ListValue* args) {
  DCHECK(features::IsEcheSWAEnabled());
  DCHECK(features::IsEchePhoneHubPermissionsOnboarding());
  DCHECK(!apps_access_operation_);

  ash::eche_app::AppsAccessManager::AccessStatus apps_access_status =
      apps_access_manager_->GetAccessStatus();

  if (apps_access_status !=
      ash::eche_app::AppsAccessManager::AccessStatus::kAvailableButNotGranted) {
    PA_LOG(WARNING) << "Cannot request apps access setup flow; current "
                    << "status: " << apps_access_status;
    return;
  }

  apps_access_operation_ =
      apps_access_manager_->AttemptAppsAccessSetup(/*delegate=*/this);
  DCHECK(apps_access_operation_);
}

void MultideviceHandler::HandleCancelAppsSetup(const base::ListValue* args) {
  DCHECK(features::IsEcheSWAEnabled());
  DCHECK(features::IsEchePhoneHubPermissionsOnboarding());
  DCHECK(apps_access_operation_);

  apps_access_operation_.reset();
}

void MultideviceHandler::OnStatusChange(
    phonehub::NotificationAccessSetupOperation::Status new_status) {
  FireWebUIListener("settings.onNotificationAccessSetupStatusChanged",
                    base::Value(static_cast<int32_t>(new_status)));

  if (phonehub::NotificationAccessSetupOperation::IsFinalStatus(new_status))
    notification_access_operation_.reset();
}

void MultideviceHandler::OnAppsStatusChange(
    ash::eche_app::AppsAccessSetupOperation::Status new_status) {
  FireWebUIListener("settings.onAppsAccessSetupStatusChanged",
                    base::Value(static_cast<int32_t>(new_status)));

  if (ash::eche_app::AppsAccessSetupOperation::IsFinalStatus(new_status))
    apps_access_operation_.reset();
}

void MultideviceHandler::OnSetFeatureStateEnabledResult(
    const std::string& js_callback_id,
    bool success) {
  ResolveJavascriptCallback(base::Value(js_callback_id), base::Value(success));
}

std::unique_ptr<base::DictionaryValue>
MultideviceHandler::GeneratePageContentDataDictionary() {
  auto page_content_dictionary = std::make_unique<base::DictionaryValue>();

  multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
      host_status_with_device = GetHostStatusWithDevice();
  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap feature_states =
      GetFeatureStatesMap();

  page_content_dictionary->SetInteger(
      kPageContentDataModeKey,
      static_cast<int32_t>(host_status_with_device.first));
  page_content_dictionary->SetInteger(
      kPageContentDataBetterTogetherStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kBetterTogetherSuite]));
  page_content_dictionary->SetInteger(
      kPageContentDataInstantTetheringStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kInstantTethering]));
  page_content_dictionary->SetInteger(
      kPageContentDataMessagesStateKey,
      static_cast<int32_t>(
          feature_states[multidevice_setup::mojom::Feature::kMessages]));
  page_content_dictionary->SetInteger(
      kPageContentDataSmartLockStateKey,
      static_cast<int32_t>(
          feature_states[multidevice_setup::mojom::Feature::kSmartLock]));
  page_content_dictionary->SetInteger(
      kPageContentDataPhoneHubStateKey,
      static_cast<int32_t>(
          feature_states[multidevice_setup::mojom::Feature::kPhoneHub]));
  page_content_dictionary->SetInteger(
      kPageContentDataPhoneHubCameraRollStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kPhoneHubCameraRoll]));
  page_content_dictionary->SetInteger(
      kPageContentDataPhoneHubNotificationsStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kPhoneHubNotifications]));
  page_content_dictionary->SetInteger(
      kPageContentDataPhoneHubTaskContinuationStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kPhoneHubTaskContinuation]));
  page_content_dictionary->SetInteger(
      kPageContentDataPhoneHubAppsStateKey,
      static_cast<int32_t>(
          feature_states[multidevice_setup::mojom::Feature::kEche]));

  page_content_dictionary->SetInteger(
      kPageContentDataWifiSyncStateKey,
      static_cast<int32_t>(
          feature_states[multidevice_setup::mojom::Feature::kWifiSync]));

  if (host_status_with_device.second) {
    page_content_dictionary->SetString(kPageContentDataHostDeviceNameKey,
                                       host_status_with_device.second->name());
  }

  page_content_dictionary->SetBoolean(
      kIsAndroidSmsPairingComplete,
      android_sms_pairing_state_tracker_
          ? android_sms_pairing_state_tracker_->IsAndroidSmsPairingComplete()
          : false);

  phonehub::NotificationAccessManager::AccessStatus access_status = phonehub::
      NotificationAccessManager::AccessStatus::kAvailableButNotGranted;
  if (notification_access_manager_)
    access_status = notification_access_manager_->GetAccessStatus();
  page_content_dictionary->SetInteger(kNotificationAccessStatus,
                                      static_cast<int32_t>(access_status));

  ash::eche_app::AppsAccessManager::AccessStatus apps_access_status =
      ash::eche_app::AppsAccessManager::AccessStatus::kAvailableButNotGranted;
  if (apps_access_manager_)
    apps_access_status = apps_access_manager_->GetAccessStatus();
  bool is_apps_access_granted =
      apps_access_status ==
      ash::eche_app::AppsAccessManager::AccessStatus::kAccessGranted;

  page_content_dictionary->SetBoolean(kIsPhoneHubAppsAccessGranted,
                                      is_apps_access_granted);

  bool is_camera_roll_file_permission_granted = false;
  if (camera_roll_manager_) {
    is_camera_roll_file_permission_granted =
        camera_roll_manager_->ui_state() !=
        ash::phonehub::CameraRollManager::CameraRollUiState::
            NO_STORAGE_PERMISSION;
  }
  page_content_dictionary->SetBoolean(kIsCameraRollFilePermissionGranted,
                                      is_camera_roll_file_permission_granted);

  bool is_nearby_share_disallowed_by_policy =
      NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          Profile::FromWebUI(web_ui())) &&
      (GetNearbyShareEnabledState(prefs_) ==
       NearbyShareEnabledState::kDisallowedByPolicy);
  page_content_dictionary->SetBoolean(kIsNearbyShareDisallowedByPolicy,
                                      is_nearby_share_disallowed_by_policy);

  page_content_dictionary->SetBoolean(
      kIsPhoneHubPermissionsDialogSupported,
      base::FeatureList::IsEnabled(
          chromeos::features::kEchePhoneHubPermissionsOnboarding));

  return page_content_dictionary;
}

void MultideviceHandler::NotifySmartLockSignInEnabledChanged() {
  bool sign_in_enabled = prefs_->GetBoolean(
      proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled);
  FireWebUIListener("smart-lock-signin-enabled-changed",
                    base::Value(sign_in_enabled));
}

void MultideviceHandler::NotifySmartLockSignInAllowedChanged() {
  bool sign_in_allowed =
      prefs_->GetBoolean(multidevice_setup::kSmartLockSigninAllowedPrefName);
  FireWebUIListener("smart-lock-signin-allowed-changed",
                    base::Value(sign_in_allowed));
}

bool MultideviceHandler::IsAuthTokenValid(const std::string& auth_token) {
  Profile* profile = Profile::FromWebUI(web_ui());
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile);
  return quick_unlock_storage->GetAuthToken() &&
         auth_token == quick_unlock_storage->GetAuthToken()->Identifier();
}

multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
MultideviceHandler::GetHostStatusWithDevice() {
  if (multidevice_setup_client_)
    return multidevice_setup_client_->GetHostStatus();

  return multidevice_setup::MultiDeviceSetupClient::
      GenerateDefaultHostStatusWithDevice();
}

multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
MultideviceHandler::GetFeatureStatesMap() {
  if (multidevice_setup_client_)
    return multidevice_setup_client_->GetFeatureStates();

  PA_LOG(WARNING)
      << "MultiDevice setup client missing. Responding to "
         "GetFeatureStatesMap() request by generating default feature map.";
  return multidevice_setup::MultiDeviceSetupClient::
      GenerateDefaultFeatureStatesMap(
          multidevice_setup::mojom::FeatureState::kProhibitedByPolicy);
}

}  // namespace settings

}  // namespace chromeos
