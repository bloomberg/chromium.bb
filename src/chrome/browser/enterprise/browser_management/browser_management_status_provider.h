// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_STATUS_PROVIDER_H_
#define CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_STATUS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/management/management_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#endif

using EnterpriseManagementAuthority = policy::EnterpriseManagementAuthority;
using ManagementAuthorityTrustworthiness =
    policy::ManagementAuthorityTrustworthiness;

class Profile;

// TODO (crbug/1238355): Add unit tests for this file.

class BrowserCloudManagementStatusProvider final
    : public policy::ManagementStatusProvider {
 public:
  BrowserCloudManagementStatusProvider();
  ~BrowserCloudManagementStatusProvider() final;

  // ManagementStatusProvider impl
  EnterpriseManagementAuthority GetAuthority() final;
};

class LocalBrowserManagementStatusProvider final
    : public policy::ManagementStatusProvider {
 public:
  LocalBrowserManagementStatusProvider();
  ~LocalBrowserManagementStatusProvider() final;

  // ManagementStatusProvider impl
  EnterpriseManagementAuthority GetAuthority() final;
};

class ProfileCloudManagementStatusProvider final
    : public policy::ManagementStatusProvider {
 public:
  explicit ProfileCloudManagementStatusProvider(Profile* profile);
  ~ProfileCloudManagementStatusProvider() final;

  // ManagementStatusProvider impl
  EnterpriseManagementAuthority GetAuthority() final;

 private:
  raw_ptr<Profile> profile_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class DeviceManagementStatusProvider final
    : public policy::ManagementStatusProvider {
 public:
  explicit DeviceManagementStatusProvider(
      policy::BrowserPolicyConnectorAsh* browser_policy_connector);
  ~DeviceManagementStatusProvider() final;

  // ManagementStatusProvider impl
  EnterpriseManagementAuthority GetAuthority() final;

 private:
  policy::BrowserPolicyConnectorAsh* browser_policy_connector_;
};
#endif

#endif  // CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_STATUS_PROVIDER_H_
