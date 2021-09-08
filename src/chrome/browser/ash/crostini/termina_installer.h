// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_TERMINA_INSTALLER_H_
#define CHROME_BROWSER_ASH_CROSTINI_TERMINA_INSTALLER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crostini {

// This class is responsible for tracking which source the termina VM is
// installed from (component updater, dlc service etc.) and ensuring that any
// unneeded versions are cleaned up.
class TerminaInstaller {
 public:
  TerminaInstaller();
  ~TerminaInstaller();

  TerminaInstaller(const TerminaInstaller&) = delete;
  TerminaInstaller& operator=(const TerminaInstaller&) = delete;

  enum class InstallResult {
    // The install succeeded.
    Success,
    // The install failed for an unspecified reason.
    Failure,
    // The install failed because it needed to download an image and the device
    // is offline.
    Offline,
    // The device must be updated before termina can be installed.
    NeedUpdate,
  };

  // This is really a bool, but std::vector<bool> has weird properties that stop
  // us from using bool here.
  using UninstallResult = int;

  // Ensure that termina is installed. This will also attempt to remove any
  // other instances of termina that may be installed, but will not block on or
  // check the result of this.
  //
  // |is_initial_install| should be set to true when this is called from the
  // crostini installer, and false otherwise. This allows us to fall back to the
  // cros-termina component if transitioning to DLC fails while still requiring
  // it for new installs. In the future this may also allow us to force the DLC
  // to be installed even on tethered connections during the install, as in this
  // case we can expect the user already knows we will download things.
  void Install(base::OnceCallback<void(InstallResult)> callback,
               bool is_initial_install);

  // Remove termina entirely. This will also attempt to remove any
  // other instances of termina that may be installed.
  void Uninstall(base::OnceCallback<void(bool)> callback);

  // Get a path to the install location of termina. You must call Install and
  // get a Success response back before calling this method.
  base::FilePath GetInstallLocation();

  // Get the id of the installed DLC, or nullopt if DLC is not being used.
  absl::optional<std::string> GetDlcId();

  // Attempt to cancel a pending install. Note that neither DLC service nor
  // component updater support this, but we have some retry logic that can be
  // aborted.
  void Cancel();

 private:
  void InstallDlc(base::OnceCallback<void(InstallResult)> callback,
                  bool is_initial_install);
  void OnInstallDlc(base::OnceCallback<void(InstallResult)> callback,
                    bool is_initial_install,
                    const chromeos::DlcserviceClient::InstallResult& result);

  void InstallComponent(base::OnceCallback<void(InstallResult)> callback);
  void OnInstallComponent(base::OnceCallback<void(InstallResult)> callback,
                          bool is_update_checked,
                          component_updater::CrOSComponentManager::Error error,
                          const base::FilePath& path);
  void ReinstallComponent(base::OnceCallback<void(InstallResult)> callback);
  void OnReinstallComponent(
      base::OnceCallback<void(InstallResult)> callback,
      bool is_update_checked,
      component_updater::CrOSComponentManager::Error error,
      const base::FilePath& path);

  void RemoveComponentIfPresent(base::OnceCallback<void()> callback,
                                UninstallResult* result);
  void RemoveDlcIfPresent(base::OnceCallback<void()> callback,
                          UninstallResult* result);
  void RemoveDlc(base::OnceCallback<void()> callback, UninstallResult* result);

  void OnUninstallFinished(base::OnceCallback<void(bool)> callback,
                           std::vector<UninstallResult> partial_results);

  // Do we need to try to force a component update? We want to do this at most
  // once per session, so this is initialized to true and unset whenever we
  // successfully check for an update or we need to install a new major version.
  bool component_update_check_needed_{true};

  absl::optional<base::FilePath> termina_location_{absl::nullopt};
  absl::optional<std::string> dlc_id_{};
  base::WeakPtrFactory<TerminaInstaller> weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_TERMINA_INSTALLER_H_
