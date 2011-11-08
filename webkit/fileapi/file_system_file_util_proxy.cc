// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_util_proxy.h"

#include "base/bind.h"
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
            base::MessageLoopProxy::current()),
        error_code_(base::PLATFORM_FILE_OK),
        context_(context),
        file_util_(NULL) {
  }

  bool Start(scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
             const tracked_objects::Location& from_here) {
    return message_loop_proxy->PostTask(
        from_here, base::Bind(&MessageLoopRelay::ProcessOnTargetThread, this));
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

  fileapi::FileSystemFileUtil* file_util() const {
    // TODO(ericu): Support calls that have two different FSFU subclasses.
    return context_.src_file_util();
  }

 private:
  void ProcessOnTargetThread() {
    RunWork();
    origin_message_loop_proxy_->PostTask(
        FROM_HERE, base::Bind(&MessageLoopRelay::RunCallback, this));
  }

  scoped_refptr<base::MessageLoopProxy> origin_message_loop_proxy_;
  base::PlatformFileError error_code_;
  fileapi::FileSystemOperationContext context_;
  fileapi::FileSystemFileUtil* file_util_;
};

class RelayEnsureFileExists : public MessageLoopRelay {
 public:
  RelayEnsureFileExists(
      const fileapi::FileSystemOperationContext& context,
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      const fileapi::FileSystemFileUtilProxy::EnsureFileExistsCallback&
          callback)
      : MessageLoopRelay(context),
        message_loop_proxy_(message_loop_proxy),
        file_path_(file_path),
        callback_(callback),
        created_(false) {
    DCHECK_EQ(false, callback.is_null());
  }

 protected:
  virtual void RunWork() {
    set_error_code(file_util()->EnsureFileExists(
        context(), file_path_, &created_));
  }

  virtual void RunCallback() {
    callback_.Run(error_code(), created_);
  }

 private:
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  FilePath file_path_;
  fileapi::FileSystemFileUtilProxy::EnsureFileExistsCallback callback_;
  bool created_;
};


class RelayGetFileInfo : public MessageLoopRelay {
 public:
  RelayGetFileInfo(
      const fileapi::FileSystemOperationContext& context,
      const FilePath& file_path,
      const fileapi::FileSystemFileUtilProxy::GetFileInfoCallback& callback)
      : MessageLoopRelay(context),
        callback_(callback),
        file_path_(file_path) {
    DCHECK_EQ(false, callback.is_null());
  }

 protected:
  virtual void RunWork() {
    set_error_code(file_util()->GetFileInfo(
        context(), file_path_, &file_info_, &platform_path_));
  }

  virtual void RunCallback() {
    callback_.Run(error_code(), file_info_, platform_path_);
  }

 private:
  fileapi::FileSystemFileUtilProxy::GetFileInfoCallback callback_;
  FilePath file_path_;
  base::PlatformFileInfo file_info_;
  FilePath platform_path_;
};

class RelayReadDirectory : public MessageLoopRelay {
 public:
  RelayReadDirectory(
      const fileapi::FileSystemOperationContext& context,
      const FilePath& file_path,
      const fileapi::FileSystemFileUtilProxy::ReadDirectoryCallback& callback)
      : MessageLoopRelay(context),
        callback_(callback), file_path_(file_path) {
    DCHECK_EQ(false, callback.is_null());
  }

 protected:
  virtual void RunWork() {
    // TODO(kkanetkar): Implement directory read in multiple chunks.
    set_error_code(file_util()->ReadDirectory(
        context(), file_path_, &entries_));
  }

  virtual void RunCallback() {
    callback_.Run(error_code(), entries_);
  }

 private:
  fileapi::FileSystemFileUtilProxy::ReadDirectoryCallback callback_;
  FilePath file_path_;
  std::vector<base::FileUtilProxy::Entry> entries_;
};

bool Start(const tracked_objects::Location& from_here,
           scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
           scoped_refptr<MessageLoopRelay> relay) {
  return relay->Start(message_loop_proxy, from_here);
}

}  // namespace

namespace fileapi {

// static
bool FileSystemFileUtilProxy::EnsureFileExists(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    const EnsureFileExistsCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayEnsureFileExists(
      context, message_loop_proxy, file_path, callback));
}

// static
bool FileSystemFileUtilProxy::GetFileInfo(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    const GetFileInfoCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayGetFileInfo(context,
               file_path, callback));
}

// static
bool FileSystemFileUtilProxy::ReadDirectory(
    const FileSystemOperationContext& context,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    const ReadDirectoryCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayReadDirectory(context,
               file_path, callback));
}

}  // namespace fileapi
