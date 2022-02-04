// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/multidevice_internals/multidevice_internals_phone_hub_handler.h"

#include "ash/components/phonehub/camera_roll_item.h"
#include "ash/components/phonehub/fake_phone_hub_manager.h"
#include "ash/components/phonehub/notification.h"
#include "ash/components/phonehub/pref_names.h"
#include "ash/public/cpp/system_tray.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace chromeos {
namespace multidevice {

namespace {

const int kIconSize = 16;
const int kContactImageSize = 80;
const int kSharedImageSize = 400;
const int kCameraRollThumbnailSize = 96;

// Fake image types used for fields that require gfx::Image().
enum class ImageType {
  kPink = 1,
  kRed = 2,
  kGreen = 3,
  kBlue = 4,
  kYellow = 5,
};

const SkBitmap ImageTypeToBitmap(ImageType image_type_num, int size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  switch (image_type_num) {
    case ImageType::kPink:
      bitmap.eraseARGB(255, 255, 192, 203);
      break;
    case ImageType::kRed:
      bitmap.eraseARGB(255, 255, 0, 0);
      break;
    case ImageType::kGreen:
      bitmap.eraseARGB(255, 0, 255, 0);
      break;
    case ImageType::kBlue:
      bitmap.eraseARGB(255, 0, 0, 255);
      break;
    case ImageType::kYellow:
      bitmap.eraseARGB(255, 255, 255, 0);
      break;
    default:
      break;
  }
  return bitmap;
}

phonehub::Notification::AppMetadata DictToAppMetadata(
    const base::DictionaryValue* app_metadata_dict) {
  std::u16string visible_app_name;
  CHECK(app_metadata_dict->GetString("visibleAppName", &visible_app_name));

  std::string package_name;
  CHECK(app_metadata_dict->GetString("packageName", &package_name));

  absl::optional<int> icon_image_type_as_int =
      app_metadata_dict->FindIntKey("icon");
  CHECK(icon_image_type_as_int);

  auto icon_image_type = static_cast<ImageType>(*icon_image_type_as_int);
  gfx::Image icon = gfx::Image::CreateFrom1xBitmap(
      ImageTypeToBitmap(icon_image_type, kIconSize));

  int user_id = app_metadata_dict->FindIntKey("userId").value_or(0);

  return phonehub::Notification::AppMetadata(visible_app_name, package_name,
                                             icon, user_id);
}

void TryAddingMetadata(
    const std::string& key,
    const base::DictionaryValue* browser_tab_status_dict,
    std::vector<phonehub::BrowserTabsModel::BrowserTabMetadata>& metadatas) {
  const base::DictionaryValue* browser_tab_metadata = nullptr;

  if (!browser_tab_status_dict->GetDictionary(key, &browser_tab_metadata))
    return;

  std::string url;
  if (!browser_tab_metadata->GetString("url", &url) || url.empty())
    return;

  std::u16string title;
  if (!browser_tab_metadata->GetString("title", &title) || title.empty())
    return;

  // JavaScript time stamps don't fit in int.
  absl::optional<double> last_accessed_timestamp =
      browser_tab_metadata->FindDoubleKey("lastAccessedTimeStamp");
  if (!last_accessed_timestamp)
    return;

  int favicon_image_type_as_int =
      browser_tab_metadata->FindIntKey("favicon").value_or(0);
  if (!favicon_image_type_as_int)
    return;

  auto favicon_image_type = static_cast<ImageType>(favicon_image_type_as_int);
  gfx::Image favicon = gfx::Image::CreateFrom1xBitmap(
      ImageTypeToBitmap(favicon_image_type, kIconSize));

  auto metadata = phonehub::BrowserTabsModel::BrowserTabMetadata(
      GURL(url), title, base::Time::FromJsTime(*last_accessed_timestamp),
      favicon);

  metadatas.push_back(metadata);
}

const SkBitmap RGB_Bitmap(U8CPU r, U8CPU g, U8CPU b, int size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseARGB(255, r, g, b);
  return bitmap;
}

}  // namespace

MultidevicePhoneHubHandler::MultidevicePhoneHubHandler() = default;

MultidevicePhoneHubHandler::~MultidevicePhoneHubHandler() {
  if (fake_phone_hub_manager_)
    EnableRealPhoneHubManager();
}

void MultidevicePhoneHubHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "setFakePhoneHubManagerEnabled",
      base::BindRepeating(
          &MultidevicePhoneHubHandler::HandleEnableFakePhoneHubManager,
          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "setFeatureStatus",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetFeatureStatus,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "setShowOnboardingFlow",
      base::BindRepeating(
          &MultidevicePhoneHubHandler::HandleSetShowOnboardingFlow,
          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "setFakePhoneName",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetFakePhoneName,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "setFakePhoneStatus",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetFakePhoneStatus,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "setBrowserTabs",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetBrowserTabs,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "setNotification",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetNotification,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "removeNotification",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleRemoveNotification,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "enableDnd",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleEnableDnd,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "setFindMyDeviceStatus",
      base::BindRepeating(
          &MultidevicePhoneHubHandler::HandleSetFindMyDeviceStatus,
          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "setTetherStatus",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetTetherStatus,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "resetShouldShowOnboardingUi",
      base::BindRepeating(
          &MultidevicePhoneHubHandler::HandleResetShouldShowOnboardingUi,
          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "resetHasNotificationSetupUiBeenDismissed",
      base::BindRepeating(&MultidevicePhoneHubHandler::
                              HandleResetHasNotificationSetupUiBeenDismissed,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "resetCameraRollOnboardingUiDismissed",
      base::BindRepeating(&MultidevicePhoneHubHandler::
                              HandleResetCameraRollOnboardingUiDismissed,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "setFakeCameraRoll",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetFakeCameraRoll,
                          base::Unretained(this)));
}

void MultidevicePhoneHubHandler::OnJavascriptDisallowed() {
  RemoveObservers();
}

void MultidevicePhoneHubHandler::AddObservers() {
  notification_manager_observation_.Observe(
      fake_phone_hub_manager_->fake_notification_manager());
  do_not_disturb_controller_observation_.Observe(
      fake_phone_hub_manager_->fake_do_not_disturb_controller());
  find_my_device_controller_observation_.Observe(
      fake_phone_hub_manager_->fake_find_my_device_controller());
  tether_controller_observation_.Observe(
      fake_phone_hub_manager_->fake_tether_controller());
  onboarding_ui_tracker_observation_.Observe(
      fake_phone_hub_manager_->fake_onboarding_ui_tracker());
  camera_roll_manager_observation_.Observe(
      fake_phone_hub_manager_->fake_camera_roll_manager());
}

void MultidevicePhoneHubHandler::RemoveObservers() {
  notification_manager_observation_.Reset();
  do_not_disturb_controller_observation_.Reset();
  find_my_device_controller_observation_.Reset();
  tether_controller_observation_.Reset();
  onboarding_ui_tracker_observation_.Reset();
  camera_roll_manager_observation_.Reset();
}

void MultidevicePhoneHubHandler::OnNotificationsRemoved(
    const base::flat_set<int64_t>& notification_ids) {
  base::ListValue removed_notification_id_js_list;
  for (const int64_t& id : notification_ids) {
    removed_notification_id_js_list.Append(static_cast<double>(id));
  }
  FireWebUIListener("removed-notification-ids",
                    removed_notification_id_js_list);
}

void MultidevicePhoneHubHandler::OnDndStateChanged() {
  bool is_dnd_enabled =
      fake_phone_hub_manager_->fake_do_not_disturb_controller()->IsDndEnabled();
  FireWebUIListener("is-dnd-enabled-changed", base::Value(is_dnd_enabled));
}

void MultidevicePhoneHubHandler::OnPhoneRingingStateChanged() {
  phonehub::FindMyDeviceController::Status ringing_status =
      fake_phone_hub_manager_->fake_find_my_device_controller()
          ->GetPhoneRingingStatus();

  FireWebUIListener("find-my-device-status-changed",
                    base::Value(static_cast<int>(ringing_status)));
}

void MultidevicePhoneHubHandler::OnTetherStatusChanged() {
  int status_as_int = static_cast<int>(
      fake_phone_hub_manager_->fake_tether_controller()->GetStatus());
  FireWebUIListener("tether-status-changed", base::Value(status_as_int));
}

void MultidevicePhoneHubHandler::OnShouldShowOnboardingUiChanged() {
  bool should_show_onboarding_ui =
      fake_phone_hub_manager_->fake_onboarding_ui_tracker()
          ->ShouldShowOnboardingUi();
  FireWebUIListener("should-show-onboarding-ui-changed",
                    base::Value(should_show_onboarding_ui));
}

void MultidevicePhoneHubHandler::OnCameraRollViewUiStateUpdated() {
  base::Value camera_roll_dict(base::Value::Type::DICTIONARY);
  camera_roll_dict.SetBoolKey(
      "isCameraRollEnabled", fake_phone_hub_manager_->fake_camera_roll_manager()
                                 ->is_camera_roll_enabled());
  camera_roll_dict.SetBoolKey(
      "isOnboardingDismissed",
      fake_phone_hub_manager_->fake_camera_roll_manager()
          ->is_onboarding_dismissed());
  camera_roll_dict.SetBoolKey(
      "isLoadingViewShown", fake_phone_hub_manager_->fake_camera_roll_manager()
                                ->is_loading_view_shown());
  FireWebUIListener("camera-roll-ui-view-state-updated", camera_roll_dict);
}

void MultidevicePhoneHubHandler::HandleEnableDnd(const base::ListValue* args) {
  const auto& list = args->GetList();
  CHECK(!list.empty());
  const bool enabled = list[0].GetBool();
  PA_LOG(VERBOSE) << "Setting Do Not Disturb state to " << enabled;
  fake_phone_hub_manager_->fake_do_not_disturb_controller()
      ->SetDoNotDisturbStateInternal(enabled,
                                     /*can_request_new_dnd_state=*/true);
}

void MultidevicePhoneHubHandler::HandleSetFindMyDeviceStatus(
    const base::ListValue* args) {
  const auto& list = args->GetList();
  CHECK_GE(list.size(), 1u);
  int status_as_int = list[0].GetInt();

  auto status =
      static_cast<phonehub::FindMyDeviceController::Status>(status_as_int);
  PA_LOG(VERBOSE) << "Setting phone ringing status to " << status;
  fake_phone_hub_manager_->fake_find_my_device_controller()
      ->SetPhoneRingingState(status);
}

void MultidevicePhoneHubHandler::HandleSetTetherStatus(
    const base::ListValue* args) {
  const auto& list = args->GetList();
  CHECK_GE(list.size(), 1u);
  int status_as_int = list[0].GetInt();

  auto status = static_cast<phonehub::TetherController::Status>(status_as_int);
  PA_LOG(VERBOSE) << "Setting tether status to " << status;
  fake_phone_hub_manager_->fake_tether_controller()->SetStatus(status);
}

void MultidevicePhoneHubHandler::EnableRealPhoneHubManager() {
  // If no FakePhoneHubManager is active, return early. This ensures that we
  // don't unnecessarily re-initialize the Phone Hub UI.
  if (!fake_phone_hub_manager_)
    return;

  PA_LOG(VERBOSE) << "Setting real Phone Hub Manager";
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* phone_hub_manager =
      phonehub::PhoneHubManagerFactory::GetForProfile(profile);
  ash::SystemTray::Get()->SetPhoneHubManager(phone_hub_manager);

  RemoveObservers();
  fake_phone_hub_manager_.reset();
}

void MultidevicePhoneHubHandler::EnableFakePhoneHubManager() {
  DCHECK(!fake_phone_hub_manager_);
  PA_LOG(VERBOSE) << "Setting fake Phone Hub Manager";
  fake_phone_hub_manager_ = std::make_unique<phonehub::FakePhoneHubManager>();
  ash::SystemTray::Get()->SetPhoneHubManager(fake_phone_hub_manager_.get());
  AddObservers();
}

void MultidevicePhoneHubHandler::HandleEnableFakePhoneHubManager(
    const base::ListValue* args) {
  AllowJavascript();
  const auto& list = args->GetList();
  CHECK(!list.empty());
  const bool enabled = list[0].GetBool();
  if (enabled) {
    EnableFakePhoneHubManager();
    return;
  }
  EnableRealPhoneHubManager();
}

void MultidevicePhoneHubHandler::HandleSetFeatureStatus(
    const base::ListValue* args) {
  const auto& list = args->GetList();
  CHECK_GE(list.size(), 1u);
  int feature_as_int = list[0].GetInt();

  auto feature = static_cast<phonehub::FeatureStatus>(feature_as_int);
  PA_LOG(VERBOSE) << "Setting feature status to " << feature;
  fake_phone_hub_manager_->fake_feature_status_provider()->SetStatus(feature);
}

void MultidevicePhoneHubHandler::HandleSetShowOnboardingFlow(
    const base::ListValue* args) {
  const auto& list = args->GetList();
  CHECK(!list.empty());
  const bool show_onboarding_flow = list[0].GetBool();
  PA_LOG(VERBOSE) << "Setting show onboarding flow to " << show_onboarding_flow;
  fake_phone_hub_manager_->fake_onboarding_ui_tracker()
      ->SetShouldShowOnboardingUi(show_onboarding_flow);
}

void MultidevicePhoneHubHandler::HandleSetFakePhoneName(
    const base::ListValue* args) {
  base::Value::ConstListView args_list = args->GetList();
  CHECK_GE(args_list.size(), 1u);
  std::u16string phone_name = base::UTF8ToUTF16(args_list[0].GetString());
  fake_phone_hub_manager_->mutable_phone_model()->SetPhoneName(phone_name);
  PA_LOG(VERBOSE) << "Set phone name to " << phone_name;
}

void MultidevicePhoneHubHandler::HandleSetFakePhoneStatus(
    const base::ListValue* args) {
  const base::Value& phones_status_value = args->GetList()[0];
  CHECK(phones_status_value.is_dict());

  absl::optional<int> mobile_status_as_int =
      phones_status_value.FindIntKey("mobileStatus");
  CHECK(mobile_status_as_int);
  auto mobile_status = static_cast<phonehub::PhoneStatusModel::MobileStatus>(
      *mobile_status_as_int);

  absl::optional<int> signal_strength_as_int =
      phones_status_value.FindIntKey("signalStrength");
  CHECK(signal_strength_as_int);

  auto signal_strength =
      static_cast<phonehub::PhoneStatusModel::SignalStrength>(
          *signal_strength_as_int);

  const base::DictionaryValue* phones_status_dict =
      static_cast<const base::DictionaryValue*>(&phones_status_value);
  std::u16string mobile_provider;
  CHECK(phones_status_dict->GetString("mobileProvider", &mobile_provider));

  absl::optional<int> charging_state_as_int =
      phones_status_value.FindIntKey("chargingState");
  CHECK(charging_state_as_int);
  auto charging_state = static_cast<phonehub::PhoneStatusModel::ChargingState>(
      *charging_state_as_int);

  absl::optional<int> battery_saver_state_as_int =
      phones_status_value.FindIntKey("batterySaverState");
  CHECK(battery_saver_state_as_int);
  auto battery_saver_state =
      static_cast<phonehub::PhoneStatusModel::BatterySaverState>(
          *battery_saver_state_as_int);

  absl::optional<int> battery_percentage =
      phones_status_value.FindIntKey("batteryPercentage");
  CHECK(battery_percentage);

  phonehub::PhoneStatusModel::MobileConnectionMetadata connection_metadata = {
      .signal_strength = signal_strength,
      .mobile_provider = mobile_provider,
  };
  auto phone_status = phonehub::PhoneStatusModel(
      mobile_status, connection_metadata, charging_state, battery_saver_state,
      *battery_percentage);
  fake_phone_hub_manager_->mutable_phone_model()->SetPhoneStatusModel(
      phone_status);

  PA_LOG(VERBOSE) << "Set phone status to -"
                  << "\n  mobile status: " << mobile_status
                  << "\n  signal strength: " << signal_strength
                  << "\n  mobile provider: " << mobile_provider
                  << "\n  charging state: " << charging_state
                  << "\n  battery saver state: " << battery_saver_state
                  << "\n  battery percentage: " << *battery_percentage;
}

void MultidevicePhoneHubHandler::HandleSetBrowserTabs(
    const base::ListValue* args) {
  const base::Value& browser_tab_status_value = args->GetList()[0];
  CHECK(browser_tab_status_value.is_dict());
  const base::DictionaryValue* browser_tab_status_dict =
      static_cast<const base::DictionaryValue*>(&browser_tab_status_value);
  bool is_tab_sync_enabled =
      browser_tab_status_dict->FindBoolKey("isTabSyncEnabled").value();

  if (!is_tab_sync_enabled) {
    fake_phone_hub_manager_->mutable_phone_model()->SetBrowserTabsModel(
        phonehub::BrowserTabsModel(is_tab_sync_enabled));
    PA_LOG(VERBOSE) << "Tab sync off; cleared browser tab metadata";
    return;
  }

  std::vector<phonehub::BrowserTabsModel::BrowserTabMetadata> metadatas;
  TryAddingMetadata("browserTabOneMetadata", browser_tab_status_dict,
                    metadatas);
  TryAddingMetadata("browserTabTwoMetadata", browser_tab_status_dict,
                    metadatas);

  fake_phone_hub_manager_->mutable_phone_model()->SetBrowserTabsModel(
      phonehub::BrowserTabsModel(is_tab_sync_enabled, metadatas));

  auto browser_tabs_model =
      fake_phone_hub_manager_->mutable_phone_model()->browser_tabs_model();
  CHECK(browser_tabs_model.has_value());

  // Log the most recently visited browser tab (at index 0) last.
  for (int i = metadatas.size() - 1; i > -1; --i) {
    PA_LOG(VERBOSE) << "Set most recent browser tab number " << i
                    << " to: " << browser_tabs_model->most_recent_tabs()[i];
  }
}

void MultidevicePhoneHubHandler::HandleSetNotification(
    const base::ListValue* args) {
  const base::Value& notification_data_value = args->GetList()[0];
  CHECK(notification_data_value.is_dict());

  absl::optional<int> id = notification_data_value.FindIntKey("id");
  CHECK(id);

  const base::DictionaryValue* notification_data_dict =
      static_cast<const base::DictionaryValue*>(&notification_data_value);
  const base::DictionaryValue* app_metadata_dict = nullptr;
  CHECK(
      notification_data_dict->GetDictionary("appMetadata", &app_metadata_dict));
  phonehub::Notification::AppMetadata app_metadata =
      DictToAppMetadata(app_metadata_dict);

  // JavaScript time stamps don't fit in int.
  absl::optional<double> js_timestamp =
      notification_data_value.FindDoubleKey("timestamp");
  CHECK(js_timestamp);
  auto timestamp = base::Time::FromJsTime(*js_timestamp);

  absl::optional<int> importance_as_int =
      notification_data_value.FindIntKey("importance");
  CHECK(importance_as_int);
  auto importance =
      static_cast<phonehub::Notification::Importance>(*importance_as_int);

  absl::optional<int> inline_reply_id =
      notification_data_value.FindIntKey("inlineReplyId");
  CHECK(inline_reply_id);

  absl::optional<std::u16string> opt_title;
  std::u16string title;
  if (notification_data_dict->GetString("title", &title) && !title.empty()) {
    opt_title = title;
  }

  absl::optional<std::u16string> opt_text_content;
  std::u16string text_content;
  if (notification_data_dict->GetString("textContent", &text_content) &&
      !text_content.empty()) {
    opt_text_content = text_content;
  }

  absl::optional<gfx::Image> opt_shared_image;
  int shared_image_type_as_int =
      notification_data_value.FindIntKey("sharedImage").value_or(0);
  if (shared_image_type_as_int) {
    auto shared_image_type = static_cast<ImageType>(shared_image_type_as_int);
    opt_shared_image = gfx::Image::CreateFrom1xBitmap(
        ImageTypeToBitmap(shared_image_type, kSharedImageSize));
  }

  absl::optional<gfx::Image> opt_contact_image;
  int contact_image_type_as_int =
      notification_data_value.FindIntKey("contactImage").value_or(0);
  if (contact_image_type_as_int) {
    auto shared_contact_image_type =
        static_cast<ImageType>(contact_image_type_as_int);
    opt_contact_image = gfx::Image::CreateFrom1xBitmap(
        ImageTypeToBitmap(shared_contact_image_type, kContactImageSize));
  }

  auto notification = phonehub::Notification(
      *id, app_metadata, timestamp, importance,
      phonehub::Notification::Category::kConversation,
      {{phonehub::Notification::ActionType::kInlineReply, *inline_reply_id}},
      phonehub::Notification::InteractionBehavior::kNone, opt_title,
      opt_text_content, opt_shared_image, opt_contact_image);

  PA_LOG(VERBOSE) << "Set notification" << notification;
  fake_phone_hub_manager_->fake_notification_manager()->SetNotification(
      std::move(notification));
}

void MultidevicePhoneHubHandler::HandleRemoveNotification(
    const base::ListValue* args) {
  const auto& list = args->GetList();
  CHECK_GE(list.size(), 1u);
  int notification_id = list[0].GetInt();
  fake_phone_hub_manager_->fake_notification_manager()->RemoveNotification(
      notification_id);
  PA_LOG(VERBOSE) << "Removed notification with id " << notification_id;
}

void MultidevicePhoneHubHandler::HandleResetShouldShowOnboardingUi(
    const base::ListValue* args) {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  prefs->SetBoolean(phonehub::prefs::kHideOnboardingUi, false);
  PA_LOG(VERBOSE) << "Reset kHideOnboardingUi pref";
}

void MultidevicePhoneHubHandler::HandleResetHasNotificationSetupUiBeenDismissed(
    const base::ListValue* args) {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  prefs->SetBoolean(phonehub::prefs::kHasDismissedSetupRequiredUi, false);
  PA_LOG(VERBOSE) << "Reset kHasDismissedSetupRequiredUi pref";
}

void MultidevicePhoneHubHandler::HandleResetCameraRollOnboardingUiDismissed(
    const base::ListValue* args) {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  prefs->SetBoolean(
      chromeos::phonehub::prefs::kHasDismissedCameraRollOnboardingUi, false);
  PA_LOG(VERBOSE) << "Reset kHasDismissedCameraRollOnboardingUi pref";
}

void MultidevicePhoneHubHandler::HandleSetFakeCameraRoll(
    const base::ListValue* args) {
  const base::Value& camera_roll_dict = args->GetList()[0];
  CHECK(camera_roll_dict.is_dict());

  absl::optional<bool> is_camera_roll_enabled =
      camera_roll_dict.FindBoolKey("isCameraRollEnabled");
  CHECK(is_camera_roll_enabled);

  fake_phone_hub_manager_->fake_camera_roll_manager()
      ->SetIsCameraRollAvailableToBeEnabled(!*is_camera_roll_enabled);

  absl::optional<bool> is_onboarding_dismissed =
      camera_roll_dict.FindBoolKey("isOnboardingDismissed");
  CHECK(is_onboarding_dismissed);

  fake_phone_hub_manager_->fake_camera_roll_manager()
      ->SetIsCameraRollOnboardingDismissed(*is_onboarding_dismissed);

  absl::optional<bool> is_file_access_granted =
      camera_roll_dict.FindBoolKey("isFileAccessGranted");
  CHECK(is_file_access_granted);

  fake_phone_hub_manager_->fake_camera_roll_manager()
      ->SetIsAndroidStorageGranted(*is_file_access_granted);

  absl::optional<bool> is_loading_view_shown =
      camera_roll_dict.FindBoolKey("isLoadingViewShown");
  CHECK(is_loading_view_shown);

  fake_phone_hub_manager_->fake_camera_roll_manager()
      ->SetIsCameraRollLoadingViewShown(*is_loading_view_shown);

  absl::optional<int> number_of_thumbnails =
      camera_roll_dict.FindIntKey("numberOfThumbnails");
  CHECK(number_of_thumbnails);

  absl::optional<int> file_type_as_int =
      camera_roll_dict.FindIntKey("fileType");
  CHECK(file_type_as_int);
  const char* file_type;
  const char* file_ext;
  if (*file_type_as_int == 0) {
    file_type = "image/jpeg";
    file_ext = ".jpg";
  } else {
    file_type = "video/mp4";
    file_ext = ".mp4";
  }

  if (*number_of_thumbnails == 0) {
    fake_phone_hub_manager_->fake_camera_roll_manager()->ClearCurrentItems();
  } else {
    std::vector<phonehub::CameraRollItem> items;
    // Create items in descending key order
    for (int i = *number_of_thumbnails; i > 0; --i) {
      ash::phonehub::proto::CameraRollItemMetadata metadata;
      metadata.set_key(base::NumberToString(i));
      metadata.set_mime_type(file_type);
      metadata.set_last_modified_millis(1577865600 + i);
      metadata.set_file_size_bytes(123456);
      metadata.set_file_name("fake_file_" + base::NumberToString(i) + file_ext);

      gfx::Image thumbnail = gfx::Image::CreateFrom1xBitmap(RGB_Bitmap(
          255 - i * 192 / *number_of_thumbnails,
          63 + i * 192 / *number_of_thumbnails, 255, kCameraRollThumbnailSize));

      items.emplace_back(metadata, thumbnail);
    }
    fake_phone_hub_manager_->fake_camera_roll_manager()->SetCurrentItems(items);
  }

  absl::optional<int> download_result_as_int =
      camera_roll_dict.FindIntKey("downloadResult");
  CHECK(download_result_as_int);
  const char* download_result;
  if (*download_result_as_int == 0) {
    download_result = "Download Success";
    fake_phone_hub_manager_->fake_camera_roll_manager()
        ->SetSimulatedDownloadError(false);
  } else {
    ash::phonehub::CameraRollManager::Observer::DownloadErrorType error_type;
    switch (*download_result_as_int) {
      case 1:
        download_result = "Generic Error";
        error_type = ash::phonehub::CameraRollManager::Observer::
            DownloadErrorType::kGenericError;
        break;
      case 2:
        download_result = "Storage Error";
        error_type = ash::phonehub::CameraRollManager::Observer::
            DownloadErrorType::kInsufficientStorage;
        break;
      case 3:
      default:
        download_result = "Network Error";
        error_type = ash::phonehub::CameraRollManager::Observer::
            DownloadErrorType::kNetworkConnection;
        break;
    }
    fake_phone_hub_manager_->fake_camera_roll_manager()
        ->SetSimulatedDownloadError(true);
    fake_phone_hub_manager_->fake_camera_roll_manager()->SetSimulatedErrorType(
        error_type);
  }

  PA_LOG(VERBOSE) << "Setting fake Camera Roll to:\n  Feature enabled: "
                  << *is_camera_roll_enabled
                  << "\n  Access granted: " << *is_file_access_granted
                  << "\n  Number of thumbnails: " << *number_of_thumbnails
                  << "\n  File type: " << file_type
                  << "\n  Download result: " << download_result;
}

}  // namespace multidevice
}  // namespace chromeos
