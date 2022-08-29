// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_attributes_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

DeviceAttributesImpl::DeviceAttributesImpl() = default;
DeviceAttributesImpl::~DeviceAttributesImpl() = default;

std::string DeviceAttributesImpl::GetEnterpriseEnrollmentDomain() const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetEnterpriseEnrollmentDomain();
}

std::string DeviceAttributesImpl::GetEnterpriseDomainManager() const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetEnterpriseDomainManager();
}

std::string DeviceAttributesImpl::GetSSOProfile() const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetSSOProfile();
}

std::string DeviceAttributesImpl::GetRealm() const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetRealm();
}

std::string DeviceAttributesImpl::GetDeviceAssetID() const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetDeviceAssetID();
}

std::string DeviceAttributesImpl::GetMachineName() const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetMachineName();
}

std::string DeviceAttributesImpl::GetDeviceAnnotatedLocation() const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetDeviceAnnotatedLocation();
}

std::string DeviceAttributesImpl::GetDirectoryApiID() const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetDirectoryApiID();
}

std::string DeviceAttributesImpl::GetObfuscatedCustomerID() const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetObfuscatedCustomerID();
}

std::string DeviceAttributesImpl::GetCustomerLogoURL() const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetCustomerLogoURL();
}

MarketSegment DeviceAttributesImpl::GetEnterpriseMarketSegment() const {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetEnterpriseMarketSegment();
}

}  // namespace policy
