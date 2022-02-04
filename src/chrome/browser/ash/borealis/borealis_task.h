// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_TASK_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_TASK_H_

#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_launch_watcher.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

namespace borealis {

class BorealisCapabilities;
class BorealisContext;

// BorealisTasks are collections of operations that are run by the
// Borealis Context Manager.
class BorealisTask {
 public:
  // Callback to be run when the task completes. The |result| should reflect
  // if the task succeeded with kSuccess and an empty string. If it fails, a
  // different result should be used, and an error string provided.
  using CompletionResultCallback =
      base::OnceCallback<void(BorealisStartupResult, std::string)>;
  explicit BorealisTask(std::string name);
  BorealisTask(const BorealisTask&) = delete;
  BorealisTask& operator=(const BorealisTask&) = delete;
  virtual ~BorealisTask();

  void Run(BorealisContext* context, CompletionResultCallback callback);

 protected:
  virtual void RunInternal(BorealisContext* context) = 0;

  void Complete(BorealisStartupResult result, std::string message);

 private:
  std::string name_;
  base::Time start_time_;
  CompletionResultCallback callback_;
};

// Mounts the Borealis DLC.
class MountDlc : public BorealisTask {
 public:
  MountDlc();
  ~MountDlc() override;
  void RunInternal(BorealisContext* context) override;

 private:
  void OnMountDlc(
      BorealisContext* context,
      const chromeos::DlcserviceClient::InstallResult& install_result);
  base::WeakPtrFactory<MountDlc> weak_factory_{this};
};

// Creates a disk image for the Borealis VM.
class CreateDiskImage : public BorealisTask {
 public:
  CreateDiskImage();
  ~CreateDiskImage() override;
  void RunInternal(BorealisContext* context) override;

 private:
  void OnCreateDiskImage(
      BorealisContext* context,
      absl::optional<vm_tools::concierge::CreateDiskImageResponse> response);
  base::WeakPtrFactory<CreateDiskImage> weak_factory_{this};
};

// Requests a wayland server from Exo for use by the borealis VM.
class RequestWaylandServer : public BorealisTask {
 public:
  RequestWaylandServer();
  ~RequestWaylandServer() override;

  // BorealisTask overrides:
  void RunInternal(BorealisContext* context) override;

 private:
  void OnServerRequested(BorealisContext* context,
                         BorealisCapabilities* capabilities,
                         const base::FilePath& server_path);
  base::WeakPtrFactory<RequestWaylandServer> weak_factory_{this};
};

// Instructs Concierge to start the Borealis VM.
class StartBorealisVm : public BorealisTask {
 public:
  StartBorealisVm();
  ~StartBorealisVm() override;
  void RunInternal(BorealisContext* context) override;

 private:
  void StartBorealisWithExternalDisk(BorealisContext* context,
                                     absl::optional<base::File> external_disk);
  void OnStartBorealisVm(
      BorealisContext* context,
      absl::optional<vm_tools::concierge::StartVmResponse> response);
  base::WeakPtrFactory<StartBorealisVm> weak_factory_{this};
};

// Waits for the startup daemon to signal completion.
class AwaitBorealisStartup : public BorealisTask {
 public:
  AwaitBorealisStartup(Profile* profile, std::string vm_name);
  ~AwaitBorealisStartup() override;
  void RunInternal(BorealisContext* context) override;
  BorealisLaunchWatcher& GetWatcherForTesting();

 private:
  void OnAwaitBorealisStartup(BorealisContext* context,
                              absl::optional<std::string> container);
  BorealisLaunchWatcher watcher_;
  base::WeakPtrFactory<AwaitBorealisStartup> weak_factory_{this};
};

// Updates the value of some chrome://flags in the VM.
class UpdateChromeFlags : public BorealisTask {
 public:
  explicit UpdateChromeFlags(Profile* profile);
  ~UpdateChromeFlags() override;
  void RunInternal(BorealisContext* context) override;

 private:
  void OnFlagsUpdated(BorealisContext* context, std::string error);

  Profile* const profile_;
  base::WeakPtrFactory<UpdateChromeFlags> weak_factory_{this};
};

// Checks the size of the disk and adjusts it if necessary.
class SyncBorealisDisk : public BorealisTask {
 public:
  SyncBorealisDisk();
  ~SyncBorealisDisk() override;
  void RunInternal(BorealisContext* context) override;

 private:
  void OnSyncBorealisDisk(
      BorealisContext* context,
      Expected<BorealisSyncDiskSizeResult,
               Described<BorealisSyncDiskSizeResult>> result);
  base::WeakPtrFactory<SyncBorealisDisk> weak_factory_{this};
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_TASK_H_
