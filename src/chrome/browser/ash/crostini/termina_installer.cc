// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/termina_installer.h"

#include <algorithm>
#include <memory>

#include "ash/constants/ash_features.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace crostini {

namespace {

const char kHistogram[] = "Crostini.InstallSource";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InstallSource {
  Component = 0,
  DLC = 1,
  kMaxValue = DLC,
};

}  // namespace

TerminaInstaller::TerminaInstaller() {}
TerminaInstaller::~TerminaInstaller() {}

void TerminaInstaller::Cancel() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void TerminaInstaller::Install(base::OnceCallback<void(InstallResult)> callback,
                               bool is_initial_install) {
  // The Remove*IfPresent methods require an unowned UninstallResult pointer to
  // record their success/failure state. This has to be unowned so that in
  // Uninstall it can be accessed further down the callback chain, but here we
  // don't care about it, so we assign ownership of the pointer to a
  // base::DoNothing that will delete it once called.
  auto ptr = std::make_unique<UninstallResult>();
  auto* uninstall_result_ptr = ptr.get();
  auto remove_callback = base::BindOnce(
      [](std::unique_ptr<UninstallResult> ptr) {}, std::move(ptr));

  // Remove whichever version of termina we're *not* using and install the right
  // one.
  if (base::FeatureList::IsEnabled(chromeos::features::kCrostiniUseDlc)) {
    InstallDlc(
        base::BindOnce(
            [](base::WeakPtr<TerminaInstaller> weak_this,
               base::OnceCallback<void(InstallResult)> callback,
               bool is_initial_install, base::OnceClosure remove_callback,
               UninstallResult* uninstall_result_ptr, InstallResult result) {
              if (!weak_this)
                return;

              // Fallback logic for the transition to DLC.
              // If we succeeded with DLC, we're good.
              // If we're running the installer, we can show a useful error
              // message. Otherwise, try and fall back to installing the
              // cros-termina component.
              if (is_initial_install || result == InstallResult::Success) {
                // Delay removing cros-termina until here so as to avoid messing
                // up the InstallComponent call below.
                weak_this->RemoveComponentIfPresent(std::move(remove_callback),
                                                    uninstall_result_ptr);
                std::move(callback).Run(result);
                return;
              }
              LOG(ERROR) << "Failed to install termina-dlc, falling back to "
                            "cros-termina";
              weak_this->InstallComponent(std::move(callback));
            },
            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
            is_initial_install, std::move(remove_callback),
            uninstall_result_ptr),
        is_initial_install);
  } else {
    RemoveDlcIfPresent(std::move(remove_callback), uninstall_result_ptr);
    InstallComponent(std::move(callback));
  }
}

void TerminaInstaller::InstallDlc(
    base::OnceCallback<void(InstallResult)> callback,
    bool is_initial_install) {
  chromeos::DlcserviceClient::Get()->Install(
      kCrostiniDlcName,
      base::BindOnce(&TerminaInstaller::OnInstallDlc,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     is_initial_install),
      base::DoNothing());
}

void TerminaInstaller::OnInstallDlc(
    base::OnceCallback<void(InstallResult)> callback,
    bool is_initial_install,
    const chromeos::DlcserviceClient::InstallResult& result) {
  CHECK(result.dlc_id == kCrostiniDlcName);
  InstallResult response;
  if (result.error == dlcservice::kErrorNone) {
    response = InstallResult::Success;
    dlc_id_ = kCrostiniDlcName;
    termina_location_ = base::FilePath(result.root_path);
    UMA_HISTOGRAM_ENUMERATION(kHistogram, InstallSource::DLC);
  } else if (is_initial_install && result.error == dlcservice::kErrorBusy) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TerminaInstaller::InstallDlc,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       is_initial_install),
        base::Seconds(5));
    return;
  } else if (result.error == dlcservice::kErrorNeedReboot ||
             result.error == dlcservice::kErrorNoImageFound) {
    LOG(ERROR)
        << "Failed to install termina-dlc because the OS must be updated";
    response = InstallResult::NeedUpdate;
  } else {
    if (content::GetNetworkConnectionTracker()->IsOffline()) {
      LOG(ERROR) << "Failed to install termina-dlc while offline, assuming "
                    "network issue: "
                 << result.error;
      response = InstallResult::Offline;
    } else {
      LOG(ERROR) << "Failed to install termina-dlc: " << result.error;
      response = InstallResult::Failure;
    }
  }
  std::move(callback).Run(response);
}

using UpdatePolicy = component_updater::CrOSComponentManager::UpdatePolicy;

void TerminaInstaller::InstallComponent(
    base::OnceCallback<void(InstallResult)> callback) {
  scoped_refptr<component_updater::CrOSComponentManager> component_manager =
      g_browser_process->platform_part()->cros_component_manager();

  bool major_update_required =
      component_manager->GetCompatiblePath(imageloader::kTerminaComponentName)
          .empty();
  bool is_offline = content::GetNetworkConnectionTracker()->IsOffline();

  if (major_update_required) {
    component_update_check_needed_ = false;
    if (is_offline) {
      LOG(ERROR) << "Need to load a major component update, but we're offline.";
      std::move(callback).Run(InstallResult::Offline);
      return;
    }
  }

  UpdatePolicy update_policy;
  if (component_update_check_needed_ && !is_offline) {
    // Don't use kForce all the time because it generates traffic to
    // ComponentUpdaterService. Also, it's only appropriate for minor version
    // updates. Not major version incompatiblility.
    update_policy = UpdatePolicy::kForce;
  } else {
    update_policy = UpdatePolicy::kDontForce;
  }

  component_manager->Load(
      imageloader::kTerminaComponentName,
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      update_policy,
      base::BindOnce(&TerminaInstaller::OnInstallComponent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     update_policy == UpdatePolicy::kForce));
}

void TerminaInstaller::OnInstallComponent(
    base::OnceCallback<void(InstallResult)> callback,
    bool is_update_checked,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  bool is_successful =
      error == component_updater::CrOSComponentManager::Error::NONE;

  if (is_successful) {
    dlc_id_ = absl::nullopt;
    termina_location_ = path;
    UMA_HISTOGRAM_ENUMERATION(kHistogram, InstallSource::Component);
  } else {
    LOG(ERROR)
        << "Failed to install the cros-termina component with error code: "
        << static_cast<int>(error);

    if (error ==
        component_updater::CrOSComponentManager::Error::MOUNT_FAILURE) {
      ReinstallComponent(std::move(callback));
      return;
    }
    if (is_update_checked) {
      scoped_refptr<component_updater::CrOSComponentManager> component_manager =
          g_browser_process->platform_part()->cros_component_manager();
      if (component_manager) {
        // Try again, this time with no update checking. The reason we do this
        // is that we may still be offline even when is_offline above was
        // false. It's notoriously difficult to know when you're really
        // connected to the Internet, and it's also possible to be unable to
        // connect to a service like ComponentUpdaterService even when you are
        // connected to the rest of the Internet.
        UpdatePolicy update_policy = UpdatePolicy::kDontForce;

        LOG(ERROR) << "Retrying cros-termina component load, no update check";
        // Load the existing component on disk.
        component_manager->Load(
            imageloader::kTerminaComponentName,
            component_updater::CrOSComponentManager::MountPolicy::kMount,
            update_policy,
            base::BindOnce(&TerminaInstaller::OnInstallComponent,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           false));
        return;
      }
    }
  }

  if (is_successful && is_update_checked) {
    VLOG(1) << "cros-termina update check successful.";
    component_update_check_needed_ = false;
  }
  InstallResult result = InstallResult::Success;
  if (!is_successful) {
    if (error ==
        component_updater::CrOSComponentManager::Error::UPDATE_IN_PROGRESS) {
      // Something else triggered an update that we have to wait on. We don't
      // know what, or when they will be finished, so just retry every 5 seconds
      // until we get a different result.
      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&TerminaInstaller::InstallComponent,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          base::Seconds(5));
      return;
    } else {
      result = InstallResult::Failure;
    }
  }

  std::move(callback).Run(result);
}

void TerminaInstaller::ReinstallComponent(
    base::OnceCallback<void(InstallResult)> callback) {
  scoped_refptr<component_updater::CrOSComponentManager> component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  if (component_manager->Unload(imageloader::kTerminaComponentName)) {
    component_manager->Load(
        imageloader::kTerminaComponentName,
        component_updater::CrOSComponentManager::MountPolicy::kMount,
        UpdatePolicy::kDontForce,
        base::BindOnce(&TerminaInstaller::OnReinstallComponent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       false));
  } else {
    std::move(callback).Run(InstallResult::Failure);
  }
}

void TerminaInstaller::OnReinstallComponent(
    base::OnceCallback<void(InstallResult)> callback,
    bool is_update_checked,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  LOG(ERROR) << "Attempting to re-install cros-termina component.";
  if (error != component_updater::CrOSComponentManager::Error::MOUNT_FAILURE) {
    OnInstallComponent(std::move(callback), is_update_checked, error, path);
    return;
  }
  // Give up with a permanent failure. The newly downloaded component failed to
  // mount.
  std::move(callback).Run(InstallResult::Failure);
}

void TerminaInstaller::Uninstall(base::OnceCallback<void(bool)> callback) {
  // Unset |termina_location_| now since it will become invalid at some point
  // soon.
  termina_location_ = absl::nullopt;

  // This is really a vector of bool, but std::vector<bool> has weird properties
  // that stop us from using it in this way.
  std::vector<UninstallResult> partial_results{0, 0};
  UninstallResult* component_result = &partial_results[0];
  UninstallResult* dlc_result = &partial_results[1];

  // We want to get the results from both uninstall calls and combine them, and
  // the asynchronous nature of this process means we can't use return values,
  // so we need to pass a pointer into those calls to store their results and
  // pass ownership of that memory into the result callback.
  auto b_closure = BarrierClosure(
      2, base::BindOnce(&TerminaInstaller::OnUninstallFinished,
                        weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                        std::move(partial_results)));

  RemoveComponentIfPresent(b_closure, component_result);
  RemoveDlcIfPresent(b_closure, dlc_result);
}

void TerminaInstaller::RemoveComponentIfPresent(
    base::OnceCallback<void()> callback,
    UninstallResult* result) {
  VLOG(1) << "Removing component";
  scoped_refptr<component_updater::CrOSComponentManager> component_manager =
      g_browser_process->platform_part()->cros_component_manager();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](scoped_refptr<component_updater::CrOSComponentManager>
                 component_manager) {
            return component_manager->IsRegisteredMayBlock(
                imageloader::kTerminaComponentName);
          },
          std::move(component_manager)),
      base::BindOnce(
          [](base::OnceCallback<void()> callback, UninstallResult* result,
             bool is_present) {
            scoped_refptr<component_updater::CrOSComponentManager>
                component_manager = g_browser_process->platform_part()
                                        ->cros_component_manager();
            if (is_present) {
              VLOG(1) << "Component present, unloading";
              *result =
                  component_manager->Unload(imageloader::kTerminaComponentName);
              if (!*result) {
                LOG(ERROR) << "Failed to remove cros-termina component";
              }
            } else {
              VLOG(1) << "No component present, skipping";
              *result = true;
            }
            std::move(callback).Run();
          },
          std::move(callback), result));
}

void TerminaInstaller::RemoveDlcIfPresent(base::OnceCallback<void()> callback,
                                          UninstallResult* result) {
  chromeos::DlcserviceClient::Get()->GetExistingDlcs(base::BindOnce(
      [](base::WeakPtr<TerminaInstaller> weak_this,
         base::OnceCallback<void()> callback, UninstallResult* result,
         const std::string& err,
         const dlcservice::DlcsWithContent& dlcs_with_content) {
        if (!weak_this)
          return;

        if (err != dlcservice::kErrorNone) {
          LOG(ERROR) << "Failed to list installed DLCs: " << err;
          *result = false;
          std::move(callback).Run();
          return;
        }
        for (const auto& dlc : dlcs_with_content.dlc_infos()) {
          if (dlc.id() == kCrostiniDlcName) {
            VLOG(1) << "DLC present, removing";
            weak_this->RemoveDlc(std::move(callback), result);
            return;
          }
        }
        VLOG(1) << "No DLC present, skipping";
        *result = true;
        std::move(callback).Run();
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), result));
}

void TerminaInstaller::RemoveDlc(base::OnceCallback<void()> callback,
                                 UninstallResult* result) {
  chromeos::DlcserviceClient::Get()->Uninstall(
      kCrostiniDlcName,
      base::BindOnce(
          [](base::OnceCallback<void()> callback, UninstallResult* result,
             const std::string& err) {
            if (err == dlcservice::kErrorNone) {
              VLOG(1) << "Removed DLC";
              *result = true;
            } else {
              LOG(ERROR) << "Failed to remove termina-dlc: " << err;
              *result = false;
            }
            std::move(callback).Run();
          },
          std::move(callback), result));
}

void TerminaInstaller::OnUninstallFinished(
    base::OnceCallback<void(bool)> callback,
    std::vector<UninstallResult> partial_results) {
  bool result = std::all_of(partial_results.begin(), partial_results.end(),
                            [](bool b) { return b; });
  std::move(callback).Run(result);
}

base::FilePath TerminaInstaller::GetInstallLocation() {
  CHECK(termina_location_)
      << "GetInstallLocation() called while termina not installed";
  return *termina_location_;
}

absl::optional<std::string> TerminaInstaller::GetDlcId() {
  CHECK(termina_location_) << "GetDlcId() called while termina not installed";
  return dlc_id_;
}

}  // namespace crostini
