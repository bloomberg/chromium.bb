// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_section.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/ash_interfaces.h"
#include "ash/public/cpp/night_light_controller.h"
#include "ash/public/cpp/stylus_utils.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_display_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_keyboard_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_pointer_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_power_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_stylus_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_features_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/display/display_features.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/touch_device_manager.h"
#include "ui/events/devices/device_data_manager.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetDeviceSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_KEYBOARD,
       mojom::kKeyboardSubpagePath,
       mojom::SearchResultIcon::kKeyboard,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kKeyboard}},
      {IDS_OS_SETTINGS_TAG_KEYBOARD_AUTO_REPEAT,
       mojom::kKeyboardSubpagePath,
       mojom::SearchResultIcon::kKeyboard,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kKeyboardAutoRepeat},
       {IDS_OS_SETTINGS_TAG_KEYBOARD_AUTO_REPEAT_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_POWER,
       mojom::kPowerSubpagePath,
       mojom::SearchResultIcon::kPower,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kPower}},
      {IDS_OS_SETTINGS_TAG_DISPLAY_SIZE,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisplaySize},
       {
           IDS_OS_SETTINGS_TAG_DISPLAY_SIZE_ALT1,
           IDS_OS_SETTINGS_TAG_DISPLAY_SIZE_ALT2,
           IDS_OS_SETTINGS_TAG_DISPLAY_SIZE_ALT3,
           IDS_OS_SETTINGS_TAG_DISPLAY_SIZE_ALT4,
           IDS_OS_SETTINGS_TAG_DISPLAY_SIZE_ALT5,
       }},
      {IDS_OS_SETTINGS_TAG_STORAGE,
       mojom::kStorageSubpagePath,
       mojom::SearchResultIcon::kHardDrive,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kStorage},
       {IDS_OS_SETTINGS_TAG_STORAGE_ALT1, IDS_OS_SETTINGS_TAG_STORAGE_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_DISPLAY_NIGHT_LIGHT,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kNightLight},
       {IDS_OS_SETTINGS_TAG_DISPLAY_NIGHT_LIGHT_ALT1,
        IDS_OS_SETTINGS_TAG_DISPLAY_NIGHT_LIGHT_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_DISPLAY,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kDisplay},
       {IDS_OS_SETTINGS_TAG_DISPLAY_ALT1, IDS_OS_SETTINGS_TAG_DISPLAY_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_KEYBOARD_SHORTCUTS,
       mojom::kKeyboardSubpagePath,
       mojom::SearchResultIcon::kKeyboard,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kKeyboardShortcuts}},
      {IDS_OS_SETTINGS_TAG_DEVICE,
       mojom::kDeviceSectionPath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kDevice}},
      {IDS_OS_SETTINGS_TAG_KEYBOARD_FUNCTION_KEYS,
       mojom::kKeyboardSubpagePath,
       mojom::SearchResultIcon::kKeyboard,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kKeyboardFunctionKeys}},
      {IDS_OS_SETTINGS_TAG_POWER_IDLE_WHILE_CHARGING,
       mojom::kPowerSubpagePath,
       mojom::SearchResultIcon::kPower,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPowerIdleBehaviorWhileCharging},
       {IDS_OS_SETTINGS_TAG_POWER_IDLE_WHILE_CHARGING_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_POWER_IDLE_WHILE_ON_BATTERY,
       mojom::kPowerSubpagePath,
       mojom::SearchResultIcon::kPower,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPowerIdleBehaviorWhileOnBattery},
       {IDS_OS_SETTINGS_TAG_POWER_IDLE_WHILE_ON_BATTERY_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetTouchpadSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_TOUCHPAD_SPEED,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTouchpadSpeed}},
      {IDS_OS_SETTINGS_TAG_TOUCHPAD_TAP_DRAGGING,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTouchpadTapDragging}},
      {IDS_OS_SETTINGS_TAG_TOUCHPAD_TAP_TO_CLICK,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTouchpadTapToClick}},
      {IDS_OS_SETTINGS_TAG_TOUCHPAD,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kPointers},
       {IDS_OS_SETTINGS_TAG_TOUCHPAD_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_TOUCHPAD_REVERSE_SCROLLING,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTouchpadReverseScrolling}},
      {IDS_OS_SETTINGS_TAG_TOUCHPAD_ACCELERATION,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTouchpadAcceleration}},
  });
  return *tags;
}

const std::vector<SearchConcept>&
GetTouchpadScrollAccelerationSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_TOUCHPAD_SCROLL_ACCELERATION,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTouchpadScrollAcceleration}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetTouchpadHapticFeedback() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_TOUCHPAD_HAPTIC_FEEDBACK,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTouchpadHapticFeedback}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetTouchpadHapticClickSensitivity() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_TOUCHPAD_HAPTIC_CLICK_SENSITIVITY,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTouchpadHapticClickSensitivity}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetMouseScrollAccelerationSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_MOUSE_SCROLL_ACCELERATION,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kMouse,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMouseScrollAcceleration}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetMouseSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_MOUSE_ACCELERATION,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kMouse,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMouseAcceleration}},
      {IDS_OS_SETTINGS_TAG_MOUSE_SWAP_BUTTON,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kMouse,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMouseSwapPrimaryButtons}},
      {IDS_OS_SETTINGS_TAG_MOUSE_SPEED,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kMouse,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMouseSpeed}},
      {IDS_OS_SETTINGS_TAG_MOUSE_REVERSE_SCROLLING,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kMouse,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMouseReverseScrolling}},
      {IDS_OS_SETTINGS_TAG_MOUSE,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kMouse,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kPointers}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetPointingStickSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_POINTING_STICK_PRIMARY_BUTTON,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPointingStickSwapPrimaryButtons}},
      {IDS_OS_SETTINGS_TAG_POINTING_STICK_ACCELERATION,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPointingStickAcceleration}},
      {IDS_OS_SETTINGS_TAG_POINTING_STICK_SPEED,
       mojom::kPointersSubpagePath,
       mojom::SearchResultIcon::kLaptop,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPointingStickSpeed}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetStylusSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_STYLUS_NOTE_APP,
       mojom::kStylusSubpagePath,
       mojom::SearchResultIcon::kStylus,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kStylusNoteTakingApp},
       {IDS_OS_SETTINGS_TAG_STYLUS_NOTE_APP_ALT1,
        IDS_OS_SETTINGS_TAG_STYLUS_NOTE_APP_ALT2, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_STYLUS_LOCK_SCREEN_LATEST_NOTE,
       mojom::kStylusSubpagePath,
       mojom::SearchResultIcon::kStylus,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kStylusLatestNoteOnLockScreen}},
      {IDS_OS_SETTINGS_TAG_STYLUS_LOCK_SCREEN_NOTES,
       mojom::kStylusSubpagePath,
       mojom::SearchResultIcon::kStylus,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kStylusNoteTakingFromLockScreen}},
      {IDS_OS_SETTINGS_TAG_STYLUS_SHELF_TOOLS,
       mojom::kStylusSubpagePath,
       mojom::SearchResultIcon::kStylus,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kStylusToolsInShelf},
       {IDS_OS_SETTINGS_TAG_STYLUS_SHELF_TOOLS_ALT1,
        IDS_OS_SETTINGS_TAG_STYLUS_SHELF_TOOLS_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_STYLUS,
       mojom::kStylusSubpagePath,
       mojom::SearchResultIcon::kStylus,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kStylus}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayArrangementSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DISPLAY_ARRANGEMENT,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisplayArrangement},
       {IDS_OS_SETTINGS_TAG_DISPLAY_ARRANGEMENT_ALT1,
        IDS_OS_SETTINGS_TAG_DISPLAY_ARRANGEMENT_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayMirrorSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_MIRRORING,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisplayMirroring}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayUnifiedDesktopSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_UNIFIED_DESKTOP,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAllowWindowsToSpanDisplays}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayExternalSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DISPLAY_RESOLUTION,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisplayResolution},
       {IDS_OS_SETTINGS_TAG_DISPLAY_RESOLUTION_ALT1,
        IDS_OS_SETTINGS_TAG_DISPLAY_RESOLUTION_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_DISPLAY_OVERSCAN,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisplayOverscan}},
  });
  return *tags;
}

const std::vector<SearchConcept>&
GetDisplayExternalWithRefreshSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DISPLAY_REFRESH_RATE,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisplayRefreshRate},
       {IDS_OS_SETTINGS_TAG_DISPLAY_REFRESH_RATE_ALT1,
        IDS_OS_SETTINGS_TAG_DISPLAY_REFRESH_RATE_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayOrientationSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DISPLAY_ORIENTATION,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisplayOrientation},
       {IDS_OS_SETTINGS_TAG_DISPLAY_ORIENTATION_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayAmbientSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DISPLAY_AMBIENT_COLORS,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAmbientColors}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayTouchCalibrationSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DISPLAY_TOUCHSCREEN_CALIBRATION,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTouchscreenCalibration}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetDisplayNightLightOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_NIGHT_LIGHT_COLOR_TEMPERATURE,
       mojom::kDisplaySubpagePath,
       mojom::SearchResultIcon::kDisplay,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kNightLightColorTemperature}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetExternalStorageSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_EXTERNAL_STORAGE,
       mojom::kExternalStorageSubpagePath,
       mojom::SearchResultIcon::kHardDrive,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kExternalStorage}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetPowerWithBatterySearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_POWER_SOURCE,
       mojom::kPowerSubpagePath,
       mojom::SearchResultIcon::kPower,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPowerSource},
       {IDS_OS_SETTINGS_TAG_POWER_SOURCE_ALT1,
        IDS_OS_SETTINGS_TAG_POWER_SOURCE_ALT2, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetPowerWithLaptopLidSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_POWER_SLEEP_COVER_CLOSED,
       mojom::kPowerSubpagePath,
       mojom::SearchResultIcon::kPower,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSleepWhenLaptopLidClosed},
       {IDS_OS_SETTINGS_TAG_POWER_SLEEP_COVER_CLOSED_ALT1,
        IDS_OS_SETTINGS_TAG_POWER_SLEEP_COVER_CLOSED_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

bool AreScrollSettingsAllowed() {
  return base::FeatureList::IsEnabled(
      ::chromeos::features::kAllowScrollSettings);
}

bool IsUnifiedDesktopAvailable() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableUnifiedDesktop);
}

bool DoesDeviceSupportAmbientColor() {
  return ash::features::IsAllowAmbientEQEnabled();
}

bool IsTouchCalibrationAvailable() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableTouchCalibrationSetting) &&
         display::HasExternalTouchscreenDevice();
}

bool IsListAllDisplayModesEnabled() {
  return display::features::IsListAllDisplayModesEnabled();
}

void AddDeviceKeyboardStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString keyboard_strings[] = {
      {"keyboardTitle", IDS_SETTINGS_KEYBOARD_TITLE},
      {"keyboardKeyCtrl", IDS_SETTINGS_KEYBOARD_KEY_LEFT_CTRL},
      {"keyboardKeyAlt", IDS_SETTINGS_KEYBOARD_KEY_LEFT_ALT},
      {"keyboardKeyCapsLock", IDS_SETTINGS_KEYBOARD_KEY_CAPS_LOCK},
      {"keyboardKeyCommand", IDS_SETTINGS_KEYBOARD_KEY_COMMAND},
      {"keyboardKeyDiamond", IDS_SETTINGS_KEYBOARD_KEY_DIAMOND},
      {"keyboardKeyEscape", IDS_SETTINGS_KEYBOARD_KEY_ESCAPE},
      {"keyboardKeyBackspace", IDS_SETTINGS_KEYBOARD_KEY_BACKSPACE},
      {"keyboardKeyAssistant", IDS_SETTINGS_KEYBOARD_KEY_ASSISTANT},
      {"keyboardKeyDisabled", IDS_SETTINGS_KEYBOARD_KEY_DISABLED},
      {"keyboardKeyExternalCommand",
       IDS_SETTINGS_KEYBOARD_KEY_EXTERNAL_COMMAND},
      {"keyboardKeyExternalMeta", IDS_SETTINGS_KEYBOARD_KEY_EXTERNAL_META},
      {"keyboardKeyMeta", IDS_SETTINGS_KEYBOARD_KEY_META},
      {"keyboardSendFunctionKeys", IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS},
      {"keyboardEnableAutoRepeat", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_ENABLE},
      {"keyRepeatDelay", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY},
      {"keyRepeatDelayLong", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY_LONG},
      {"keyRepeatDelayShort", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY_SHORT},
      {"keyRepeatRate", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_RATE},
      {"keyRepeatRateSlow", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_RATE_SLOW},
      {"keyRepeatRateFast", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_FAST},
      {"showKeyboardShortcutViewer",
       IDS_SETTINGS_KEYBOARD_SHOW_SHORTCUT_VIEWER},
      // TODO(crbug.com/1097328): Remove this string, as it is unused.
      {"keyboardShowLanguageAndInput",
       IDS_SETTINGS_KEYBOARD_SHOW_LANGUAGE_AND_INPUT},
      {"keyboardShowInputSettings", IDS_SETTINGS_KEYBOARD_SHOW_INPUT_SETTINGS},
  };
  html_source->AddLocalizedStrings(keyboard_strings);

  html_source->AddLocalizedString("keyboardKeySearch",
                                  ui::DeviceUsesKeyboardLayout2()
                                      ? IDS_SETTINGS_KEYBOARD_KEY_LAUNCHER
                                      : IDS_SETTINGS_KEYBOARD_KEY_SEARCH);
  html_source->AddLocalizedString(
      "keyboardSendFunctionKeysDescription",
      ui::DeviceUsesKeyboardLayout2()
          ? IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS_LAYOUT2_DESCRIPTION
          : IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS_DESCRIPTION);
}

void AddDeviceStylusStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kStylusStrings[] = {
      {"stylusTitle", IDS_SETTINGS_STYLUS_TITLE},
      {"stylusEnableStylusTools", IDS_SETTINGS_STYLUS_ENABLE_STYLUS_TOOLS},
      {"stylusAutoOpenStylusTools", IDS_SETTINGS_STYLUS_AUTO_OPEN_STYLUS_TOOLS},
      {"stylusFindMoreAppsPrimary", IDS_SETTINGS_STYLUS_FIND_MORE_APPS_PRIMARY},
      {"stylusFindMoreAppsSecondary",
       IDS_SETTINGS_STYLUS_FIND_MORE_APPS_SECONDARY},
      {"stylusNoteTakingApp", IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_LABEL},
      {"stylusNoteTakingAppEnabledOnLockScreen",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_LOCK_SCREEN_CHECKBOX},
      {"stylusNoteTakingAppKeepsLastNoteOnLockScreen",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_KEEP_LATEST_NOTE},
      {"stylusNoteTakingAppLockScreenSettingsHeader",
       IDS_SETTINGS_STYLUS_LOCK_SCREEN_NOTES_TITLE},
      {"stylusNoteTakingAppNoneAvailable",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_NONE_AVAILABLE},
      {"stylusNoteTakingAppWaitingForAndroid",
       IDS_SETTINGS_STYLUS_NOTE_TAKING_APP_WAITING_FOR_ANDROID}};
  html_source->AddLocalizedStrings(kStylusStrings);

  html_source->AddBoolean("hasInternalStylus",
                          ash::stylus_utils::HasInternalStylus());
}

void AddDeviceDisplayStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kDisplayStrings[] = {
      {"displayTitle", IDS_SETTINGS_DISPLAY_TITLE},
      {"displayArrangementTitle", IDS_SETTINGS_DISPLAY_ARRANGEMENT_TITLE},
      {"displayMirror", IDS_SETTINGS_DISPLAY_MIRROR},
      {"displayMirrorDisplayName", IDS_SETTINGS_DISPLAY_MIRROR_DISPLAY_NAME},
      {"displayAmbientColorTitle", IDS_SETTINGS_DISPLAY_AMBIENT_COLOR_TITLE},
      {"displayAmbientColorSubtitle",
       IDS_SETTINGS_DISPLAY_AMBIENT_COLOR_SUBTITLE},
      {"displayNightLightLabel", IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_LABEL},
      {"displayNightLightOnAtSunset",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_ON_AT_SUNSET},
      {"displayNightLightOffAtSunrise",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_OFF_AT_SUNRISE},
      {"displayNightLightScheduleCustom",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_CUSTOM},
      {"displayNightLightScheduleLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_LABEL},
      {"displayNightLightScheduleNever",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_NEVER},
      {"displayNightLightScheduleSunsetToSunRise",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_SCHEDULE_SUNSET_TO_SUNRISE},
      {"displayNightLightText", IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEXT},
      {"displayNightLightTemperatureLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEMPERATURE_LABEL},
      {"displayNightLightTempSliderMaxLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEMP_SLIDER_MAX_LABEL},
      {"displayNightLightTempSliderMinLabel",
       IDS_SETTINGS_DISPLAY_NIGHT_LIGHT_TEMP_SLIDER_MIN_LABEL},
      {"displayUnifiedDesktop", IDS_SETTINGS_DISPLAY_UNIFIED_DESKTOP},
      {"displayUnifiedDesktopOn", IDS_SETTINGS_DISPLAY_UNIFIED_DESKTOP_ON},
      {"displayUnifiedDesktopOff", IDS_SETTINGS_DISPLAY_UNIFIED_DESKTOP_OFF},
      {"displayResolutionTitle", IDS_SETTINGS_DISPLAY_RESOLUTION_TITLE},
      {"displayResolutionText", IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT},
      {"displayResolutionTextBest", IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT_BEST},
      {"displayResolutionTextNative",
       IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT_NATIVE},
      {"displayResolutionSublabel", IDS_SETTINGS_DISPLAY_RESOLUTION_SUBLABEL},
      {"displayResolutionMenuItem", IDS_SETTINGS_DISPLAY_RESOLUTION_MENU_ITEM},
      {"displayResolutionOnlyMenuItem",
       IDS_SETTINGS_DISPLAY_RESOLUTION_ONLY_MENU_ITEM},
      {"displayResolutionInterlacedMenuItem",
       IDS_SETTINGS_DISPLAY_RESOLUTION_INTERLACED_MENU_ITEM},
      {"displayRefreshRateTitle", IDS_SETTINGS_DISPLAY_REFRESH_RATE_TITLE},
      {"displayRefreshRateSublabel",
       IDS_SETTINGS_DISPLAY_REFRESH_RATE_SUBLABEL},
      {"displayRefreshRateMenuItem",
       IDS_SETTINGS_DISPLAY_REFRESH_RATE_MENU_ITEM},
      {"displayRefreshRateInterlacedMenuItem",
       IDS_SETTINGS_DISPLAY_REFRESH_RATE_INTERLACED_MENU_ITEM},
      {"displayZoomTitle", IDS_SETTINGS_DISPLAY_ZOOM_TITLE},
      {"displayZoomSublabel", IDS_SETTINGS_DISPLAY_ZOOM_SUBLABEL},
      {"displayZoomValue", IDS_SETTINGS_DISPLAY_ZOOM_VALUE},
      {"displayZoomLogicalResolutionText",
       IDS_SETTINGS_DISPLAY_ZOOM_LOGICAL_RESOLUTION_TEXT},
      {"displayZoomNativeLogicalResolutionNativeText",
       IDS_SETTINGS_DISPLAY_ZOOM_LOGICAL_RESOLUTION_NATIVE_TEXT},
      {"displayZoomLogicalResolutionDefaultText",
       IDS_SETTINGS_DISPLAY_ZOOM_LOGICAL_RESOLUTION_DEFAULT_TEXT},
      {"displaySizeSliderMinLabel", IDS_SETTINGS_DISPLAY_ZOOM_SLIDER_MINIMUM},
      {"displaySizeSliderMaxLabel", IDS_SETTINGS_DISPLAY_ZOOM_SLIDER_MAXIMUM},
      {"displayScreenTitle", IDS_SETTINGS_DISPLAY_SCREEN},
      {"displayScreenExtended", IDS_SETTINGS_DISPLAY_SCREEN_EXTENDED},
      {"displayScreenPrimary", IDS_SETTINGS_DISPLAY_SCREEN_PRIMARY},
      {"displayOrientation", IDS_SETTINGS_DISPLAY_ORIENTATION},
      {"displayOrientationStandard", IDS_SETTINGS_DISPLAY_ORIENTATION_STANDARD},
      {"displayOrientationAutoRotate",
       IDS_SETTINGS_DISPLAY_ORIENTATION_AUTO_ROTATE},
      {"displayOverscanPageText", IDS_SETTINGS_DISPLAY_OVERSCAN_TEXT},
      {"displayOverscanPageTitle", IDS_SETTINGS_DISPLAY_OVERSCAN_TITLE},
      {"displayOverscanSubtitle", IDS_SETTINGS_DISPLAY_OVERSCAN_SUBTITLE},
      {"displayOverscanInstructions",
       IDS_SETTINGS_DISPLAY_OVERSCAN_INSTRUCTIONS},
      {"displayOverscanResize", IDS_SETTINGS_DISPLAY_OVERSCAN_RESIZE},
      {"displayOverscanPosition", IDS_SETTINGS_DISPLAY_OVERSCAN_POSITION},
      {"displayOverscanReset", IDS_SETTINGS_DISPLAY_OVERSCAN_RESET},
      {"displayTouchCalibrationTitle",
       IDS_SETTINGS_DISPLAY_TOUCH_CALIBRATION_TITLE},
      {"displayTouchCalibrationText",
       IDS_SETTINGS_DISPLAY_TOUCH_CALIBRATION_TEXT}};
  html_source->AddLocalizedStrings(kDisplayStrings);

  html_source->AddLocalizedString(
      "displayArrangementText",
      base::FeatureList::IsEnabled(
          ash::features::kKeyboardBasedDisplayArrangementInSettings)
          ? IDS_SETTINGS_DISPLAY_ARRANGEMENT_WITH_KEYBOARD_TEXT
          : IDS_SETTINGS_DISPLAY_ARRANGEMENT_TEXT);

  html_source->AddBoolean("unifiedDesktopAvailable",
                          IsUnifiedDesktopAvailable());

  html_source->AddBoolean("listAllDisplayModes",
                          IsListAllDisplayModesEnabled());

  html_source->AddBoolean("deviceSupportsAmbientColor",
                          DoesDeviceSupportAmbientColor());

  html_source->AddBoolean("enableTouchCalibrationSetting",
                          IsTouchCalibrationAvailable());

  html_source->AddString("invalidDisplayId",
                         base::NumberToString(display::kInvalidDisplayId));

  html_source->AddBoolean(
      "allowDisplayAlignmentApi",
      base::FeatureList::IsEnabled(ash::features::kDisplayAlignAssist));

  html_source->AddBoolean(
      "allowKeyboardBasedDisplayArrangementInSettings",
      base::FeatureList::IsEnabled(
          ash::features::kKeyboardBasedDisplayArrangementInSettings));
}

void AddDeviceStorageStrings(content::WebUIDataSource* html_source,
                             bool is_external_storage_page_available) {
  static constexpr webui::LocalizedString kStorageStrings[] = {
      {"storageTitle", IDS_SETTINGS_STORAGE_TITLE},
      {"storageItemInUse", IDS_SETTINGS_STORAGE_ITEM_IN_USE},
      {"storageItemAvailable", IDS_SETTINGS_STORAGE_ITEM_AVAILABLE},
      {"storageItemSystem", IDS_SETTINGS_STORAGE_ITEM_SYSTEM},
      {"storageItemMyFiles", IDS_SETTINGS_STORAGE_ITEM_MY_FILES},
      {"storageItemBrowsingData", IDS_SETTINGS_STORAGE_ITEM_BROWSING_DATA},
      {"storageItemApps", IDS_SETTINGS_STORAGE_ITEM_APPS},
      {"storageItemCrostini", IDS_SETTINGS_STORAGE_ITEM_CROSTINI},
      {"storageItemOtherUsers", IDS_SETTINGS_STORAGE_ITEM_OTHER_USERS},
      {"storageSizeComputing", IDS_SETTINGS_STORAGE_SIZE_CALCULATING},
      {"storageSizeUnknown", IDS_SETTINGS_STORAGE_SIZE_UNKNOWN},
      {"storageSpaceLowMessageTitle",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_TITLE},
      {"storageSpaceLowMessageLine1",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_LINE_1},
      {"storageSpaceLowMessageLine2",
       IDS_SETTINGS_STORAGE_SPACE_LOW_MESSAGE_LINE_2},
      {"storageSpaceCriticallyLowMessageTitle",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_TITLE},
      {"storageSpaceCriticallyLowMessageLine1",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_LINE_1},
      {"storageSpaceCriticallyLowMessageLine2",
       IDS_SETTINGS_STORAGE_SPACE_CRITICALLY_LOW_MESSAGE_LINE_2},
      {"storageExternal", IDS_SETTINGS_STORAGE_EXTERNAL},
      {"storageExternalStorageEmptyListHeader",
       IDS_SETTINGS_STORAGE_EXTERNAL_STORAGE_EMPTY_LIST_HEADER},
      {"storageExternalStorageListHeader",
       IDS_SETTINGS_STORAGE_EXTERNAL_STORAGE_LIST_HEADER},
      {"storageOverviewAriaLabel", IDS_SETTINGS_STORAGE_OVERVIEW_ARIA_LABEL}};
  html_source->AddLocalizedStrings(kStorageStrings);

  html_source->AddBoolean("androidEnabled", is_external_storage_page_available);

  html_source->AddString(
      "storageAndroidAppsExternalDrivesNote",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_STORAGE_ANDROID_APPS_ACCESS_EXTERNAL_DRIVES_NOTE,
          base::ASCIIToUTF16(chrome::kArcExternalStorageLearnMoreURL)));
}

void AddDevicePowerStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kPowerStrings[] = {
      {"powerTitle", IDS_SETTINGS_POWER_TITLE},
      {"powerSourceLabel", IDS_SETTINGS_POWER_SOURCE_LABEL},
      {"powerSourceBattery", IDS_SETTINGS_POWER_SOURCE_BATTERY},
      {"powerSourceAcAdapter", IDS_SETTINGS_POWER_SOURCE_AC_ADAPTER},
      {"powerSourceLowPowerCharger",
       IDS_SETTINGS_POWER_SOURCE_LOW_POWER_CHARGER},
      {"calculatingPower", IDS_SETTINGS_POWER_SOURCE_CALCULATING},
      {"powerIdleLabel", IDS_SETTINGS_POWER_IDLE_LABEL},
      {"powerIdleWhileChargingLabel",
       IDS_SETTINGS_POWER_IDLE_WHILE_CHARGING_LABEL},
      {"powerIdleWhileChargingAriaLabel",
       IDS_SETTINGS_POWER_IDLE_WHILE_CHARGING_ARIA_LABEL},
      {"powerIdleWhileOnBatteryLabel",
       IDS_SETTINGS_POWER_IDLE_WHILE_ON_BATTERY_LABEL},
      {"powerIdleWhileOnBatteryAriaLabel",
       IDS_SETTINGS_POWER_IDLE_WHILE_ON_BATTERY_ARIA_LABEL},
      {"powerIdleDisplayOffSleep", IDS_SETTINGS_POWER_IDLE_DISPLAY_OFF_SLEEP},
      {"powerIdleDisplayOff", IDS_SETTINGS_POWER_IDLE_DISPLAY_OFF},
      {"powerIdleDisplayOn", IDS_SETTINGS_POWER_IDLE_DISPLAY_ON},
      {"powerIdleDisplayShutDown", IDS_SETTINGS_POWER_IDLE_SHUT_DOWN},
      {"powerIdleDisplayStopSession", IDS_SETTINGS_POWER_IDLE_STOP_SESSION},
      {"powerLidSleepLabel", IDS_SETTINGS_POWER_LID_CLOSED_SLEEP_LABEL},
      {"powerLidSignOutLabel", IDS_SETTINGS_POWER_LID_CLOSED_SIGN_OUT_LABEL},
      {"powerLidShutDownLabel", IDS_SETTINGS_POWER_LID_CLOSED_SHUT_DOWN_LABEL},
  };
  html_source->AddLocalizedStrings(kPowerStrings);
}

// Mirrors enum of the same name in enums.xml.
enum class TouchpadSensitivity {
  kNONE = 0,
  kSlowest = 1,
  kSlow = 2,
  kMedium = 3,
  kFast = 4,
  kFastest = 5,
  kMaxValue = kFastest,
};

}  // namespace

DeviceSection::DeviceSection(Profile* profile,
                             SearchTagRegistry* search_tag_registry,
                             PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetDeviceSearchConcepts());

  if (features::ShouldShowExternalStorageSettings(profile))
    updater.AddSearchTags(GetExternalStorageSearchConcepts());

  PowerManagerClient* power_manager_client = PowerManagerClient::Get();
  if (power_manager_client) {
    power_manager_client->AddObserver(this);

    const absl::optional<power_manager::PowerSupplyProperties>& last_status =
        power_manager_client->GetLastStatus();
    if (last_status)
      PowerChanged(*last_status);

    // Determine whether to show laptop lid power settings.
    power_manager_client->GetSwitchStates(base::BindOnce(
        &DeviceSection::OnGotSwitchStates, weak_ptr_factory_.GetWeakPtr()));
  }

  // Keyboard/mouse search tags are added/removed dynamically.
  pointer_device_observer_.Init();
  pointer_device_observer_.AddObserver(this);
  pointer_device_observer_.CheckDevices();

  // Stylus search tags are added/removed dynamically.
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
  UpdateStylusSearchTags();

  // Display search tags are added/removed dynamically.
  ash::BindCrosDisplayConfigController(
      cros_display_config_.BindNewPipeAndPassReceiver());
  mojo::PendingAssociatedRemote<ash::mojom::CrosDisplayConfigObserver> observer;
  cros_display_config_observer_receiver_.Bind(
      observer.InitWithNewEndpointAndPassReceiver());
  cros_display_config_->AddObserver(std::move(observer));
  OnDisplayConfigChanged();

  // Night Light settings are added/removed dynamically.
  ash::NightLightController* night_light_controller =
      ash::NightLightController::GetInstance();
  if (night_light_controller) {
    ash::NightLightController::GetInstance()->AddObserver(this);
    OnNightLightEnabledChanged(
        ash::NightLightController::GetInstance()->GetEnabled());
  }
}

DeviceSection::~DeviceSection() {
  pointer_device_observer_.RemoveObserver(this);
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);

  PowerManagerClient* power_manager_client = PowerManagerClient::Get();
  if (power_manager_client)
    power_manager_client->RemoveObserver(this);

  ash::NightLightController* night_light_controller =
      ash::NightLightController::GetInstance();
  if (night_light_controller)
    night_light_controller->RemoveObserver(this);
}

void DeviceSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kDeviceStrings[] = {
      {"devicePageTitle", IDS_SETTINGS_DEVICE_TITLE},
      {"touchPadScrollLabel", IDS_OS_SETTINGS_TOUCHPAD_REVERSE_SCROLL_LABEL},
  };
  html_source->AddLocalizedStrings(kDeviceStrings);

  html_source->AddBoolean("isDemoSession", DemoSession::IsDeviceInDemoMode());

  AddDevicePointersStrings(html_source);
  AddDeviceKeyboardStrings(html_source);
  AddDeviceStylusStrings(html_source);
  AddDeviceDisplayStrings(html_source);
  AddDeviceStorageStrings(
      html_source, features::ShouldShowExternalStorageSettings(profile()));
  AddDevicePowerStrings(html_source);
}

void DeviceSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::DisplayHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::KeyboardHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::PointerHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::PowerHandler>(pref_service_));
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::StylusHandler>());
}

int DeviceSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_DEVICE_TITLE;
}

mojom::Section DeviceSection::GetSection() const {
  return mojom::Section::kDevice;
}

mojom::SearchResultIcon DeviceSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kLaptop;
}

std::string DeviceSection::GetSectionPath() const {
  return mojom::kDeviceSectionPath;
}

bool DeviceSection::LogMetric(mojom::Setting setting,
                              base::Value& value) const {
  switch (setting) {
    case mojom::Setting::kTouchpadSpeed:
      base::UmaHistogramEnumeration(
          "ChromeOS.Settings.Device.TouchpadSpeedValue",
          static_cast<TouchpadSensitivity>(value.GetInt()));
      return true;

    case mojom::Setting::kKeyboardFunctionKeys:
      base::UmaHistogramBoolean("ChromeOS.Settings.Device.KeyboardFunctionKeys",
                                value.GetBool());
      return true;

    default:
      return false;
  }
}

void DeviceSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  // Pointers.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_MOUSE_AND_TOUCHPAD_TITLE, mojom::Subpage::kPointers,
      mojom::SearchResultIcon::kMouse, mojom::SearchResultDefaultRank::kMedium,
      mojom::kPointersSubpagePath);
  static constexpr mojom::Setting kPointersSettings[] = {
      mojom::Setting::kTouchpadTapToClick,
      mojom::Setting::kTouchpadTapDragging,
      mojom::Setting::kTouchpadReverseScrolling,
      mojom::Setting::kTouchpadAcceleration,
      mojom::Setting::kTouchpadScrollAcceleration,
      mojom::Setting::kTouchpadSpeed,
      mojom::Setting::kTouchpadHapticFeedback,
      mojom::Setting::kTouchpadHapticClickSensitivity,
      mojom::Setting::kPointingStickSwapPrimaryButtons,
      mojom::Setting::kPointingStickSpeed,
      mojom::Setting::kPointingStickAcceleration,
      mojom::Setting::kMouseSwapPrimaryButtons,
      mojom::Setting::kMouseReverseScrolling,
      mojom::Setting::kMouseAcceleration,
      mojom::Setting::kMouseScrollAcceleration,
      mojom::Setting::kMouseSpeed,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kPointers, kPointersSettings,
                            generator);

  // Keyboard.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_KEYBOARD_TITLE, mojom::Subpage::kKeyboard,
      mojom::SearchResultIcon::kKeyboard,
      mojom::SearchResultDefaultRank::kMedium, mojom::kKeyboardSubpagePath);
  static constexpr mojom::Setting kKeyboardSettings[] = {
      mojom::Setting::kKeyboardFunctionKeys,
      mojom::Setting::kKeyboardAutoRepeat,
      mojom::Setting::kKeyboardShortcuts,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kKeyboard, kKeyboardSettings,
                            generator);

  // Stylus.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_STYLUS_TITLE, mojom::Subpage::kStylus,
      mojom::SearchResultIcon::kStylus, mojom::SearchResultDefaultRank::kMedium,
      mojom::kStylusSubpagePath);
  static constexpr mojom::Setting kStylusSettings[] = {
      mojom::Setting::kStylusToolsInShelf,
      mojom::Setting::kStylusNoteTakingApp,
      mojom::Setting::kStylusNoteTakingFromLockScreen,
      mojom::Setting::kStylusLatestNoteOnLockScreen,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kStylus, kStylusSettings,
                            generator);

  // Display.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_DISPLAY_TITLE, mojom::Subpage::kDisplay,
      mojom::SearchResultIcon::kDisplay,
      mojom::SearchResultDefaultRank::kMedium, mojom::kDisplaySubpagePath);
  static constexpr mojom::Setting kDisplaySettings[] = {
      mojom::Setting::kDisplaySize,
      mojom::Setting::kNightLight,
      mojom::Setting::kDisplayOrientation,
      mojom::Setting::kDisplayArrangement,
      mojom::Setting::kDisplayResolution,
      mojom::Setting::kDisplayRefreshRate,
      mojom::Setting::kDisplayMirroring,
      mojom::Setting::kAllowWindowsToSpanDisplays,
      mojom::Setting::kAmbientColors,
      mojom::Setting::kTouchscreenCalibration,
      mojom::Setting::kNightLightColorTemperature,
      mojom::Setting::kDisplayOverscan,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kDisplay, kDisplaySettings,
                            generator);

  // Storage.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_STORAGE_TITLE, mojom::Subpage::kStorage,
      mojom::SearchResultIcon::kHardDrive,
      mojom::SearchResultDefaultRank::kMedium, mojom::kStorageSubpagePath);
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_STORAGE_EXTERNAL, mojom::Subpage::kExternalStorage,
      mojom::Subpage::kStorage, mojom::SearchResultIcon::kHardDrive,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kExternalStorageSubpagePath);

  // Power.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_POWER_TITLE, mojom::Subpage::kPower,
      mojom::SearchResultIcon::kPower, mojom::SearchResultDefaultRank::kMedium,
      mojom::kPowerSubpagePath);
  static constexpr mojom::Setting kPowerSettings[] = {
      mojom::Setting::kPowerIdleBehaviorWhileCharging,
      mojom::Setting::kPowerIdleBehaviorWhileOnBattery,
      mojom::Setting::kPowerSource,
      mojom::Setting::kSleepWhenLaptopLidClosed,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kPower, kPowerSettings, generator);
}

void DeviceSection::TouchpadExists(bool exists) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetTouchpadSearchConcepts());
  updater.RemoveSearchTags(GetTouchpadScrollAccelerationSearchConcepts());

  if (exists) {
    updater.AddSearchTags(GetTouchpadSearchConcepts());
    if (base::FeatureList::IsEnabled(chromeos::features::kAllowScrollSettings))
      updater.AddSearchTags(GetTouchpadScrollAccelerationSearchConcepts());
  }
}

void DeviceSection::HapticTouchpadExists(bool exists) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetTouchpadHapticFeedback());
  updater.RemoveSearchTags(GetTouchpadHapticClickSensitivity());

  if (!exists) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          ::features::kAllowDisableTouchpadHapticFeedback)) {
    updater.AddSearchTags(GetTouchpadHapticFeedback());
  }
  if (base::FeatureList::IsEnabled(
          ::features::kAllowTouchpadHapticClickSettings)) {
    updater.AddSearchTags(GetTouchpadHapticClickSensitivity());
  }
}

void DeviceSection::MouseExists(bool exists) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetMouseSearchConcepts());
  updater.RemoveSearchTags(GetMouseScrollAccelerationSearchConcepts());

  if (exists) {
    updater.AddSearchTags(GetMouseSearchConcepts());
    if (AreScrollSettingsAllowed())
      updater.AddSearchTags(GetMouseScrollAccelerationSearchConcepts());
  }
}

void DeviceSection::PointingStickExists(bool exists) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (exists)
    updater.AddSearchTags(GetPointingStickSearchConcepts());
  else
    updater.RemoveSearchTags(GetPointingStickSearchConcepts());
}

void DeviceSection::OnDeviceListsComplete() {
  UpdateStylusSearchTags();
}

void DeviceSection::OnNightLightEnabledChanged(bool enabled) {
  OnDisplayConfigChanged();
}

void DeviceSection::OnDisplayConfigChanged() {
  cros_display_config_->GetDisplayUnitInfoList(
      /*single_unified=*/true,
      base::BindOnce(&DeviceSection::OnGetDisplayUnitInfoList,
                     base::Unretained(this)));
}

void DeviceSection::PowerChanged(
    const power_manager::PowerSupplyProperties& properties) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (properties.battery_state() !=
      power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT) {
    updater.AddSearchTags(GetPowerWithBatterySearchConcepts());
  }
}

void DeviceSection::OnGetDisplayUnitInfoList(
    std::vector<ash::mojom::DisplayUnitInfoPtr> display_unit_info_list) {
  cros_display_config_->GetDisplayLayoutInfo(base::BindOnce(
      &DeviceSection::OnGetDisplayLayoutInfo, base::Unretained(this),
      std::move(display_unit_info_list)));
}

void DeviceSection::OnGetDisplayLayoutInfo(
    std::vector<ash::mojom::DisplayUnitInfoPtr> display_unit_info_list,
    ash::mojom::DisplayLayoutInfoPtr display_layout_info) {
  bool has_multiple_displays = display_unit_info_list.size() > 1u;

  // Mirroring mode is active if there's at least one display and if there's a
  // mirror source ID.
  bool is_mirrored = !display_unit_info_list.empty() &&
                     display_layout_info->mirror_source_id.has_value();

  bool has_internal_display = false;
  bool has_external_display = false;
  bool unified_desktop_mode = false;
  for (const auto& display_unit_info : display_unit_info_list) {
    has_internal_display |= display_unit_info->is_internal;
    has_external_display |= !display_unit_info->is_internal;

    unified_desktop_mode |= display_unit_info->is_primary &&
                            display_layout_info->layout_mode ==
                                ash::mojom::DisplayLayoutMode::kUnified;
  }

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  // Arrangement UI.
  if (has_multiple_displays || is_mirrored)
    updater.AddSearchTags(GetDisplayArrangementSearchConcepts());
  else
    updater.RemoveSearchTags(GetDisplayArrangementSearchConcepts());

  // Mirror toggle.
  if (is_mirrored || (!unified_desktop_mode && has_multiple_displays))
    updater.AddSearchTags(GetDisplayMirrorSearchConcepts());
  else
    updater.RemoveSearchTags(GetDisplayMirrorSearchConcepts());

  // Unified Desktop toggle.
  if (unified_desktop_mode ||
      (IsUnifiedDesktopAvailable() && has_multiple_displays && !is_mirrored)) {
    updater.AddSearchTags(GetDisplayUnifiedDesktopSearchConcepts());
  } else {
    updater.RemoveSearchTags(GetDisplayUnifiedDesktopSearchConcepts());
  }

  // External display settings.
  if (has_external_display)
    updater.AddSearchTags(GetDisplayExternalSearchConcepts());
  else
    updater.RemoveSearchTags(GetDisplayExternalSearchConcepts());

  // Refresh Rate dropdown.
  if (has_external_display && IsListAllDisplayModesEnabled())
    updater.AddSearchTags(GetDisplayExternalWithRefreshSearchConcepts());
  else
    updater.RemoveSearchTags(GetDisplayExternalWithRefreshSearchConcepts());

  // Orientation settings.
  if (!unified_desktop_mode)
    updater.AddSearchTags(GetDisplayOrientationSearchConcepts());
  else
    updater.RemoveSearchTags(GetDisplayOrientationSearchConcepts());

  // Ambient color settings.
  if (DoesDeviceSupportAmbientColor() && has_internal_display)
    updater.AddSearchTags(GetDisplayAmbientSearchConcepts());
  else
    updater.RemoveSearchTags(GetDisplayAmbientSearchConcepts());

  // Touch calibration settings.
  if (IsTouchCalibrationAvailable())
    updater.AddSearchTags(GetDisplayTouchCalibrationSearchConcepts());
  else
    updater.RemoveSearchTags(GetDisplayTouchCalibrationSearchConcepts());

  // Night Light on settings.
  if (ash::NightLightController::GetInstance()->GetEnabled())
    updater.AddSearchTags(GetDisplayNightLightOnSearchConcepts());
  else
    updater.RemoveSearchTags(GetDisplayNightLightOnSearchConcepts());
}

void DeviceSection::OnGotSwitchStates(
    absl::optional<PowerManagerClient::SwitchStates> result) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (result && result->lid_state != PowerManagerClient::LidState::NOT_PRESENT)
    updater.AddSearchTags(GetPowerWithLaptopLidSearchConcepts());
}

void DeviceSection::UpdateStylusSearchTags() {
  // If not yet complete, wait for OnDeviceListsComplete() callback.
  if (!ui::DeviceDataManager::GetInstance()->AreDeviceListsComplete())
    return;

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  // TODO(https://crbug.com/1071905): Only show stylus settings if a stylus has
  // been set up. HasStylusInput() will return true for any stylus-compatible
  // device, even if it doesn't have a stylus.
  if (ash::stylus_utils::HasStylusInput())
    updater.AddSearchTags(GetStylusSearchConcepts());
  else
    updater.RemoveSearchTags(GetStylusSearchConcepts());
}

void DeviceSection::AddDevicePointersStrings(
    content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kPointersStrings[] = {
      {"mouseTitle", IDS_SETTINGS_MOUSE_TITLE},
      {"pointingStickTitle", IDS_SETTINGS_POINTING_STICK_TITLE},
      {"touchpadTitle", IDS_SETTINGS_TOUCHPAD_TITLE},
      {"mouseAndTouchpadTitle", IDS_SETTINGS_MOUSE_AND_TOUCHPAD_TITLE},
      {"touchpadTapToClickEnabledLabel",
       IDS_SETTINGS_TOUCHPAD_TAP_TO_CLICK_ENABLED_LABEL},
      {"touchpadSpeed", IDS_SETTINGS_TOUCHPAD_SPEED_LABEL},
      {"pointerSlow", IDS_SETTINGS_POINTER_SPEED_SLOW_LABEL},
      {"pointerFast", IDS_SETTINGS_POINTER_SPEED_FAST_LABEL},
      {"mouseScrollSpeed", IDS_SETTINGS_MOUSE_SCROLL_SPEED_LABEL},
      {"mouseSpeed", IDS_SETTINGS_MOUSE_SPEED_LABEL},
      {"pointingStickSpeed", IDS_SETTINGS_POINTING_STICK_SPEED_LABEL},
      {"mouseSwapButtons", IDS_SETTINGS_MOUSE_SWAP_BUTTONS_LABEL},
      {"pointingStickPrimaryButton",
       IDS_SETTINGS_POINTING_STICK_PRIMARY_BUTTON_LABEL},
      {"primaryMouseButtonLeft", IDS_SETTINGS_PRIMARY_MOUSE_BUTTON_LEFT_LABEL},
      {"primaryMouseButtonRight",
       IDS_SETTINGS_PRIMARY_MOUSE_BUTTON_RIGHT_LABEL},
      {"mouseReverseScroll", IDS_SETTINGS_MOUSE_REVERSE_SCROLL_LABEL},
      {"mouseAccelerationLabel", IDS_SETTINGS_MOUSE_ACCELERATION_LABEL},
      {"mouseScrollAccelerationLabel",
       IDS_SETTINGS_MOUSE_SCROLL_ACCELERATION_LABEL},
      {"pointingStickAccelerationLabel",
       IDS_SETTINGS_POINTING_STICK_ACCELERATION_LABEL},
      {"touchpadAccelerationLabel", IDS_SETTINGS_TOUCHPAD_ACCELERATION_LABEL},
      {"touchpadHapticClickSensitivityLabel",
       IDS_SETTINGS_TOUCHPAD_HAPTIC_CLICK_SENSITIVITY_LABEL},
      {"touchpadHapticFeedbackTitle",
       IDS_SETTINGS_TOUCHPAD_HAPTIC_FEEDBACK_TITLE},
      {"touchpadHapticFeedbackSecondaryText",
       IDS_SETTINGS_TOUCHPAD_HAPTIC_FEEDBACK_SECONDARY_TEXT},
      {"touchpadHapticFirmClickLabel",
       IDS_SETTINGS_TOUCHPAD_HAPTIC_FIRM_CLICK_LABEL},
      {"touchpadHapticLightClickLabel",
       IDS_SETTINGS_TOUCHPAD_HAPTIC_LIGHT_CLICK_LABEL},
      {"touchpadScrollAccelerationLabel",
       IDS_SETTINGS_TOUCHPAD_SCROLL_ACCELERATION_LABEL},
      {"touchpadScrollSpeed", IDS_SETTINGS_TOUCHPAD_SCROLL_SPEED_LABEL},
  };
  html_source->AddLocalizedStrings(kPointersStrings);

  html_source->AddString("naturalScrollLearnMoreLink",
                         GetHelpUrlWithBoard(chrome::kNaturalScrollHelpURL));

  html_source->AddBoolean(
      "allowDisableMouseAcceleration",
      base::FeatureList::IsEnabled(::features::kAllowDisableMouseAcceleration));
  html_source->AddBoolean("allowScrollSettings", AreScrollSettingsAllowed());
  html_source->AddBoolean("allowTouchpadHapticFeedback",
                          base::FeatureList::IsEnabled(
                              ::features::kAllowDisableTouchpadHapticFeedback));
  html_source->AddBoolean("allowTouchpadHapticClickSettings",
                          base::FeatureList::IsEnabled(
                              ::features::kAllowTouchpadHapticClickSettings));
}

}  // namespace settings
}  // namespace chromeos
