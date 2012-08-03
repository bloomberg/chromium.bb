// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_INTERFACE_IMPL_LINUX_H_
#define WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_INTERFACE_IMPL_LINUX_H_

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/media/media_device_interface.h"

namespace base {
struct PlatformFileInfo;
class SequencedTaskRunner;
class Time;
}

namespace fileapi {

struct DefaultMtpDeviceInterfaceImplDeleter;

// Helper class to support MTP device isolated file system operations. This
// class contains platform specific code to communicate with the attached
// MTP device. We instantiate this class per MTP device. MTP device is
// opened for communication during initialization and closed during
// destruction.
class MtpDeviceInterfaceImplLinux
    : public MediaDeviceInterface,
      public base::RefCountedThreadSafe<MtpDeviceInterfaceImplLinux,
                                        DefaultMtpDeviceInterfaceImplDeleter> {
 public:
  MtpDeviceInterfaceImplLinux(
      const FilePath::StringType& device_id,
      base::SequencedTaskRunner* media_task_runner);

  // MediaDeviceInterface methods.
  virtual base::PlatformFileError GetFileInfo(
      const FilePath& file_path,
      base::PlatformFileInfo* file_info) OVERRIDE;
  virtual FileSystemFileUtil::AbstractFileEnumerator* CreateFileEnumerator(
      const FilePath& root,
      bool recursive) OVERRIDE;
  virtual base::PlatformFileError Touch(
      const FilePath& file_path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) OVERRIDE;
  virtual bool PathExists(const FilePath& file_path) OVERRIDE;
  virtual bool DirectoryExists(const FilePath& file_path) OVERRIDE;
  virtual bool IsDirectoryEmpty(const FilePath& file_path) OVERRIDE;
  virtual base::PlatformFileError CreateSnapshotFile(
      const FilePath& device_file_path,
      const FilePath& local_path,
      base::PlatformFileInfo* file_info) OVERRIDE;

 private:
  friend struct DefaultMtpDeviceInterfaceImplDeleter;
  friend class base::DeleteHelper<MtpDeviceInterfaceImplLinux>;
  friend class base::RefCountedThreadSafe<MtpDeviceInterfaceImplLinux,
                                          DefaultMtpDeviceInterfaceImplDeleter>;

  virtual ~MtpDeviceInterfaceImplLinux();
  void DeleteOnCorrectThread() const;

  // Device initialization should be done in |media_task_runner_|.
  bool LazyInit();

  // Mtp device id.
  FilePath::StringType device_id_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MtpDeviceInterfaceImplLinux);
};

struct DefaultMtpDeviceInterfaceImplDeleter {
  static void Destruct(const MtpDeviceInterfaceImplLinux* device_impl) {
    device_impl->DeleteOnCorrectThread();
  }
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_INTERFACE_IMPL_LINUX_H_
