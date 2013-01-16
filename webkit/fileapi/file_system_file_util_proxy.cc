// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_util_proxy.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_util_helper.h"

namespace fileapi {

using base::Bind;
using base::Callback;
using base::Owned;
using base::PlatformFileError;
using base::Unretained;

namespace {

typedef fileapi::FileSystemFileUtilProxy Proxy;

class EnsureFileExistsHelper {
 public:
  EnsureFileExistsHelper() : error_(base::PLATFORM_FILE_OK), created_(false) {}

  void RunWork(FileSystemFileUtil* file_util,
               FileSystemOperationContext* context,
               const FileSystemURL& url) {
    error_ = file_util->EnsureFileExists(context, url, &created_);
  }

  void Reply(const Proxy::EnsureFileExistsCallback& callback) {
    if (!callback.is_null())
      callback.Run(error_, created_);
  }

 private:
  base::PlatformFileError error_;
  bool created_;
  DISALLOW_COPY_AND_ASSIGN(EnsureFileExistsHelper);
};

class GetFileInfoHelper {
 public:
  GetFileInfoHelper()
      : error_(base::PLATFORM_FILE_OK),
        snapshot_policy_(FileSystemFileUtil::kSnapshotFileUnknown) {}

  void GetFileInfo(FileSystemFileUtil* file_util,
                   FileSystemOperationContext* context,
                   const FileSystemURL& url) {
    error_ = file_util->GetFileInfo(context, url, &file_info_, &platform_path_);
  }

  void CreateSnapshotFile(FileSystemFileUtil* file_util,
                          FileSystemOperationContext* context,
                          const FileSystemURL& url) {
    error_ = file_util->CreateSnapshotFile(
        context, url, &file_info_, &platform_path_, &snapshot_policy_);
  }

  void ReplyFileInfo(const Proxy::GetFileInfoCallback& callback) {
    if (!callback.is_null())
      callback.Run(error_, file_info_, platform_path_);
  }

  void ReplySnapshotFile(const Proxy::SnapshotFileCallback& callback) {
    DCHECK(snapshot_policy_ != FileSystemFileUtil::kSnapshotFileUnknown);
    if (!callback.is_null())
      callback.Run(error_, file_info_, platform_path_, snapshot_policy_);
  }

 private:
  base::PlatformFileError error_;
  base::PlatformFileInfo file_info_;
  FilePath platform_path_;
  FileSystemFileUtil::SnapshotFilePolicy snapshot_policy_;
  DISALLOW_COPY_AND_ASSIGN(GetFileInfoHelper);
};

class ReadDirectoryHelper {
 public:
  ReadDirectoryHelper() : error_(base::PLATFORM_FILE_OK) {}

  void RunWork(FileSystemFileUtil* file_util,
               FileSystemOperationContext* context,
               const FileSystemURL& url) {
    error_ = FileUtilHelper::ReadDirectory(context, file_util, url, &entries_);
  }

  void Reply(const Proxy::ReadDirectoryCallback& callback) {
    if (!callback.is_null())
      callback.Run(error_, entries_, false  /* has_more */);
  }

 private:
  base::PlatformFileError error_;
  std::vector<Proxy::Entry> entries_;
  DISALLOW_COPY_AND_ASSIGN(ReadDirectoryHelper);
};

}  // namespace

// static
bool FileSystemFileUtilProxy::Delete(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemURL& url,
    bool recursive,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileUtilHelper::Delete, context, file_util, url, recursive),
      callback);
}

// static
bool FileSystemFileUtilProxy::CreateOrOpen(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  return base::FileUtilProxy::RelayCreateOrOpen(
      context->task_runner(),
      Bind(&FileSystemFileUtil::CreateOrOpen, Unretained(file_util),
           context, url, file_flags),
      Bind(&FileSystemFileUtil::Close, Unretained(file_util),
           context),
      callback);
}

// static
bool FileSystemFileUtilProxy::Copy(
    FileSystemOperationContext* context,
    FileSystemFileUtil* src_util,
    FileSystemFileUtil* dest_util,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileUtilHelper::Copy,
           context, src_util, dest_util, src_url, dest_url),
      callback);
}

// static
bool FileSystemFileUtilProxy::CopyInForeignFile(
    FileSystemOperationContext* context,
    FileSystemFileUtil* dest_util,
    const FilePath& src_local_disk_file_path,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::CopyInForeignFile, Unretained(dest_util),
           context, src_local_disk_file_path, dest_url),
      callback);
}

// static
bool FileSystemFileUtilProxy::Move(
    FileSystemOperationContext* context,
      FileSystemFileUtil* src_util,
      FileSystemFileUtil* dest_util,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileUtilHelper::Move,
           context, src_util, dest_util, src_url, dest_url),
      callback);
}

// static
bool FileSystemFileUtilProxy::EnsureFileExists(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  EnsureFileExistsHelper* helper = new EnsureFileExistsHelper;
  return context->task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&EnsureFileExistsHelper::RunWork, Unretained(helper),
             file_util, context, url),
        Bind(&EnsureFileExistsHelper::Reply, Owned(helper), callback));
}

// static
bool FileSystemFileUtilProxy::CreateDirectory(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::CreateDirectory, Unretained(file_util),
           context, url, exclusive, recursive),
      callback);
}

// static
bool FileSystemFileUtilProxy::GetFileInfo(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  GetFileInfoHelper* helper = new GetFileInfoHelper;
  return context->task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&GetFileInfoHelper::GetFileInfo, Unretained(helper),
             file_util, context, url),
        Bind(&GetFileInfoHelper::ReplyFileInfo, Owned(helper), callback));
}

// static
bool FileSystemFileUtilProxy::CreateSnapshotFile(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemURL& url,
    const SnapshotFileCallback& callback) {
  GetFileInfoHelper* helper = new GetFileInfoHelper;
  return context->task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&GetFileInfoHelper::CreateSnapshotFile, Unretained(helper),
             file_util, context, url),
        Bind(&GetFileInfoHelper::ReplySnapshotFile, Owned(helper), callback));
}

// static
bool FileSystemFileUtilProxy::ReadDirectory(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  ReadDirectoryHelper* helper = new ReadDirectoryHelper;
  return context->task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&ReadDirectoryHelper::RunWork, Unretained(helper),
             file_util, context, url),
        Bind(&ReadDirectoryHelper::Reply, Owned(helper), callback));
}

// static
bool FileSystemFileUtilProxy::Touch(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::Touch, Unretained(file_util),
           context, url, last_access_time, last_modified_time),
      callback);
}

// static
bool FileSystemFileUtilProxy::Truncate(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::Truncate, Unretained(file_util),
           context, url, length),
      callback);
}

}  // namespace fileapi
