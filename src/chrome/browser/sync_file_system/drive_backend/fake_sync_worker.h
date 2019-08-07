// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_SYNC_WORKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_SYNC_WORKER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_worker_interface.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "net/base/network_change_notifier.h"

class GURL;

namespace base {
class FilePath;
class ListValue;
}

namespace storage {
class FileSystemURL;
}

namespace sync_file_system {

class FileChange;
class SyncFileMetadata;

namespace drive_backend {

class RemoteChangeProcessorOnWorker;
class SyncEngineContext;

class FakeSyncWorker : public SyncWorkerInterface {
 public:
  FakeSyncWorker();
  ~FakeSyncWorker() override;

  // SyncWorkerInterface overrides.
  void Initialize(
      std::unique_ptr<SyncEngineContext> sync_engine_context) override;
  void RegisterOrigin(const GURL& origin,
                      const SyncStatusCallback& callback) override;
  void EnableOrigin(const GURL& origin,
                    const SyncStatusCallback& callback) override;
  void DisableOrigin(const GURL& origin,
                     const SyncStatusCallback& callback) override;
  void UninstallOrigin(const GURL& origin,
                       RemoteFileSyncService::UninstallFlag flag,
                       const SyncStatusCallback& callback) override;
  void ProcessRemoteChange(const SyncFileCallback& callback) override;
  void SetRemoteChangeProcessor(RemoteChangeProcessorOnWorker*
                                    remote_change_processor_on_worker) override;
  RemoteServiceState GetCurrentState() const override;
  void GetOriginStatusMap(
      const RemoteFileSyncService::StatusMapCallback& callback) override;
  std::unique_ptr<base::ListValue> DumpFiles(const GURL& origin) override;
  std::unique_ptr<base::ListValue> DumpDatabase() override;
  void SetSyncEnabled(bool enabled) override;
  void PromoteDemotedChanges(const base::Closure& callback) override;
  void ApplyLocalChange(const FileChange& local_change,
                        const base::FilePath& local_path,
                        const SyncFileMetadata& local_metadata,
                        const storage::FileSystemURL& url,
                        const SyncStatusCallback& callback) override;
  void ActivateService(RemoteServiceState service_state,
                       const std::string& description) override;
  void DeactivateService(const std::string& description) override;
  void DetachFromSequence() override;
  void AddObserver(Observer* observer) override;

 private:
  friend class SyncEngineTest;

  enum AppStatus {
    REGISTERED,
    ENABLED,
    DISABLED,
    UNINSTALLED,
  };
  typedef std::map<GURL, AppStatus> StatusMap;

  void UpdateServiceState(RemoteServiceState state,
                          const std::string& description);

  bool sync_enabled_;
  StatusMap status_map_;

  std::unique_ptr<SyncEngineContext> sync_engine_context_;

  base::ObserverList<Observer>::Unchecked observers_;
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncWorker);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_SYNC_WORKER_H_
