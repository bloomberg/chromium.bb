// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/pumpkin_installer.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "ui/accessibility/accessibility_features.h"

namespace {
constexpr char kPumpkinDlcName[] = "pumpkin";
constexpr char kInstallationMetricName[] =
    "PumpkinInstaller.InstallationSuccess";
constexpr char kPendingDlcRequestError[] =
    "Cannot install Pumpkin, DLC request in progress.";
constexpr char kPumpkinInstalledError[] = "Pumpkin already installed.";
constexpr char kPumpkinInstallingError[] = "Pumpkin already installing.";
}  // namespace

namespace ash {

PumpkinInstaller::PumpkinInstaller() {
  DCHECK(features::IsExperimentalAccessibilityDictationWithPumpkinEnabled());
}

PumpkinInstaller::~PumpkinInstaller() = default;

void PumpkinInstaller::MaybeInstall(InstalledCallback on_installed,
                                    ProgressCallback on_progress,
                                    ErrorCallback on_error) {
  if (pending_dlc_request_) {
    std::move(on_error).Run(kPendingDlcRequestError);
    return;
  }

  on_installed_ = std::move(on_installed);
  on_progress_ = std::move(on_progress);
  on_error_ = std::move(on_error);

  pending_dlc_request_ = true;
  chromeos::DlcserviceClient::Get()->GetDlcState(
      kPumpkinDlcName,
      base::BindOnce(&PumpkinInstaller::MaybeInstallHelper, GetWeakPtr()));
}

void PumpkinInstaller::MaybeInstallHelper(
    const std::string& error,
    const dlcservice::DlcState& dlc_state) {
  pending_dlc_request_ = false;
  if (error != dlcservice::kErrorNone) {
    OnError(error);
    return;
  }

  switch (dlc_state.state()) {
    case dlcservice::DlcState_State_INSTALLING:
      OnError(kPumpkinInstallingError);
      return;
    case dlcservice::DlcState_State_INSTALLED:
      // TODO(akihiroota): If pumpkin is already installed, we should do
      // `std::move(on_installed_).Run(true) to communicate that Pumpkin files
      // are available.
      OnError(kPumpkinInstalledError);
      return;
    default:
      break;
  }

  // Install Pumpkin DLC.
  pending_dlc_request_ = true;
  dlcservice::InstallRequest install_request;
  install_request.set_id(kPumpkinDlcName);
  chromeos::DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&PumpkinInstaller::OnInstalled, GetWeakPtr()),
      base::BindRepeating(&PumpkinInstaller::OnProgress, GetWeakPtr()));
}

void PumpkinInstaller::OnInstalled(
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  pending_dlc_request_ = false;
  base::UmaHistogramBoolean(kInstallationMetricName,
                            install_result.error == dlcservice::kErrorNone);
  if (install_result.error != dlcservice::kErrorNone) {
    OnError(install_result.error);
    return;
  }

  DCHECK(!on_installed_.is_null());
  std::move(on_installed_).Run(true);
}

void PumpkinInstaller::OnProgress(double progress) {
  std::move(on_progress_).Run(progress);
}

void PumpkinInstaller::OnError(const std::string& error) {
  DCHECK(!on_error_.is_null());
  std::move(on_error_).Run(error);
}

base::WeakPtr<PumpkinInstaller> PumpkinInstaller::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
