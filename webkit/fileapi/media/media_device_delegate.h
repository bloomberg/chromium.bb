// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_MEDIA_DEVICE_DELEGATE_H_
#define WEBKIT_FILEAPI_MEDIA_MEDIA_DEVICE_DELEGATE_H_

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner_helpers.h"
#include "webkit/fileapi/file_system_file_util.h"

namespace base {
struct PlatformFileInfo;
class SequencedTaskRunner;
class Time;
}

namespace fileapi {

struct MediaDeviceDelegateDeleter;

// Delegate for mtp device to perform media device isolated file system
// operations. Class that implements this delegate does the actual communication
// with the mtp device.
class MediaDeviceDelegate
    : public base::RefCountedThreadSafe<MediaDeviceDelegate,
                                        MediaDeviceDelegateDeleter> {
 public:
  // Returns information about the given file path.
  virtual base::PlatformFileError GetFileInfo(
      const FilePath& file_path,
      base::PlatformFileInfo* file_info) = 0;

  // Returns a pointer to a new instance of AbstractFileEnumerator to enumerate
  // the file entries of |root| path. The instance needs to be freed by the
  // caller, and its lifetime should not extend past when the current call
  // returns to the main media task runner thread.
  virtual FileSystemFileUtil::AbstractFileEnumerator* CreateFileEnumerator(
      const FilePath& root,
      bool recursive) = 0;

  // Updates the temporary snapshot file contents given by |local_path| with
  // media file contents given by |device_file_path| and also returns the
  // metadata of the temporary file.
  virtual PlatformFileError CreateSnapshotFile(
      const FilePath& device_file_path,
      const FilePath& local_path,
      base::PlatformFileInfo* file_info) = 0;

  // Returns TaskRunner on which the operation is performed.
  virtual base::SequencedTaskRunner* media_task_runner() = 0;

  // Helper function to destruct the delegate object on UI thread.
  virtual void DeleteOnCorrectThread() const = 0;

 protected:
  virtual ~MediaDeviceDelegate() {}

 private:
  friend struct MediaDeviceDelegateDeleter;
  friend class base::DeleteHelper<MediaDeviceDelegate>;
  friend class base::RefCountedThreadSafe<MediaDeviceDelegate,
                                          MediaDeviceDelegateDeleter>;
};

struct MediaDeviceDelegateDeleter {
  static void Destruct(const MediaDeviceDelegate* delegate) {
    delegate->DeleteOnCorrectThread();
  }
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_MEDIA_DEVICE_DELEGATE_H_
