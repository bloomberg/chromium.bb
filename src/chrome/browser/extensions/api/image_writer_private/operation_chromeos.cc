// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "ash/components/disks/disk.h"
#include "ash/components/disks/disk_mount_manager.h"
#include "base/bind.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_burner/image_burner_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace image_writer {

using ::ash::disks::DiskMountManager;
using chromeos::ImageBurnerClient;
using content::BrowserThread;

namespace {

void ClearImageBurner() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ClearImageBurner));
    return;
  }

  chromeos::DBusThreadManager::Get()->
      GetImageBurnerClient()->
      ResetEventHandlers();
}

}  // namespace

void Operation::Write(base::OnceClosure continuation) {
  DCHECK(IsRunningInCorrectSequence());
  SetStage(image_writer_api::STAGE_WRITE);

  // Note this has to be run on the FILE thread to avoid concurrent access.
  AddCleanUpFunction(base::BindOnce(&ClearImageBurner));

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Operation::UnmountVolumes, this,
                                std::move(continuation)));
}

void Operation::VerifyWrite(base::OnceClosure continuation) {
  DCHECK(IsRunningInCorrectSequence());

  // No verification is available in Chrome OS currently.
  std::move(continuation).Run();
}

void Operation::UnmountVolumes(base::OnceClosure continuation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DiskMountManager::GetInstance()->UnmountDeviceRecursively(
      device_path_.value(), base::BindOnce(&Operation::UnmountVolumesCallback,
                                           this, std::move(continuation)));
}

void Operation::UnmountVolumesCallback(base::OnceClosure continuation,
                                       chromeos::MountError error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error_code != chromeos::MOUNT_ERROR_NONE) {
    LOG(ERROR) << "Volume unmounting failed with error code " << error_code;
    PostTask(
        base::BindOnce(&Operation::Error, this, error::kUnmountVolumesError));
    return;
  }

  const DiskMountManager::DiskMap& disks =
      DiskMountManager::GetInstance()->disks();
  DiskMountManager::DiskMap::const_iterator iter =
      disks.find(device_path_.value());

  if (iter == disks.end()) {
    LOG(ERROR) << "Disk not found in disk list after unmounting volumes.";
    PostTask(
        base::BindOnce(&Operation::Error, this, error::kUnmountVolumesError));
    return;
  }

  StartWriteOnUIThread(iter->second->file_path(), std::move(continuation));
}

void Operation::StartWriteOnUIThread(const std::string& target_path,
                                     base::OnceClosure continuation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(haven): Image Burner cannot handle multiple burns. crbug.com/373575
  ImageBurnerClient* burner =
      chromeos::DBusThreadManager::Get()->GetImageBurnerClient();

  burner->SetEventHandlers(
      base::BindOnce(&Operation::OnBurnFinished, this, std::move(continuation)),
      base::BindRepeating(&Operation::OnBurnProgress, this));

  burner->BurnImage(image_path_.value(), target_path,
                    base::BindOnce(&Operation::OnBurnError, this));
}

void Operation::OnBurnFinished(base::OnceClosure continuation,
                               const std::string& target_path,
                               bool success,
                               const std::string& error) {
  if (success) {
    PostTask(base::BindOnce(&Operation::SetProgress, this, kProgressComplete));
    PostTask(std::move(continuation));
  } else {
    DLOG(ERROR) << "Error encountered while burning: " << error;
    PostTask(base::BindOnce(&Operation::Error, this,
                            error::kChromeOSImageBurnerError));
  }
}

void Operation::OnBurnProgress(const std::string& target_path,
                               int64_t num_bytes_burnt,
                               int64_t total_size) {
  int progress = kProgressComplete * num_bytes_burnt / total_size;
  PostTask(base::BindOnce(&Operation::SetProgress, this, progress));
}

void Operation::OnBurnError() {
  PostTask(base::BindOnce(&Operation::Error, this,
                          error::kChromeOSImageBurnerError));
}

}  // namespace image_writer
}  // namespace extensions
