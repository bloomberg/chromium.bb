// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

ScopedDeviceSettingsTestHelper::ScopedDeviceSettingsTestHelper() {
  DeviceSettingsService::Initialize();
  DeviceSettingsService::Get()->SetSessionManager(
      &session_manager_client_, new ownership::MockOwnerKeyUtil());
  DeviceSettingsService::Get()->Load();
  content::RunAllTasksUntilIdle();
}

ScopedDeviceSettingsTestHelper::~ScopedDeviceSettingsTestHelper() {
  content::RunAllTasksUntilIdle();
  DeviceSettingsService::Get()->UnsetSessionManager();
  DeviceSettingsService::Shutdown();
}

DeviceSettingsTestBase::DeviceSettingsTestBase()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      user_manager_(new FakeChromeUserManager()),
      user_manager_enabler_(base::WrapUnique(user_manager_)),
      owner_key_util_(new ownership::MockOwnerKeyUtil()),
      dbus_setter_(chromeos::DBusThreadManager::GetSetterForTesting()) {
  OwnerSettingsServiceChromeOSFactory::SetDeviceSettingsServiceForTesting(
      &device_settings_service_);
  OwnerSettingsServiceChromeOSFactory::GetInstance()->SetOwnerKeyUtilForTesting(
      owner_key_util_);
  base::RunLoop().RunUntilIdle();

  device_policy_.payload().mutable_metrics_enabled()->set_metrics_enabled(
      false);
  ReloadDevicePolicy();
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
  device_settings_service_.SetSessionManager(&session_manager_client_,
                                             owner_key_util_);

  profile_.reset(new TestingProfile());
}

DeviceSettingsTestBase::~DeviceSettingsTestBase() {
  OwnerSettingsServiceChromeOSFactory::SetDeviceSettingsServiceForTesting(NULL);
  FlushDeviceSettings();
  device_settings_service_.UnsetSessionManager();
  DBusThreadManager::Shutdown();
  base::RunLoop().RunUntilIdle();
}

void DeviceSettingsTestBase::ReloadDevicePolicy() {
  device_policy_.Build();
  session_manager_client_.set_device_policy(device_policy_.GetBlob());
}

void DeviceSettingsTestBase::FlushDeviceSettings() {
  content::RunAllTasksUntilIdle();
}

void DeviceSettingsTestBase::ReloadDeviceSettings() {
  device_settings_service_.OwnerKeySet(true);
  FlushDeviceSettings();
}

void DeviceSettingsTestBase::InitOwner(const AccountId& account_id,
                                       bool tpm_is_ready) {
  const user_manager::User* user = user_manager_->FindUser(account_id);
  if (!user) {
    user = user_manager_->AddUser(account_id);
    profile_->set_profile_name(account_id.GetUserEmail());

    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                            profile_.get());
    ProfileHelper::Get()->SetProfileToUserMappingForTesting(
        const_cast<user_manager::User*>(user));
  }
  OwnerSettingsServiceChromeOS* service =
      OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(profile_.get());
  CHECK(service);
  if (tpm_is_ready)
    service->OnTPMTokenReady(true /* token is enabled */);
}

}  // namespace chromeos
