// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/arc_android_management_checker.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/signin/public/base/consent_level.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace arc {

namespace {

constexpr base::TimeDelta kRetryDelayMin = base::Seconds(10);
constexpr base::TimeDelta kRetryDelayMax = base::Hours(1);

policy::DeviceManagementService* GetDeviceManagementService() {
  policy::BrowserPolicyConnectorAsh* const connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->device_management_service();
}

// Returns the Device Account Id. Assumes that |profile| is the only Profile
// on Chrome OS.
CoreAccountId GetDeviceAccountId(Profile* profile) {
  const auto* const identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  // The account is the same whether or not the user consented to browser sync.
  return identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
}

}  // namespace

ArcAndroidManagementChecker::ArcAndroidManagementChecker(Profile* profile,
                                                         bool retry_on_error)
    : profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)),
      device_account_id_(GetDeviceAccountId(profile_)),
      retry_on_error_(retry_on_error),
      retry_delay_(kRetryDelayMin),
      android_management_client_(
          GetDeviceManagementService(),
          g_browser_process->system_network_context_manager()
              ->GetSharedURLLoaderFactory(),
          device_account_id_,
          identity_manager_) {}

ArcAndroidManagementChecker::~ArcAndroidManagementChecker() {
  identity_manager_->RemoveObserver(this);
}

// static
void ArcAndroidManagementChecker::StartClient() {
  GetDeviceManagementService()->ScheduleInitialization(0);
}

void ArcAndroidManagementChecker::StartCheck(CheckCallback callback) {
  DCHECK(callback_.is_null());

  // Do not send requests for Chrome OS managed users, nor for well-known
  // consumer domains.
  if (policy_util::IsAccountManaged(profile_) ||
      policy::BrowserPolicyConnector::IsNonEnterpriseUser(
          profile_->GetProfileUserName())) {
    std::move(callback).Run(policy::AndroidManagementClient::Result::UNMANAGED);
    return;
  }

  callback_ = std::move(callback);
  EnsureRefreshTokenLoaded();
}

void ArcAndroidManagementChecker::EnsureRefreshTokenLoaded() {
  if (identity_manager_->HasAccountWithRefreshToken(device_account_id_)) {
    // If the refresh token is already available, just start the management
    // check immediately.
    StartCheckInternal();
    return;
  }

  // Set the observer to the token service so the callback will be called
  // when the token is loaded.
  identity_manager_->AddObserver(this);
}

void ArcAndroidManagementChecker::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (account_info.account_id != device_account_id_)
    return;
  OnRefreshTokensLoaded();
}

void ArcAndroidManagementChecker::OnRefreshTokensLoaded() {
  identity_manager_->RemoveObserver(this);
  StartCheckInternal();
}

void ArcAndroidManagementChecker::StartCheckInternal() {
  DCHECK(!callback_.is_null());

  if (!identity_manager_->HasAccountWithRefreshToken(device_account_id_)) {
    VLOG(2) << "No refresh token is available for android management check.";
    std::move(callback_).Run(policy::AndroidManagementClient::Result::ERROR);
    return;
  }

  VLOG(2) << "Start android management check.";
  android_management_client_.StartCheckAndroidManagement(
      base::BindOnce(&ArcAndroidManagementChecker::OnAndroidManagementChecked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcAndroidManagementChecker::OnAndroidManagementChecked(
    policy::AndroidManagementClient::Result result) {
  DCHECK(!callback_.is_null());
  VLOG(2) << "Android management check done " << result << ".";
  if (retry_on_error_ &&
      result == policy::AndroidManagementClient::Result::ERROR) {
    ScheduleRetry();
    return;
  }

  std::move(callback_).Run(result);
}

void ArcAndroidManagementChecker::ScheduleRetry() {
  DCHECK(retry_on_error_);
  DCHECK(!callback_.is_null());
  VLOG(2) << "Schedule next android management check in " << retry_delay_;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ArcAndroidManagementChecker::StartCheckInternal,
                     weak_ptr_factory_.GetWeakPtr()),
      retry_delay_);
  retry_delay_ = std::min(retry_delay_ * 2, kRetryDelayMax);
}

}  // namespace arc
