// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_util_proxy.h"

#include "base/message_loop_proxy.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"

namespace {

class MessageLoopRelay
    : public base::RefCountedThreadSafe<MessageLoopRelay> {
 public:
  // FileSystemOperationContext is passed by value from the IO thread to the
  // File thread.
  explicit MessageLoopRelay(const fileapi::FileSystemOperationContext& context)
      : origin_message_loop_proxy_(
            base::MessageLoopProxy::CreateForCurrentThread()),
        error_code_(base::PLATFORM_FILE_OK),
        context_(context) {
  }

  bool Start(scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
             const tracked_objects::Location& from_here) {
    return message_loop_proxy->PostTask(
        from_here,
        NewRunnableMethod(this, &MessageLoopRelay::ProcessOnTargetThread));
  }

 protected:
  friend class base::RefCountedThreadSafe<MessageLoopRelay>;
  virtual ~MessageLoopRelay() {}

  // Called to perform work on the FILE thread.
  virtual void RunWork() = 0;

  // Called to notify the callback on the origin thread.
  virtual void RunCallback() = 0;

  void set_error_code(base::PlatformFileError error_code) {
    error_code_ = error_code;
  }

  base::PlatformFileError error_code() const {
    return error_code_;
  }

  fileapi::FileSystemOperationContext* context() {
    return &context_;
  }

  fileapi::FileSystemFileUtil* file_system_file_util() const {
    return context_.file_system_file_util();
  }

 private:
  void ProcessOnTargetThread() {
    RunWork();
    origin_message_loop_proxy_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &MessageLoopRelay::RunCallback));
  }

  scoped_refptr<base::MessageLoopProxy> origin_message_loop_proxy_;
  base::PlatformFileError error_code_;
  fileapi::FileSystemOperationContext context_;
  fileapi::FileSystemFileUtil* file_system_file_util_;
};

class RelayCreateOrOpen : public MessageLoopRelay {
 public:
  RelayCreateOrOpen(
      const fileapi::FileSystemOperationContext& context,
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      int file_flags,
      fileapi::FileSystemFileUtilProxy::CreateOrOpenCallback* callback)
      : MessageLoopRelay(context),
        message_loop_proxy_(message_loop_proxy),
        file_path_(file_path),
        file_flags_(file_flags),
        callback_(callback),
        file_handle_(base::kInvalidPlatformFileValue),
        created_(false) {
    DCHECK(callback);
  }

 protected:
  virtual ~RelayCreateOrOpen() {
    if (file_handle_ != base::kInvalidPlatformFileValue)
      fileapi::FileSystemFileUtilProxy::Close(*context(),
          message_loop_proxy_, file_handle_, NULL);
  }

  virtual void RunWork() {
    set_error_code(
        file_system_file_util()->CreateOrOpen(
            context(), file_path_, file_flags_, &file_handle_, &created_));
  }

  virtual void RunCallback() {
    callback_->Run(error_code(), base::PassPlatformFile(&file_handle_),
                   created_);
    delete callback_;
  }

 private:
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  FilePath file_path_;
  int file_flags_;
  fileapi::FileSystemFileUtilProxy::CreateOrOpenCallback* callback_;
  base::PlatformFile file_handle_;
  bool created_;
};

class RelayWithStatusCallback : public MessageLoopRelay {
 public:
  RelayWithStatusCallback(
      const fileapi::FileSystemOperationContext& context,
      fileapi::FileSystemFileUtilProxy::StatusCallback* callback)
      : MessageLoopRelay(context),
        callback_(callback) {
    // It is OK for callback to be NULL.
  }

 protected:
  virtual void RunCallback() {
    // The caller may not have been interested in the result.
    if (callback_) {
      callback_->Run(error_code());
      delete callback_;
    }
  }

 private:
  fileapi::FileSystemFileUtilProxy::StatusCallback* callback_;
};

class RelayClose : public RelayWithStatusCallback {
 public:
  RelayClose(const fileapi::FileSystemOperationContext& context,
             base::PlatformFile file_handle,
             fileapi::FileSystemFileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(context, callback),
        file_handle_(file_handle) {
  }

 protected:
  virtual void RunWork() {
    set_error_code(
        file_system_file_util()->Close(context(), file_handle_));
  }

 private:
  base::PlatformFile file_handle_;
};

class RelayEnsureFileExists : public MessageLoopRelay {
 public:
  RelayEnsureFileExists(
      const fileapi::FileSystemOperationContext& context,
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      fileapi::FileSystemFileUtilProxy::EnsureFileExistsCallback* callback)
      : MessageLoopRelay(context),
        message_loop_proxy_(message_loop_proxy),
        file_path_(file_path),
        callback_(callback),
        created_(false) {
    DCHECK(callback);
  }

 protected:
  virtual void RunWork() {
    set_error_code(
        file_system_file_util()->EnsureFileExists(
            context(), file_path_, &created_));
  }

  virtual void RunCallback() {
    callback_->Run(error_code(), created_);
    delete callback_;
  }

 private:
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  FilePath file_path_;
  fileapi::FileSystemFileUtilProxy::EnsureFileExistsCallback* callback_;
  bool created_;
};

class RelayGetFileInfo : public MessageLoopRelay {
 public:
  RelayGetFileInfo(
      const fileapi::FileSystemOperationContext& context,
      const FilePath& file_path,
      fileapi::FileSystemFileUtilProxy::GetFileInfoCallback* callback)
      : MessageLoopRelay(context),
        callback_(callback),
        file_path_(file_path) {
    DCHECK(callback);
  }

 protected:
  virtual void RunWork() {
    set_error_code(
        file_system_file_util()->GetFileInfo(
            context(), file_path_, &file_info_));
  }

  virtual void RunCallback() {
    callback_->Run(error_code(), file_info_);
    delete callback_;
  }

 private:
  fileapi::FileSystemFileUtilProxy::GetFileInfoCallback* callback_;
  FilePath file_path_;
  base::PlatformFileInfo file_info_;
};

class RelayReadDirectory : public MessageLoopRelay {
 public:
  RelayReadDirectory(
      const fileapi::FileSystemOperationContext& context,
      const FilePath& file_path,
      fileapi::FileSystemFileUtilProxy::ReadDirectoryCallback* callback)
      : MessageLoopRelay(context),
        callback_(callback), file_path_(file_path) {
    DCHECK(callback);
  }

 protected:
  virtual void RunWork() {
    // TODO(kkanetkar): Implement directory read in multiple chunks.
    set_error_code(
        file_system_file_util()->ReadDirectory(
            context(), file_path_, &entries_));
  }

  virtual void RunCallback() {
    callback_->Run(error_code(), entries_);
    delete callback_;
  }

 private:
  fileapi::FileSystemFileUtilProxy::ReadDirectoryCallback* callback_;
  FilePath file_path_;
  std::vector<base::FileUtilProxy::Entry> entries_;
};

class RelayCreateDirectory : public RelayWithStatusCallback {
 public:
  RelayCreateDirectory(
      const fileapi::FileSystemOperationContext& context,
      const FilePath& file_path,
      bool exclusive,
      bool recursive,
      fileapi::FileSystemFileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(context, callback),
        file_path_(file_path),
        exclusive_(exclusive),
        recursive_(recursive) {
  }

 protected:
  virtual void RunWork() {
    set_error_code(
        file_system_file_util()->CreateDirectory(
            context(), file_path_, exclusive_, recursive_));
  }

 private:
  FilePath file_path_;
  bool exclusive_;
  bool recursive_;
};

class RelayCopy : public RelayWithStatusCallback {
 public:
  RelayCopy(const fileapi::FileSystemOperationContext& context,
            const FilePath& src_file_path,
            const FilePath& dest_file_path,
            fileapi::FileSystemFileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(context, callback),
        src_file_path_(src_file_path),
        dest_file_path_(dest_file_path) {
  }

 protected:
  virtual void RunWork() {
    set_error_code(
        file_system_file_util()->Copy(
            context(), src_file_path_, dest_file_path_));
  }

 private:
  FilePath src_file_path_;
  FilePath dest_file_path_;
};

class RelayMove : public RelayWithStatusCallback {
 public:
  RelayMove(const fileapi::FileSystemOperationContext& context,
            const FilePath& src_file_path,
            const FilePath& dest_file_path,
            fileapi::FileSystemFileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(context, callback),
        src_file_path_(src_file_path),
        dest_file_path_(dest_file_path) {
  }

 protected:
  virtual void RunWork() {
    set_error_code(
        file_system_file_util()->Move(
            context(), src_file_path_, dest_file_path_));
  }

 private:
  FilePath src_file_path_;
  FilePath dest_file_path_;
};

class RelayDelete : public RelayWithStatusCallback {
 public:
  RelayDelete(const fileapi::FileSystemOperationContext& context,
              const FilePath& file_path,
              bool recursive,
              fileapi::FileSystemFileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(context, callback),
        file_path_(file_path),
        recursive_(recursive) {
  }

 protected:
  virtual void RunWork() {
    set_error_code(
        file_system_file_util()->Delete(
            context(), file_path_, recursive_));
  }

 private:
  FilePath file_path_;
  bool recursive_;
};

class RelayTouchFilePath : public RelayWithStatusCallback {
 public:
  RelayTouchFilePath(const fileapi::FileSystemOperationContext& context,
                     const FilePath& file_path,
                     const base::Time& last_access_time,
                     const base::Time& last_modified_time,
                     fileapi::FileSystemFileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(context, callback),
        file_path_(file_path),
        last_access_time_(last_access_time),
        last_modified_time_(last_modified_time) {
  }

 protected:
  virtual void RunWork() {
    set_error_code(
        file_system_file_util()->Touch(
            context(), file_path_, last_access_time_, last_modified_time_));
  }

 private:
  FilePath file_path_;
  base::Time last_access_time_;
  base::Time last_modified_time_;
};

class RelayTruncate : public RelayWithStatusCallback {
 public:
  RelayTruncate(const fileapi::FileSystemOperationContext& context,
                const FilePath& file_path,
                int64 length,
                fileapi::FileSystemFileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(context, callback),
        file_path_(file_path),
        length_(length) {
  }

 protected:
  virtual void RunWork() {
    set_error_code(
        file_system_file_util()->Truncate(context(), file_path_, length_));
  }

 private:
  FilePath file_path_;
  int64 length_;
};

bool Start(const tracked_objects::Location& from_here,
           scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
           scoped_refptr<MessageLoopRelay> relay) {
  return relay->Start(message_loop_proxy, from_here);
}

}  // namespace

namespace fileapi {

// static
bool FileSystemFileUtilProxy::CreateOrOpen(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path, int file_flags,
    CreateOrOpenCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayCreateOrOpen(context,
      message_loop_proxy, file_path, file_flags, callback));
}

// static
bool FileSystemFileUtilProxy::Close(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    base::PlatformFile file_handle,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayClose(context, file_handle, callback));
}

// static
bool FileSystemFileUtilProxy::EnsureFileExists(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    EnsureFileExistsCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayEnsureFileExists(
      context, message_loop_proxy, file_path, callback));
}

// Retrieves the information about a file. It is invalid to pass NULL for the
// callback.
bool FileSystemFileUtilProxy::GetFileInfo(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    GetFileInfoCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayGetFileInfo(context,
               file_path, callback));
}

// static
bool FileSystemFileUtilProxy::ReadDirectory(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    ReadDirectoryCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayReadDirectory(context,
               file_path, callback));
}

// static
bool FileSystemFileUtilProxy::CreateDirectory(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    bool exclusive,
    bool recursive,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayCreateDirectory(
      context, file_path, exclusive, recursive, callback));
}

// static
bool FileSystemFileUtilProxy::Copy(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayCopy(context, src_file_path, dest_file_path,
                   callback));
}

// static
bool FileSystemFileUtilProxy::Move(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayMove(context, src_file_path, dest_file_path,
                   callback));
}

// static
bool FileSystemFileUtilProxy::Delete(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    bool recursive,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayDelete(context, file_path, recursive, callback));
}

// static
bool FileSystemFileUtilProxy::Touch(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayTouchFilePath(context, file_path, last_access_time,
                                      last_modified_time, callback));
}

// static
bool FileSystemFileUtilProxy::Truncate(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& path,
    int64 length,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayTruncate(context, path, length, callback));
}

}  // namespace fileapi
