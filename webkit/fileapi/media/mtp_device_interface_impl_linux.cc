// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/media/mtp_device_interface_impl_linux.h"

#include "base/sequenced_task_runner.h"

using base::PlatformFileError;
using base::PlatformFileInfo;
using base::Time;

namespace fileapi {

MtpDeviceInterfaceImplLinux::MtpDeviceInterfaceImplLinux(
    const FilePath::StringType& device_id,
    base::SequencedTaskRunner* media_task_runner)
    : device_id_(device_id),
      media_task_runner_(media_task_runner) {
  // Initialize the device in LazyInit() function.
  // This object is constructed on IO thread.
}

MtpDeviceInterfaceImplLinux::~MtpDeviceInterfaceImplLinux() {
  // Do the clean up in DeleteOnCorrectThread() function.
  // This object must be destructed on media_task_runner.
}

bool MtpDeviceInterfaceImplLinux::LazyInit() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());

  // TODO(kmadhusu, thestig): Open the device for communication. If is already
  // opened, return.
  return true;
}

void MtpDeviceInterfaceImplLinux::DeleteOnCorrectThread() const {
  if (!media_task_runner_->RunsTasksOnCurrentThread()) {
    media_task_runner_->DeleteSoon(FROM_HERE, this);
    return;
  }
  delete this;
}

PlatformFileError MtpDeviceInterfaceImplLinux::GetFileInfo(
    const FilePath& file_path,
    PlatformFileInfo* file_info) {
  if (!LazyInit())
    return base::PLATFORM_FILE_ERROR_SECURITY;

  NOTIMPLEMENTED();
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

FileSystemFileUtil::AbstractFileEnumerator*
MtpDeviceInterfaceImplLinux::CreateFileEnumerator(
        const FilePath& root,
        bool recursive) {
  if (!LazyInit())
    return new FileSystemFileUtil::EmptyFileEnumerator();

  NOTIMPLEMENTED();
  return new FileSystemFileUtil::EmptyFileEnumerator();
}

PlatformFileError MtpDeviceInterfaceImplLinux::Touch(
    const FilePath& file_path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  if (!LazyInit())
    return base::PLATFORM_FILE_ERROR_SECURITY;

  NOTIMPLEMENTED();
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

bool MtpDeviceInterfaceImplLinux::PathExists(const FilePath& file_path) {
  if (!LazyInit())
    return false;

  NOTIMPLEMENTED();
  return false;
}

bool MtpDeviceInterfaceImplLinux::DirectoryExists(const FilePath& file_path) {
  if (!LazyInit())
    return false;

  NOTIMPLEMENTED();
  return false;
}

bool MtpDeviceInterfaceImplLinux::IsDirectoryEmpty(const FilePath& file_path) {
  if (!LazyInit())
    return false;

  NOTIMPLEMENTED();
  return true;
}

PlatformFileError MtpDeviceInterfaceImplLinux::CreateSnapshotFile(
    const FilePath& device_file_path,
    const FilePath& local_path,
    PlatformFileInfo* file_info) {
  if (!LazyInit())
    return base::PLATFORM_FILE_ERROR_FAILED;

  // Write the device_file_path data to local_path file and set the file_info
  // accordingly.

  NOTIMPLEMENTED();
  return base::PLATFORM_FILE_ERROR_FAILED;
}

}  // namespace fileapi
