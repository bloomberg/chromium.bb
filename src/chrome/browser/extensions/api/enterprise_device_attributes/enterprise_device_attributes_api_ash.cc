// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_api_ash.h"

#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_device_attributes.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace extensions {

// TODO(http://crbug.com/1056550): Return an error if the user is not permitted
// to get device attributes instead of an empty string.

EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::
    EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction() {}

EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::
    ~EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction() {}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::Run() {
  std::string device_id;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (crosapi::browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
          profile)) {
    device_id = g_browser_process->platform_part()
                    ->browser_policy_connector_ash()
                    ->GetDirectoryApiID();
  }
  return RespondNow(ArgumentList(
      api::enterprise_device_attributes::GetDirectoryDeviceId::Results::Create(
          device_id)));
}

EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::
    EnterpriseDeviceAttributesGetDeviceSerialNumberFunction() {}

EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::
    ~EnterpriseDeviceAttributesGetDeviceSerialNumberFunction() {}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::Run() {
  std::string serial_number;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (crosapi::browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
          profile)) {
    serial_number = chromeos::system::StatisticsProvider::GetInstance()
                        ->GetEnterpriseMachineID();
  }
  return RespondNow(ArgumentList(
      api::enterprise_device_attributes::GetDeviceSerialNumber::Results::Create(
          serial_number)));
}

EnterpriseDeviceAttributesGetDeviceAssetIdFunction::
    EnterpriseDeviceAttributesGetDeviceAssetIdFunction() {}

EnterpriseDeviceAttributesGetDeviceAssetIdFunction::
    ~EnterpriseDeviceAttributesGetDeviceAssetIdFunction() {}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceAssetIdFunction::Run() {
  std::string asset_id;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (crosapi::browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
          profile)) {
    asset_id = g_browser_process->platform_part()
                   ->browser_policy_connector_ash()
                   ->GetDeviceAssetID();
  }
  return RespondNow(ArgumentList(
      api::enterprise_device_attributes::GetDeviceAssetId::Results::Create(
          asset_id)));
}

EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::
    EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction() {}

EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::
    ~EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction() {}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::Run() {
  std::string annotated_location;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (crosapi::browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
          profile)) {
    annotated_location = g_browser_process->platform_part()
                             ->browser_policy_connector_ash()
                             ->GetDeviceAnnotatedLocation();
  }
  return RespondNow(ArgumentList(
      api::enterprise_device_attributes::GetDeviceAnnotatedLocation::Results::
          Create(annotated_location)));
}

EnterpriseDeviceAttributesGetDeviceHostnameFunction::
    EnterpriseDeviceAttributesGetDeviceHostnameFunction() = default;

EnterpriseDeviceAttributesGetDeviceHostnameFunction::
    ~EnterpriseDeviceAttributesGetDeviceHostnameFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceHostnameFunction::Run() {
  // If string is nullopt, it means there is no policy set by admin.
  std::string hostname;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (crosapi::browser_util::IsSigninProfileOrBelongsToAffiliatedUser(
          profile)) {
    absl::optional<std::string> hostname_chosen_by_admin =
        g_browser_process->platform_part()
            ->browser_policy_connector_ash()
            ->GetDeviceNamePolicyHandler()
            ->GetHostnameChosenByAdministrator();
    if (hostname_chosen_by_admin)
      hostname = *hostname_chosen_by_admin;
  }
  return RespondNow(ArgumentList(
      api::enterprise_device_attributes::GetDeviceHostname::Results::Create(
          hostname)));
}

}  // namespace extensions
