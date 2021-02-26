// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_installer_impl.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/borealis/borealis_features.h"
#include "chrome/browser/chromeos/borealis/borealis_prefs.h"
#include "chrome/browser/chromeos/borealis/borealis_service.h"
#include "chrome/browser/chromeos/borealis/borealis_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace borealis {

BorealisInstallerImpl::BorealisInstallerImpl(Profile* profile)
    : state_(State::kIdle),
      installing_state_(InstallingState::kInactive),
      profile_(profile),
      weak_ptr_factory_(this) {}

BorealisInstallerImpl::~BorealisInstallerImpl() = default;

bool BorealisInstallerImpl::IsProcessing() {
  return state_ != State::kIdle;
}

void BorealisInstallerImpl::Start() {
  RecordBorealisInstallNumAttemptsHistogram();
  if (!BorealisService::GetForProfile(profile_)->Features().IsAllowed()) {
    LOG(ERROR) << "Installation of Borealis cannot be started because "
               << "Borealis is not allowed.";
    InstallationEnded(BorealisInstallResult::kBorealisNotAllowed);
    return;
  }

  if (IsProcessing()) {
    LOG(ERROR) << "Installation of Borealis is already in progress.";
    InstallationEnded(BorealisInstallResult::kBorealisInstallInProgress);
    return;
  }
  installation_start_tick_ = base::TimeTicks::Now();

  progress_ = 0;
  StartDlcInstallation();
}

void BorealisInstallerImpl::Cancel() {
  if (state_ != State::kIdle) {
    state_ = State::kCancelling;
  }
  for (auto& observer : observers_) {
    observer.OnCancelInitiated();
  }
}

void BorealisInstallerImpl::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BorealisInstallerImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void BorealisInstallerImpl::StartDlcInstallation() {
  state_ = State::kInstalling;
  UpdateInstallingState(InstallingState::kInstallingDlc);

  chromeos::DlcserviceClient::Get()->Install(
      kBorealisDlcName,
      base::BindOnce(&BorealisInstallerImpl::OnDlcInstallationCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &BorealisInstallerImpl::OnDlcInstallationProgressUpdated,
          weak_ptr_factory_.GetWeakPtr()));
}

void BorealisInstallerImpl::InstallationEnded(BorealisInstallResult result) {
  // If another installation is in progress, we don't want to reset any states
  // and interfere with the process. When that process completes, it will reset
  // these states.
  if (result != BorealisInstallResult::kBorealisInstallInProgress) {
    state_ = State::kIdle;
    installing_state_ = InstallingState::kInactive;
  }
  if (result == BorealisInstallResult::kSuccess) {
    profile_->GetPrefs()->SetBoolean(prefs::kBorealisInstalledOnDevice, true);
    RecordBorealisInstallOverallTimeHistogram(base::TimeTicks::Now() -
                                              installation_start_tick_);
  }
  RecordBorealisInstallResultHistogram(result);
  for (auto& observer : observers_) {
    observer.OnInstallationEnded(result);
  }
}

void BorealisInstallerImpl::UpdateProgress(double state_progress) {
  DCHECK_EQ(state_, State::kInstalling);
  if (state_progress < 0 || state_progress > 1) {
    LOG(ERROR) << "Unexpected progress value " << state_progress
               << " in installing state "
               << GetInstallingStateName(installing_state_);
    return;
  }

  double start_range = 0;
  double end_range = 0;
  switch (installing_state_) {
    case InstallingState::kInstallingDlc:
      start_range = 0;
      end_range = 1;
      break;
    default:
      NOTREACHED();
  }

  double new_progress =
      start_range + (end_range - start_range) * state_progress;
  if (new_progress < progress_) {
    LOG(ERROR) << "Progress went backwards from " << progress_ << " to "
               << progress_;
    return;
  }

  progress_ = new_progress;
  for (auto& observer : observers_) {
    observer.OnProgressUpdated(new_progress);
  }
}

void BorealisInstallerImpl::UpdateInstallingState(
    InstallingState installing_state) {
  DCHECK_NE(installing_state, InstallingState::kInactive);
  installing_state_ = installing_state;
  for (auto& observer : observers_) {
    observer.OnStateUpdated(installing_state_);
  }
}

void BorealisInstallerImpl::OnDlcInstallationProgressUpdated(double progress) {
  DCHECK_EQ(installing_state_, InstallingState::kInstallingDlc);
  if (state_ == State::kCancelling)
    return;

  UpdateProgress(progress);
}

void BorealisInstallerImpl::OnDlcInstallationCompleted(
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  DCHECK_EQ(installing_state_, InstallingState::kInstallingDlc);
  if (state_ == State::kCancelling) {
    InstallationEnded(BorealisInstallResult::kCancelled);
    return;
  }

  // If success, continue to the next state.
  if (install_result.error == dlcservice::kErrorNone) {
    InstallationEnded(BorealisInstallResult::kSuccess);
    return;
  }

  // At this point, the Borealis DLC installation has failed.
  BorealisInstallResult result = BorealisInstallResult::kDlcUnknownError;

  // TODO(b/172284265): Handle the case where a device update is required before
  // a DLC can be installed.
  if (install_result.error == dlcservice::kErrorInternal) {
    LOG(ERROR) << "Something went wrong internally with DlcService.";
    result = BorealisInstallResult::kDlcInternalError;
  } else if (install_result.error == dlcservice::kErrorInvalidDlc) {
    LOG(ERROR) << "Borealis DLC is not supported, need to enable Borealis DLC.";
    result = BorealisInstallResult::kDlcUnsupportedError;
  } else if (install_result.error == dlcservice::kErrorBusy) {
    LOG(ERROR)
        << "Borealis DLC is not able to be installed as dlcservice is busy.";
    result = BorealisInstallResult::kDlcBusyError;
  } else if (install_result.error == dlcservice::kErrorNeedReboot) {
    LOG(ERROR)
        << "Device has pending update and needs a reboot to use Borealis DLC.";
    result = BorealisInstallResult::kDlcNeedRebootError;
  } else if (install_result.error == dlcservice::kErrorAllocation) {
    LOG(ERROR) << "Device needs to free space to use Borealis DLC.";
    result = BorealisInstallResult::kDlcNeedSpaceError;
  } else {
    LOG(ERROR) << "Failed to install Borealis DLC: " << install_result.error;
  }

  InstallationEnded(result);
}

}  // namespace borealis
