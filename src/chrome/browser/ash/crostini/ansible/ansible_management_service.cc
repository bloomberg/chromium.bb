// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service_factory.h"
#include "chrome/browser/ash/crostini/crostini_manager_factory.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"

namespace crostini {

const char kCrostiniDefaultAnsibleVersion[] =
    "ansible;2.2.1.0-2+deb9u1;all;debian-stable-main";

namespace {

ash::CiceroneClient* GetCiceroneClient() {
  return ash::CiceroneClient::Get();
}

}  // namespace

AnsibleConfiguration::AnsibleConfiguration(
    std::string playbook,
    base::FilePath path,
    base::OnceCallback<void(bool success)> callback)
    : playbook(playbook), path(path), callback(std::move(callback)) {}
AnsibleConfiguration::AnsibleConfiguration(
    base::FilePath path,
    base::OnceCallback<void(bool success)> callback)
    : AnsibleConfiguration("", path, std::move(callback)) {}

AnsibleConfiguration::~AnsibleConfiguration() {}

AnsibleManagementService* AnsibleManagementService::GetForProfile(
    Profile* profile) {
  return AnsibleManagementServiceFactory::GetForProfile(profile);
}

AnsibleManagementService::AnsibleManagementService(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}
AnsibleManagementService::~AnsibleManagementService() = default;

void AnsibleManagementService::ConfigureContainer(
    const ContainerId& container_id,
    base::FilePath playbook_path,
    base::OnceCallback<void(bool success)> callback) {
  if (configuration_tasks_.count(container_id) > 0) {
    LOG(ERROR) << "Attempting to configure a container which is already being "
                  "configured";
    std::move(callback).Run(false);
    return;
  }
  if (container_id == ContainerId::GetDefault() &&
      !ShouldConfigureDefaultContainer(profile_)) {
    LOG(ERROR) << "Trying to configure default Crostini container when it "
               << "should not be configured";
    std::move(callback).Run(false);
    return;
  }
  // Add ourselves as an observer if we aren't already awaiting.
  if (configuration_tasks_.empty()) {
    CrostiniManager::GetForProfile(profile_)
        ->AddLinuxPackageOperationProgressObserver(this);
  }
  configuration_tasks_.emplace(
      std::make_pair(container_id, std::make_unique<AnsibleConfiguration>(
                                       playbook_path, std::move(callback))));

  // Popup dialog is shown in case Crostini has already been installed.
  if (!CrostiniManager::GetForProfile(profile_)->GetCrostiniDialogStatus(
          DialogType::INSTALLER))
    ShowCrostiniAnsibleSoftwareConfigView(profile_);

  for (auto& observer : observers_) {
    observer.OnAnsibleSoftwareConfigurationStarted(container_id);
  }
  CrostiniManager::GetForProfile(profile_)->InstallLinuxPackageFromApt(
      container_id, kCrostiniDefaultAnsibleVersion,
      base::BindOnce(&AnsibleManagementService::OnInstallAnsibleInContainer,
                     weak_ptr_factory_.GetWeakPtr(), container_id));
}

void AnsibleManagementService::OnInstallAnsibleInContainer(
    const ContainerId& container_id,
    CrostiniResult result) {
  if (result == CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED) {
    LOG(ERROR) << "Ansible installation failed";
    OnConfigurationFinished(container_id, false);
    return;
  }

  DCHECK_NE(result, CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE);

  DCHECK_EQ(result, CrostiniResult::SUCCESS);
  VLOG(1) << "Ansible installation has been started successfully";
  // Waiting for Ansible installation progress being reported.
  for (auto& observer : observers_) {
    observer.OnAnsibleSoftwareInstall(container_id);
  }
}

void AnsibleManagementService::OnInstallLinuxPackageProgress(
    const ContainerId& container_id,
    InstallLinuxPackageProgressStatus status,
    int progress_percent,
    const std::string& error_message) {
  switch (status) {
    case InstallLinuxPackageProgressStatus::SUCCEEDED: {
      GetAnsiblePlaybookToApply(container_id);
      return;
    }
    case InstallLinuxPackageProgressStatus::FAILED:
      LOG(ERROR) << "Ansible installation failed";
      OnConfigurationFinished(container_id, false);
      return;
    // TODO(okalitova): Report Ansible downloading/installation progress.
    case InstallLinuxPackageProgressStatus::DOWNLOADING:
      VLOG(1) << "Ansible downloading progress: " << progress_percent << "%";
      return;
    case InstallLinuxPackageProgressStatus::INSTALLING:
      VLOG(1) << "Ansible installing progress: " << progress_percent << "%";
      return;
    default:
      NOTREACHED();
  }
}

void AnsibleManagementService::GetAnsiblePlaybookToApply(
    const ContainerId& container_id) {
  DCHECK_GT(configuration_tasks_.count(container_id), 0);
  const base::FilePath& ansible_playbook_file_path =
      configuration_tasks_[container_id]->path;
  bool success = base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(base::ReadFileToString, ansible_playbook_file_path,
                     &configuration_tasks_[container_id]->playbook),
      base::BindOnce(&AnsibleManagementService::OnAnsiblePlaybookRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), container_id));
  if (!success) {
    LOG(ERROR) << "Failed to post task to retrieve Ansible playbook content";
    OnConfigurationFinished(container_id, false);
  }
}

void AnsibleManagementService::OnAnsiblePlaybookRetrieved(
    const ContainerId& container_id,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to retrieve Ansible playbook content";
    OnConfigurationFinished(container_id, false);
    return;
  }

  ApplyAnsiblePlaybook(container_id);
}

void AnsibleManagementService::ApplyAnsiblePlaybook(
    const ContainerId& container_id) {
  DCHECK_GT(configuration_tasks_.count(container_id), 0);
  if (!GetCiceroneClient()->IsApplyAnsiblePlaybookProgressSignalConnected()) {
    // Technically we could still start the application, but we wouldn't be able
    // to detect when the application completes, successfully or otherwise.
    LOG(ERROR)
        << "Attempted to apply playbook when progress signal not connected.";
    OnConfigurationFinished(container_id, false);
    return;
  }

  vm_tools::cicerone::ApplyAnsiblePlaybookRequest request;
  request.set_owner_id(CryptohomeIdForProfile(profile_));
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_playbook(configuration_tasks_[container_id]->playbook);

  GetCiceroneClient()->ApplyAnsiblePlaybook(
      std::move(request),
      base::BindOnce(&AnsibleManagementService::OnApplyAnsiblePlaybook,
                     weak_ptr_factory_.GetWeakPtr(), container_id));
}

void AnsibleManagementService::OnApplyAnsiblePlaybook(
    const ContainerId& container_id,
    absl::optional<vm_tools::cicerone::ApplyAnsiblePlaybookResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to apply Ansible playbook. Empty response.";
    OnConfigurationFinished(container_id, false);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::FAILED) {
    LOG(ERROR) << "Failed to apply Ansible playbook: "
               << response->failure_reason();
    OnConfigurationFinished(container_id, false);
    return;
  }

  VLOG(1) << "Ansible playbook application has been started successfully";
  // Waiting for Ansible playbook application progress being reported.
  // TODO(https://crbug.com/1043060): Add a timeout after which we stop waiting.
  for (auto& observer : observers_) {
    observer.OnApplyAnsiblePlaybook(container_id);
  }
}

void AnsibleManagementService::OnApplyAnsiblePlaybookProgress(
    const vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal& signal) {
  ContainerId container_id(signal.vm_name(), signal.container_name());
  switch (signal.status()) {
    case vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::SUCCEEDED:
      OnConfigurationFinished(container_id, true);
      break;
    case vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::FAILED:
      LOG(ERROR) << "Ansible playbook application has failed with reason:\n"
                 << signal.failure_details();
      OnConfigurationFinished(container_id, false);
      break;
    case vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::IN_PROGRESS:
      // TODO(okalitova): Report Ansible playbook application progress.
      break;
    default:
      NOTREACHED();
  }
}

void AnsibleManagementService::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void AnsibleManagementService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AnsibleManagementService::OnUninstallPackageProgress(
    const ContainerId& container_id,
    UninstallPackageProgressStatus status,
    int progress_percent) {
  NOTIMPLEMENTED();
}

void AnsibleManagementService::OnConfigurationFinished(
    const ContainerId& container_id,
    bool success) {
  DCHECK_GT(configuration_tasks_.count(container_id), 0);
  if (success && container_id == ContainerId::GetDefault()) {
    profile_->GetPrefs()->SetBoolean(prefs::kCrostiniDefaultContainerConfigured,
                                     true);
  }
  for (auto& observer : observers_) {
    observer.OnAnsibleSoftwareConfigurationFinished(container_id, success);
  }
  auto callback = std::move(configuration_tasks_[container_id]->callback);
  configuration_tasks_.erase(configuration_tasks_.find(container_id));

  // Clean up our observer if no more packages are awaiting this.
  if (configuration_tasks_.empty()) {
    CrostiniManager::GetForProfile(profile_)
        ->RemoveLinuxPackageOperationProgressObserver(this);
  }

  std::move(callback).Run(success);
}

}  // namespace crostini
