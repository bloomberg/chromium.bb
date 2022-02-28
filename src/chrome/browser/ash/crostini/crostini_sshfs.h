// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SSHFS_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SSHFS_H_

#include <queue>
#include <set>
#include <utility>

#include "ash/components/disks/disk_mount_manager.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"

namespace crostini {

class CrostiniSshfs : ContainerShutdownObserver {
 public:
  explicit CrostiniSshfs(Profile* profile);

  CrostiniSshfs(const CrostiniSshfs&) = delete;
  CrostiniSshfs& operator=(const CrostiniSshfs&) = delete;

  ~CrostiniSshfs() override;
  using MountCrostiniFilesCallback = base::OnceCallback<void(bool succeeded)>;

  // Mounts the user's Crostini home directory so it's accessible from the host.
  // Must be called from the UI thread, no-op if the home directory is already
  // mounted. If this is something running in the background set background to
  // true, if failures are user-visible set it to false. If you're setting
  // base::DoNothing as the callback then background should be true.
  void MountCrostiniFiles(const ContainerId& container_id,
                          MountCrostiniFilesCallback callback,
                          bool background);

  // Unmounts the user's Crostini home directory. Must be called from the UI
  // thread.
  void UnmountCrostiniFiles(const ContainerId& container_id,
                            MountCrostiniFilesCallback callback);

  // ContainerShutdownObserver.
  void OnContainerShutdown(const ContainerId& container_id) override;

  void OnMountEvent(
      chromeos::MountError error_code,
      const ash::disks::DiskMountManager::MountPointInfo& mount_info);

  // Returns true if sshfs is mounted for the specified container, else false.
  bool IsSshfsMounted(const ContainerId& container);

  // Only public so unit tests can reference them without needing to FRIEND_TEST
  // every single test case.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CrostiniSshfsResult {
    kSuccess = 0,
    kNotDefaultContainer = 1,
    kContainerNotRunning = 2,
    kGetSshKeysFailed = 3,
    kGetContainerInfoFailed = 4,
    kMountErrorInternal = 5,
    kMountErrorProgramFailed = 6,
    kMountErrorOther = 7,
    kMaxValue = kMountErrorOther,
  };

 private:
  void SetSshfsMounted(const ContainerId& container, bool mounted);
  void Finish(CrostiniSshfsResult result);

  void OnRemoveSshfsCrostiniVolume(const ContainerId& container_id,
                                   MountCrostiniFilesCallback callback,
                                   base::Time started,
                                   bool success);

  void OnGetContainerSshKeys(bool success,
                             const std::string& container_public_key,
                             const std::string& host_private_key,
                             const std::string& hostname);

  struct InProgressMount {
    std::string source_path;
    ContainerId container_id;
    base::FilePath container_homedir;
    MountCrostiniFilesCallback callback;
    base::Time started;
    bool background;
    InProgressMount(const ContainerId& container,
                    MountCrostiniFilesCallback callback,
                    bool background);
    InProgressMount(InProgressMount&& other) noexcept;
    InProgressMount& operator=(InProgressMount&& other) noexcept;
    ~InProgressMount();
  };
  struct PendingRequest {
    ContainerId container_id;
    MountCrostiniFilesCallback callback;
    bool background;
    PendingRequest(const ContainerId& container_id,
                   MountCrostiniFilesCallback callback,
                   bool background);
    PendingRequest(PendingRequest&& other) noexcept;
    PendingRequest& operator=(PendingRequest&& other) noexcept;
    ~PendingRequest();
  };
  Profile* profile_;

  base::ScopedObservation<CrostiniManager,
                          ContainerShutdownObserver,
                          &CrostiniManager::AddContainerShutdownObserver,
                          &CrostiniManager::RemoveContainerShutdownObserver>
      container_shutdown_observer_{this};

  std::unique_ptr<InProgressMount> in_progress_mount_;

  std::set<ContainerId> sshfs_mounted_;
  std::queue<PendingRequest> pending_requests_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrostiniSshfs> weak_ptr_factory_{this};
};
}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SSHFS_H_
