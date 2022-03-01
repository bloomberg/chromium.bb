// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_container_client_adapter.h"

#include <string>
#include <utility>

#include "ash/components/arc/session/arc_session.h"
#include "ash/components/arc/session/arc_upgrade_params.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/login_manager/arc.pb.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"

namespace arc {
namespace {

// Converts PackageCacheMode into login_manager's.
login_manager::UpgradeArcContainerRequest_PackageCacheMode
ToLoginManagerPackageCacheMode(UpgradeParams::PackageCacheMode mode) {
  switch (mode) {
    case UpgradeParams::PackageCacheMode::DEFAULT:
      return login_manager::UpgradeArcContainerRequest_PackageCacheMode_DEFAULT;
    case UpgradeParams::PackageCacheMode::COPY_ON_INIT:
      return login_manager::
          UpgradeArcContainerRequest_PackageCacheMode_COPY_ON_INIT;
    case UpgradeParams::PackageCacheMode::SKIP_SETUP_COPY_ON_INIT:
      return login_manager::
          UpgradeArcContainerRequest_PackageCacheMode_SKIP_SETUP_COPY_ON_INIT;
  }
}

// Converts ArcManagementTransition into login_manager's.
login_manager::UpgradeArcContainerRequest_ManagementTransition
ToLoginManagerManagementTransition(ArcManagementTransition transition) {
  switch (transition) {
    case ArcManagementTransition::NO_TRANSITION:
      return login_manager::
          UpgradeArcContainerRequest_ManagementTransition_NONE;
    case ArcManagementTransition::CHILD_TO_REGULAR:
      return login_manager::
          UpgradeArcContainerRequest_ManagementTransition_CHILD_TO_REGULAR;
    case ArcManagementTransition::REGULAR_TO_CHILD:
      return login_manager::
          UpgradeArcContainerRequest_ManagementTransition_REGULAR_TO_CHILD;
    case ArcManagementTransition::UNMANAGED_TO_MANAGED:
      return login_manager::
          UpgradeArcContainerRequest_ManagementTransition_UNMANAGED_TO_MANAGED;
  }
}

// Converts PlayStoreAutoUpdate into login_manager's.
login_manager::StartArcMiniContainerRequest_PlayStoreAutoUpdate
ToLoginManagerPlayStoreAutoUpdate(StartParams::PlayStoreAutoUpdate update) {
  switch (update) {
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_DEFAULT:
      return login_manager::
          StartArcMiniContainerRequest_PlayStoreAutoUpdate_AUTO_UPDATE_DEFAULT;
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_ON:
      return login_manager::
          StartArcMiniContainerRequest_PlayStoreAutoUpdate_AUTO_UPDATE_ON;
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_OFF:
      return login_manager::
          StartArcMiniContainerRequest_PlayStoreAutoUpdate_AUTO_UPDATE_OFF;
  }
}

// Converts DalvikMemoryProfile into login_manager's.
login_manager::StartArcMiniContainerRequest_DalvikMemoryProfile
ToLoginManagerDalvikMemoryProfile(
    StartParams::DalvikMemoryProfile dalvik_memory_profile) {
  switch (dalvik_memory_profile) {
    case StartParams::DalvikMemoryProfile::DEFAULT:
      return login_manager::
          StartArcMiniContainerRequest_DalvikMemoryProfile_MEMORY_PROFILE_DEFAULT;
    case StartParams::DalvikMemoryProfile::M4G:
      return login_manager::
          StartArcMiniContainerRequest_DalvikMemoryProfile_MEMORY_PROFILE_4G;
    case StartParams::DalvikMemoryProfile::M8G:
      return login_manager::
          StartArcMiniContainerRequest_DalvikMemoryProfile_MEMORY_PROFILE_8G;
    case StartParams::DalvikMemoryProfile::M16G:
      return login_manager::
          StartArcMiniContainerRequest_DalvikMemoryProfile_MEMORY_PROFILE_16G;
  }
}

}  // namespace

class ArcContainerClientAdapter
    : public ArcClientAdapter,
      public chromeos::SessionManagerClient::Observer {
 public:
  ArcContainerClientAdapter() {
    if (chromeos::SessionManagerClient::Get())
      chromeos::SessionManagerClient::Get()->AddObserver(this);
  }

  ArcContainerClientAdapter(const ArcContainerClientAdapter&) = delete;
  ArcContainerClientAdapter& operator=(const ArcContainerClientAdapter&) =
      delete;

  ~ArcContainerClientAdapter() override {
    if (chromeos::SessionManagerClient::Get())
      chromeos::SessionManagerClient::Get()->RemoveObserver(this);
  }

  login_manager::StartArcMiniContainerRequest
  ConvertStartParamsToStartArcMiniContainerRequest(StartParams params) {
    login_manager::StartArcMiniContainerRequest request;
    request.set_native_bridge_experiment(params.native_bridge_experiment);
    request.set_lcd_density(params.lcd_density);
    request.set_arc_file_picker_experiment(params.arc_file_picker_experiment);
    request.set_play_store_auto_update(
        ToLoginManagerPlayStoreAutoUpdate(params.play_store_auto_update));
    request.set_dalvik_memory_profile(
        ToLoginManagerDalvikMemoryProfile(params.dalvik_memory_profile));
    request.set_arc_custom_tabs_experiment(params.arc_custom_tabs_experiment);
    request.set_disable_system_default_app(
        params.arc_disable_system_default_app);
    request.set_disable_media_store_maintenance(
        params.disable_media_store_maintenance);
    request.set_disable_download_provider(params.disable_download_provider);
    request.set_disable_ureadahead(params.disable_ureadahead);
    request.set_arc_generate_pai(params.arc_generate_play_auto_install);
    request.set_enable_notifications_refresh(
        params.enable_notifications_refresh);
    return request;
  }

  // ArcClientAdapter overrides:
  void StartMiniArc(StartParams params,
                    chromeos::VoidDBusMethodCallback callback) override {
    switch (params.usap_profile) {
      case StartParams::UsapProfile::DEFAULT:
        break;
      case StartParams::UsapProfile::M4G:
      case StartParams::UsapProfile::M8G:
      case StartParams::UsapProfile::M16G:
        VLOG(1) << "USAP profile is not supported for container.";
        break;
    }

    auto request =
        ConvertStartParamsToStartArcMiniContainerRequest(std::move(params));
    chromeos::SessionManagerClient::Get()->StartArcMiniContainer(
        request, std::move(callback));
  }

  login_manager::UpgradeArcContainerRequest
  ConvertUpgradeParamsToUpgradeArcContainerRequest(UpgradeParams params) {
    login_manager::UpgradeArcContainerRequest request;
    request.set_account_id(params.account_id);
    request.set_is_account_managed(params.is_account_managed);
    request.set_is_managed_adb_sideloading_allowed(
        params.is_managed_adb_sideloading_allowed);
    request.set_skip_boot_completed_broadcast(
        params.skip_boot_completed_broadcast);
    request.set_packages_cache_mode(
        ToLoginManagerPackageCacheMode(params.packages_cache_mode));
    request.set_skip_gms_core_cache(params.skip_gms_core_cache);
    request.set_is_demo_session(params.is_demo_session);
    request.set_demo_session_apps_path(params.demo_session_apps_path.value());
    request.set_locale(params.locale);
    request.set_enable_arc_nearby_share(params.enable_arc_nearby_share);
    for (const auto& language : params.preferred_languages)
      request.add_preferred_languages(language);
    request.set_management_transition(
        ToLoginManagerManagementTransition(params.management_transition));
    return request;
  }

  void UpgradeArc(UpgradeParams params,
                  chromeos::VoidDBusMethodCallback callback) override {
    auto request = ConvertUpgradeParamsToUpgradeArcContainerRequest(params);
    chromeos::SessionManagerClient::Get()->UpgradeArcContainer(
        request, std::move(callback));
  }

  void StopArcInstance(bool on_shutdown, bool should_backup_log) override {
    // Since we have the ArcInstanceStopped() callback, we don't need to do
    // anything when StopArcInstance completes.
    chromeos::SessionManagerClient::Get()->StopArcInstance(
        cryptohome_id_.id(), should_backup_log, base::DoNothing());
  }

  void SetUserInfo(const cryptohome::Identification& cryptohome_id,
                   const std::string& hash,
                   const std::string& serial_number) override {
    DCHECK(cryptohome_id_.id().empty());
    if (cryptohome_id.id().empty())
      LOG(WARNING) << "cryptohome_id is empty";
    cryptohome_id_ = cryptohome_id;
  }

  // ArcContainerClientAdapter gets the demo session apps path from
  // UpgradeParams, so it does not use the DemoModeDelegate.
  void SetDemoModeDelegate(DemoModeDelegate* delegate) override {}

  // The interface is only for ARCVM.
  void TrimVmMemory(TrimVmMemoryCallback callback) override {
    NOTREACHED();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), /*success=*/true,
                       /*failure_reason=*/"ARC container is not supported."));
  }

  // chromeos::SessionManagerClient::Observer overrides:
  void ArcInstanceStopped(
      login_manager::ArcContainerStopReason reason) override {
    const bool is_system_shutdown =
        reason ==
        login_manager::ArcContainerStopReason::SESSION_MANAGER_SHUTDOWN;
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped(is_system_shutdown);
  }

 private:
  // A cryptohome ID of the primary profile.
  cryptohome::Identification cryptohome_id_;
};

std::unique_ptr<ArcClientAdapter> CreateArcContainerClientAdapter() {
  return std::make_unique<ArcContainerClientAdapter>();
}

}  // namespace arc
