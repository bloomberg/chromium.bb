// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace crostini {

struct AnsibleConfiguration {
  AnsibleConfiguration(std::string playbook,
                       base::FilePath path,
                       base::OnceCallback<void(bool success)> callback);
  AnsibleConfiguration(base::FilePath path,
                       base::OnceCallback<void(bool success)> callback);
  ~AnsibleConfiguration();

  std::string playbook;
  base::FilePath path;
  base::OnceCallback<void(bool success)> callback;
};

// TODO(okalitova): Install Ansible from backports repo once this is feasible.
extern const char kCrostiniDefaultAnsibleVersion[];

// AnsibleManagementService is responsible for Crostini default
// container management using Ansible.
class AnsibleManagementService : public KeyedService,
                                 public LinuxPackageOperationProgressObserver {
 public:
  // Observer class for Ansible Management Service related events.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnAnsibleSoftwareConfigurationStarted(
        const ContainerId& container_id) = 0;
    virtual void OnAnsibleSoftwareConfigurationFinished(
        const ContainerId& container_id,
        bool success) = 0;
    virtual void OnAnsibleSoftwareInstall(const ContainerId& container_id) {}
    virtual void OnApplyAnsiblePlaybook(const ContainerId& container_id) {}
  };

  static AnsibleManagementService* GetForProfile(Profile* profile);

  explicit AnsibleManagementService(Profile* profile);

  AnsibleManagementService(const AnsibleManagementService&) = delete;
  AnsibleManagementService& operator=(const AnsibleManagementService&) = delete;

  ~AnsibleManagementService() override;

  // Preconfigures a container with a specified Ansible playbook.
  void ConfigureContainer(const ContainerId& container_id,
                          base::FilePath playbook,
                          base::OnceCallback<void(bool success)> callback);

  // LinuxPackageOperationProgressObserver:
  void OnInstallLinuxPackageProgress(const ContainerId& container_id,
                                     InstallLinuxPackageProgressStatus status,
                                     int progress_percent,
                                     const std::string& error_message) override;
  void OnUninstallPackageProgress(const ContainerId& container_id,
                                  UninstallPackageProgressStatus status,
                                  int progress_percent) override;

  void OnApplyAnsiblePlaybookProgress(
      const vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal& signal);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void OnInstallAnsibleInContainer(const ContainerId& container_id,
                                   CrostiniResult result);
  void GetAnsiblePlaybookToApply(const ContainerId& container_id);
  void OnAnsiblePlaybookRetrieved(const ContainerId& container_id,
                                  bool success);
  void ApplyAnsiblePlaybook(const ContainerId& container_id);
  void OnApplyAnsiblePlaybook(
      const ContainerId& container_id,
      absl::optional<vm_tools::cicerone::ApplyAnsiblePlaybookResponse>
          response);

  // Helper function that runs relevant callback and notifies observers.
  void OnConfigurationFinished(const ContainerId& container_id, bool success);

  Profile* profile_;
  base::ObserverList<Observer> observers_;
  std::map<ContainerId, std::unique_ptr<AnsibleConfiguration>>
      configuration_tasks_;

  base::WeakPtrFactory<AnsibleManagementService> weak_ptr_factory_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_H_
