// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_DELEGATE_H_
#define WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_DELEGATE_H_

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner_helpers.h"
#include "webkit/fileapi/file_system_file_util.h"

namespace base {
struct PlatformFileInfo;
class SequencedTaskRunner;
class Time;
}

namespace fileapi {

// Delegate for media transfer protocol (MTP) device to perform media device
// isolated file system operations. Class that implements this delegate does
// the actual communication with the MTP device. ScopedMTPDeviceMapEntry class
// manages the lifetime of the delegate via MTPDeviceMapService class.
class MTPDeviceDelegate : public base::SupportsWeakPtr<MTPDeviceDelegate> {
 public:
  // Returns information about the given file path.
  virtual base::PlatformFileError GetFileInfo(
      const FilePath& file_path,
      base::PlatformFileInfo* file_info) = 0;

  // Returns a pointer to a new instance of AbstractFileEnumerator to enumerate
  // the file entries of |root| path. The instance needs to be freed by the
  // caller, and its lifetime should not extend past when the current call
  // returns to the main media task runner thread.
  virtual scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator>
      CreateFileEnumerator(const FilePath& root,
                           bool recursive) = 0;

  // Updates the temporary snapshot file contents given by |local_path| with
  // media file contents given by |device_file_path| and also returns the
  // metadata of the temporary file.
  virtual PlatformFileError CreateSnapshotFile(
      const FilePath& device_file_path,
      const FilePath& local_path,
      base::PlatformFileInfo* file_info) = 0;

  // Returns TaskRunner on which the operation is performed.
  virtual base::SequencedTaskRunner* GetMediaTaskRunner() = 0;

  // Called when the
  // (1) Browser application is in shutdown mode (or)
  // (2) Last extension using this MTP device is destroyed (or)
  // (3) Attached MTP device is removed (or)
  // (4) User revoked the MTP device gallery permission.
  // Ownership of |MTPDeviceDelegate| is handed off to the delegate
  // implementation class by this call. This function should take care of
  // deleting itself on the right thread. This function should cancel all the
  // pending requests before posting any message to delete itself.
  // Called on the IO thread.
  virtual void CancelPendingTasksAndDeleteDelegate() = 0;

  // Called on the IO thread.
  virtual base::WeakPtr<MTPDeviceDelegate> GetAsWeakPtrOnIOThread() = 0;

 protected:
  // Always destruct this object via CancelPendingTasksAndDeleteDelegate().
  virtual ~MTPDeviceDelegate() {}
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_MTP_DEVICE_DELEGATE_H_
