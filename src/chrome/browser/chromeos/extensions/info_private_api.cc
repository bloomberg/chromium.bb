// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/info_private_api.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "ash/components/arc/arc_util.h"
#include "ash/components/settings/cros_settings_names.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/devicetype.h"
#include "ash/public/cpp/stylus_utils.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/system/statistics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/error_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::NetworkHandler;

namespace extensions {

namespace {

// Key which corresponds to the HWID setting.
const char kPropertyHWID[] = "hwid";

// Key which corresponds to the customization ID setting.
const char kPropertyCustomizationID[] = "customizationId";

// Key which corresponds to the oem_device_requisition setting.
const char kPropertyDeviceRequisition[] = "deviceRequisition";

// Key which corresponds to the home provider property.
const char kPropertyHomeProvider[] = "homeProvider";

// Key which corresponds to the initial_locale property.
const char kPropertyInitialLocale[] = "initialLocale";

// Key which corresponds to the board property in JS.
const char kPropertyBoard[] = "board";

// Key which corresponds to the isOwner property in JS.
const char kPropertyOwner[] = "isOwner";

// Key which corresponds to the clientId property in JS.
const char kPropertyClientId[] = "clientId";

// Key which corresponds to the timezone property in JS.
const char kPropertyTimezone[] = "timezone";

// Key which corresponds to the timezone property in JS.
const char kPropertySupportedTimezones[] = "supportedTimezones";

// Key which corresponds to the large cursor A11Y property in JS.
const char kPropertyLargeCursorEnabled[] = "a11yLargeCursorEnabled";

// Key which corresponds to the sticky keys A11Y property in JS.
const char kPropertyStickyKeysEnabled[] = "a11yStickyKeysEnabled";

// Key which corresponds to the spoken feedback A11Y property in JS.
const char kPropertySpokenFeedbackEnabled[] = "a11ySpokenFeedbackEnabled";

// Key which corresponds to the high contrast mode A11Y property in JS.
const char kPropertyHighContrastEnabled[] = "a11yHighContrastEnabled";

// Key which corresponds to the screen magnifier A11Y property in JS.
const char kPropertyScreenMagnifierEnabled[] = "a11yScreenMagnifierEnabled";

// Key which corresponds to the auto click A11Y property in JS.
const char kPropertyAutoclickEnabled[] = "a11yAutoClickEnabled";

// Key which corresponds to the auto click A11Y property in JS.
const char kPropertyVirtualKeyboardEnabled[] = "a11yVirtualKeyboardEnabled";

// Key which corresponds to the caret highlight A11Y property in JS.
const char kPropertyCaretHighlightEnabled[] = "a11yCaretHighlightEnabled";

// Key which corresponds to the cursor highlight A11Y property in JS.
const char kPropertyCursorHighlightEnabled[] = "a11yCursorHighlightEnabled";

// Key which corresponds to the focus highlight A11Y property in JS.
const char kPropertyFocusHighlightEnabled[] = "a11yFocusHighlightEnabled";

// Key which corresponds to the select-to-speak A11Y property in JS.
const char kPropertySelectToSpeakEnabled[] = "a11ySelectToSpeakEnabled";

// Key which corresponds to the Switch Access A11Y property in JS.
const char kPropertySwitchAccessEnabled[] = "a11ySwitchAccessEnabled";

// Key which corresponds to the cursor color A11Y property in JS.
const char kPropertyCursorColorEnabled[] = "a11yCursorColorEnabled";

// Key which corresponds to the docked magnifier property in JS.
const char kPropertyDockedMagnifierEnabled[] = "a11yDockedMagnifierEnabled";

// Key which corresponds to the send-function-keys property in JS.
const char kPropertySendFunctionsKeys[] = "sendFunctionKeys";

// Property not found error message.
const char kPropertyNotFound[] = "Property '*' does not exist.";

// Key which corresponds to the sessionType property in JS.
const char kPropertySessionType[] = "sessionType";

// Key which corresponds to the "kiosk" value of the SessionType enum in JS.
const char kSessionTypeKiosk[] = "kiosk";

// Key which corresponds to the "public session" value of the SessionType enum
// in JS.
const char kSessionTypePublicSession[] = "public session";

// Key which corresponds to the "normal" value of the SessionType enum in JS.
const char kSessionTypeNormal[] = "normal";

// Key which corresponds to the playStoreStatus property in JS.
const char kPropertyPlayStoreStatus[] = "playStoreStatus";

// Key which corresponds to the "not available" value of the PlayStoreStatus
// enum in JS.
const char kPlayStoreStatusNotAvailable[] = "not available";

// Key which corresponds to the "available" value of the PlayStoreStatus enum in
// JS.
const char kPlayStoreStatusAvailable[] = "available";

// Key which corresponds to the "enabled" value of the PlayStoreStatus enum in
// JS.
const char kPlayStoreStatusEnabled[] = "enabled";

// Key which corresponds to the managedDeviceStatus property in JS.
const char kPropertyManagedDeviceStatus[] = "managedDeviceStatus";

// Value to which managedDeviceStatus property is set for unmanaged devices.
const char kManagedDeviceStatusNotManaged[] = "not managed";

// Value to which managedDeviceStatus property is set for managed devices.
const char kManagedDeviceStatusManaged[] = "managed";

// Key which corresponds to the deviceType property in JS.
const char kPropertyDeviceType[] = "deviceType";

// Value to which deviceType property is set for Chromebase.
const char kDeviceTypeChromebase[] = "chromebase";

// Value to which deviceType property is set for Chromebit.
const char kDeviceTypeChromebit[] = "chromebit";

// Value to which deviceType property is set for Chromebook.
const char kDeviceTypeChromebook[] = "chromebook";

// Value to which deviceType property is set for Chromebox.
const char kDeviceTypeChromebox[] = "chromebox";

// Value to which deviceType property is set when the specific type is unknown.
const char kDeviceTypeChromedevice[] = "chromedevice";

// Key which corresponds to the stylusStatus property in JS.
const char kPropertyStylusStatus[] = "stylusStatus";

// Value to which stylusStatus property is set when the device does not support
// stylus input.
const char kStylusStatusUnsupported[] = "unsupported";

// Value to which stylusStatus property is set when the device supports stylus
// input, but no stylus has been seen before.
const char kStylusStatusSupported[] = "supported";

// Value to which stylusStatus property is set when the device has a built-in
// stylus or a stylus has been seen before.
const char kStylusStatusSeen[] = "seen";

// Key which corresponds to the assistantStatus property in JS.
const char kPropertyAssistantStatus[] = "assistantStatus";

// Value to which assistantStatus property is set when the device supports
// Assistant.
const char kAssistantStatusSupported[] = "supported";

const struct {
  const char* api_name;
  const char* preference_name;
} kPreferencesMap[] = {
    {kPropertyLargeCursorEnabled, ash::prefs::kAccessibilityLargeCursorEnabled},
    {kPropertyStickyKeysEnabled, ash::prefs::kAccessibilityStickyKeysEnabled},
    {kPropertySpokenFeedbackEnabled,
     ash::prefs::kAccessibilitySpokenFeedbackEnabled},
    {kPropertyHighContrastEnabled,
     ash::prefs::kAccessibilityHighContrastEnabled},
    {kPropertyScreenMagnifierEnabled,
     ash::prefs::kAccessibilityScreenMagnifierEnabled},
    {kPropertyAutoclickEnabled, ash::prefs::kAccessibilityAutoclickEnabled},
    {kPropertyVirtualKeyboardEnabled,
     ash::prefs::kAccessibilityVirtualKeyboardEnabled},
    {kPropertyCaretHighlightEnabled,
     ash::prefs::kAccessibilityCaretHighlightEnabled},
    {kPropertyCursorHighlightEnabled,
     ash::prefs::kAccessibilityCursorHighlightEnabled},
    {kPropertyFocusHighlightEnabled,
     ash::prefs::kAccessibilityFocusHighlightEnabled},
    {kPropertySelectToSpeakEnabled,
     ash::prefs::kAccessibilitySelectToSpeakEnabled},
    {kPropertySwitchAccessEnabled,
     ash::prefs::kAccessibilitySwitchAccessEnabled},
    {kPropertyCursorColorEnabled, ash::prefs::kAccessibilityCursorColorEnabled},
    {kPropertyDockedMagnifierEnabled, ash::prefs::kDockedMagnifierEnabled},
    {kPropertySendFunctionsKeys, prefs::kLanguageSendFunctionKeys}};

const char* GetBoolPrefNameForApiProperty(const char* api_name) {
  for (size_t i = 0;
       i < (sizeof(kPreferencesMap)/sizeof(*kPreferencesMap));
       i++) {
    if (strcmp(kPreferencesMap[i].api_name, api_name) == 0)
      return kPreferencesMap[i].preference_name;
  }

  return NULL;
}

bool IsEnterpriseKiosk() {
  if (!chrome::IsRunningInForcedAppMode())
    return false;

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->IsDeviceEnterpriseManaged();
}

std::string GetClientId() {
  return IsEnterpriseKiosk()
             ? g_browser_process->metrics_service()->GetClientId()
             : std::string();
}

}  // namespace

ChromeosInfoPrivateGetFunction::ChromeosInfoPrivateGetFunction() {
}

ChromeosInfoPrivateGetFunction::~ChromeosInfoPrivateGetFunction() {
}

ExtensionFunction::ResponseAction ChromeosInfoPrivateGetFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(!args().empty() && args()[0].is_list());
  base::Value::ConstListView list = args()[0].GetList();

  base::Value result(base::Value::Type::DICTIONARY);
  for (size_t i = 0; i < list.size(); ++i) {
    EXTENSION_FUNCTION_VALIDATE(list[i].is_string());
    std::string property_name = list[i].GetString();
    std::unique_ptr<base::Value> value = GetValue(property_name);
    if (value)
      result.SetKey(property_name,
                    base::Value::FromUniquePtrValue(std::move(value)));
  }
  return RespondNow(OneArgument(std::move(result)));
}

std::unique_ptr<base::Value> ChromeosInfoPrivateGetFunction::GetValue(
    const std::string& property_name) {
  if (property_name == kPropertyHWID) {
    std::string hwid;
    chromeos::system::StatisticsProvider* provider =
        chromeos::system::StatisticsProvider::GetInstance();
    provider->GetMachineStatistic(chromeos::system::kHardwareClassKey, &hwid);
    return std::make_unique<base::Value>(hwid);
  }

  if (property_name == kPropertyCustomizationID) {
    std::string customization_id;
    chromeos::system::StatisticsProvider* provider =
        chromeos::system::StatisticsProvider::GetInstance();
    provider->GetMachineStatistic(chromeos::system::kCustomizationIdKey,
                                  &customization_id);
    return std::make_unique<base::Value>(customization_id);
  }

  if (property_name == kPropertyDeviceRequisition) {
    std::string device_requisition;
    chromeos::system::StatisticsProvider* provider =
        chromeos::system::StatisticsProvider::GetInstance();
    provider->GetMachineStatistic(chromeos::system::kOemDeviceRequisitionKey,
                                  &device_requisition);
    return std::make_unique<base::Value>(device_requisition);
  }

  if (property_name == kPropertyHomeProvider) {
    const chromeos::DeviceState* cellular_device =
        NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
            chromeos::NetworkTypePattern::Cellular());
    std::string home_provider_id;
    if (cellular_device) {
      if (!cellular_device->country_code().empty()) {
        home_provider_id = base::StringPrintf(
            "%s (%s)", cellular_device->operator_name().c_str(),
            cellular_device->country_code().c_str());
      } else {
        home_provider_id = cellular_device->operator_name();
      }
    }
    return std::make_unique<base::Value>(home_provider_id);
  }

  if (property_name == kPropertyInitialLocale) {
    return std::make_unique<base::Value>(ash::StartupUtils::GetInitialLocale());
  }

  if (property_name == kPropertyBoard) {
    return std::make_unique<base::Value>(base::SysInfo::GetLsbReleaseBoard());
  }

  if (property_name == kPropertyOwner) {
    return std::make_unique<base::Value>(
        user_manager::UserManager::Get()->IsCurrentUserOwner());
  }

  if (property_name == kPropertySessionType) {
    if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode())
      return std::make_unique<base::Value>(kSessionTypeKiosk);
    if (ExtensionsBrowserClient::Get()->IsLoggedInAsPublicAccount())
      return std::make_unique<base::Value>(kSessionTypePublicSession);
    return std::make_unique<base::Value>(kSessionTypeNormal);
  }

  if (property_name == kPropertyPlayStoreStatus) {
    if (arc::IsArcAllowedForProfile(
            Profile::FromBrowserContext(browser_context())))
      return std::make_unique<base::Value>(kPlayStoreStatusEnabled);
    if (arc::IsArcAvailable())
      return std::make_unique<base::Value>(kPlayStoreStatusAvailable);
    return std::make_unique<base::Value>(kPlayStoreStatusNotAvailable);
  }

  if (property_name == kPropertyManagedDeviceStatus) {
    policy::BrowserPolicyConnectorAsh* connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    if (connector->IsDeviceEnterpriseManaged()) {
      return std::make_unique<base::Value>(kManagedDeviceStatusManaged);
    }
    return std::make_unique<base::Value>(kManagedDeviceStatusNotManaged);
  }

  if (property_name == kPropertyDeviceType) {
    switch (chromeos::GetDeviceType()) {
      case chromeos::DeviceType::kChromebox:
        return std::make_unique<base::Value>(kDeviceTypeChromebox);
      case chromeos::DeviceType::kChromebase:
        return std::make_unique<base::Value>(kDeviceTypeChromebase);
      case chromeos::DeviceType::kChromebit:
        return std::make_unique<base::Value>(kDeviceTypeChromebit);
      case chromeos::DeviceType::kChromebook:
        return std::make_unique<base::Value>(kDeviceTypeChromebook);
      default:
        return std::make_unique<base::Value>(kDeviceTypeChromedevice);
    }
  }

  if (property_name == kPropertyStylusStatus) {
    if (!ash::stylus_utils::HasStylusInput()) {
      return std::make_unique<base::Value>(kStylusStatusUnsupported);
    }

    bool seen = g_browser_process->local_state()->HasPrefPath(
        ash::prefs::kHasSeenStylus);
    return std::make_unique<base::Value>(seen ? kStylusStatusSeen
                                              : kStylusStatusSupported);
  }

  if (property_name == kPropertyAssistantStatus) {
    return std::make_unique<base::Value>(kAssistantStatusSupported);
  }

  if (property_name == kPropertyClientId) {
    return std::make_unique<base::Value>(GetClientId());
  }

  if (property_name == kPropertyTimezone) {
    if (ash::system::PerUserTimezoneEnabled()) {
      const PrefService::Preference* timezone =
          Profile::FromBrowserContext(browser_context())
              ->GetPrefs()
              ->FindPreference(prefs::kUserTimezone);
      return std::make_unique<base::Value>(timezone->GetValue()->Clone());
    }
    // TODO(crbug.com/697817): Convert CrosSettings::Get to take a unique_ptr.
    return base::Value::ToUniquePtrValue(
        ash::CrosSettings::Get()->GetPref(ash::kSystemTimezone)->Clone());
  }

  if (property_name == kPropertySupportedTimezones) {
    return ash::system::GetTimezoneList();
  }

  const char* pref_name = GetBoolPrefNameForApiProperty(property_name.c_str());
  if (pref_name) {
    return std::make_unique<base::Value>(
        Profile::FromBrowserContext(browser_context())
            ->GetPrefs()
            ->GetBoolean(pref_name));
  }

  DLOG(ERROR) << "Unknown property request: " << property_name;
  return nullptr;
}

ChromeosInfoPrivateSetFunction::ChromeosInfoPrivateSetFunction() {
}

ChromeosInfoPrivateSetFunction::~ChromeosInfoPrivateSetFunction() {
}

ExtensionFunction::ResponseAction ChromeosInfoPrivateSetFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  const std::string& param_name = args()[0].GetString();

  if (param_name == kPropertyTimezone) {
    EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
    EXTENSION_FUNCTION_VALIDATE(args()[1].is_string());
    const std::string& param_value = args()[1].GetString();
    if (ash::system::PerUserTimezoneEnabled()) {
      Profile::FromBrowserContext(browser_context())
          ->GetPrefs()
          ->SetString(prefs::kUserTimezone, param_value);
    } else {
      const user_manager::User* user =
          chromeos::ProfileHelper::Get()->GetUserByProfile(
              Profile::FromBrowserContext(browser_context()));
      if (user)
        ash::system::SetSystemTimezone(user, param_value);
    }

  } else {
    const char* pref_name = GetBoolPrefNameForApiProperty(param_name.c_str());
    if (pref_name) {
      EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
      EXTENSION_FUNCTION_VALIDATE(args()[1].is_bool());
      bool param_value = args()[1].GetBool();
      Profile::FromBrowserContext(browser_context())
          ->GetPrefs()
          ->SetBoolean(pref_name, param_value);
    } else {
      return RespondNow(Error(kPropertyNotFound, param_name));
    }
  }

  return RespondNow(NoArguments());
}

ChromeosInfoPrivateIsTabletModeEnabledFunction::
    ChromeosInfoPrivateIsTabletModeEnabledFunction() {}

ChromeosInfoPrivateIsTabletModeEnabledFunction::
    ~ChromeosInfoPrivateIsTabletModeEnabledFunction() {}

ExtensionFunction::ResponseAction
ChromeosInfoPrivateIsTabletModeEnabledFunction::Run() {
  return RespondNow(
      OneArgument(base::Value(ash::TabletMode::Get()->InTabletMode())));
}

}  // namespace extensions
