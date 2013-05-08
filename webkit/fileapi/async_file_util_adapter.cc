// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/async_file_util_adapter.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"

using base::Bind;
using base::Callback;
using base::Owned;
using base::PlatformFileError;
using base::Unretained;
using webkit_blob::ShareableFileReference;

namespace fileapi {

namespace {

class EnsureFileExistsHelper {
 public:
  EnsureFileExistsHelper() : error_(base::PLATFORM_FILE_OK), created_(false) {}

  void RunWork(FileSystemFileUtil* file_util,
               FileSystemOperationContext* context,
               const FileSystemURL& url) {
    error_ = file_util->EnsureFileExists(context, url, &created_);
  }

  void Reply(const AsyncFileUtil::EnsureFileExistsCallback& callback) {
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
      : error_(base::PLATFORM_FILE_OK) {}

  void GetFileInfo(FileSystemFileUtil* file_util,
                   FileSystemOperationContext* context,
                   const FileSystemURL& url) {
    error_ = file_util->GetFileInfo(context, url, &file_info_, &platform_path_);
  }

  void CreateSnapshotFile(FileSystemFileUtil* file_util,
                          FileSystemOperationContext* context,
                          const FileSystemURL& url) {
    scoped_file_ = file_util->CreateSnapshotFile(
        context, url, &error_, &file_info_, &platform_path_);
  }

  void ReplyFileInfo(const AsyncFileUtil::GetFileInfoCallback& callback) {
    if (!callback.is_null())
      callback.Run(error_, file_info_, platform_path_);
  }

  void ReplySnapshotFile(
      const AsyncFileUtil::CreateSnapshotFileCallback& callback) {
    if (!callback.is_null())
      callback.Run(error_, file_info_, platform_path_,
                   ShareableFileReference::GetOrCreate(scoped_file_.Pass()));
  }

 private:
  base::PlatformFileError error_;
  base::PlatformFileInfo file_info_;
  base::FilePath platform_path_;
  webkit_blob::ScopedFile scoped_file_;
  DISALLOW_COPY_AND_ASSIGN(GetFileInfoHelper);
};

class ReadDirectoryHelper {
 public:
  ReadDirectoryHelper() : error_(base::PLATFORM_FILE_OK) {}

  void RunWork(FileSystemFileUtil* file_util,
               FileSystemOperationContext* context,
               const FileSystemURL& url) {
    base::PlatformFileInfo file_info;
    base::FilePath platform_path;
    PlatformFileError error = file_util->GetFileInfo(
        context, url, &file_info, &platform_path);
    if (error != base::PLATFORM_FILE_OK) {
      error_ = error;
      return;
    }
    if (!file_info.is_directory) {
      error_ = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
      return;
    }

    scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
        file_util->CreateFileEnumerator(context, url));

    base::FilePath current;
    while (!(current = file_enum->Next()).empty()) {
      AsyncFileUtil::Entry entry;
      entry.is_directory = file_enum->IsDirectory();
      entry.name = VirtualPath::BaseName(current).value();
      entry.size = file_enum->Size();
      entry.last_modified_time = file_enum->LastModifiedTime();
      entries_.push_back(entry);
    }
    error_ = base::PLATFORM_FILE_OK;
  }

  void Reply(const AsyncFileUtil::ReadDirectoryCallback& callback) {
    if (!callback.is_null())
      callback.Run(error_, entries_, false /* has_more */);
  }

 private:
  base::PlatformFileError error_;
  std::vector<AsyncFileUtil::Entry> entries_;
  DISALLOW_COPY_AND_ASSIGN(ReadDirectoryHelper);
};

}  // namespace

AsyncFileUtilAdapter::AsyncFileUtilAdapter(
    FileSystemFileUtil* sync_file_util)
    : sync_file_util_(sync_file_util) {
  DCHECK(sync_file_util_.get());
}

AsyncFileUtilAdapter::~AsyncFileUtilAdapter() {
}

bool AsyncFileUtilAdapter::CreateOrOpen(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  return base::FileUtilProxy::RelayCreateOrOpen(
      context->task_runner(),
      Bind(&FileSystemFileUtil::CreateOrOpen, Unretained(sync_file_util_.get()),
           context, url, file_flags),
      Bind(&FileSystemFileUtil::Close, Unretained(sync_file_util_.get()),
           context),
      callback);
}

bool AsyncFileUtilAdapter::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  EnsureFileExistsHelper* helper = new EnsureFileExistsHelper;
  return context->task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&EnsureFileExistsHelper::RunWork, Unretained(helper),
             sync_file_util_.get(), context, url),
        Bind(&EnsureFileExistsHelper::Reply, Owned(helper), callback));
}

bool AsyncFileUtilAdapter::CreateDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::CreateDirectory,
           Unretained(sync_file_util_.get()),
           context, url, exclusive, recursive),
      callback);
}

bool AsyncFileUtilAdapter::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  GetFileInfoHelper* helper = new GetFileInfoHelper;
  return context->task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&GetFileInfoHelper::GetFileInfo, Unretained(helper),
             sync_file_util_.get(), context, url),
        Bind(&GetFileInfoHelper::ReplyFileInfo, Owned(helper), callback));
}

bool AsyncFileUtilAdapter::ReadDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  ReadDirectoryHelper* helper = new ReadDirectoryHelper;
  return context->task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&ReadDirectoryHelper::RunWork, Unretained(helper),
             sync_file_util_.get(), context, url),
        Bind(&ReadDirectoryHelper::Reply, Owned(helper), callback));
}

bool AsyncFileUtilAdapter::Touch(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::Touch, Unretained(sync_file_util_.get()),
           context, url, last_access_time, last_modified_time),
      callback);
}

bool AsyncFileUtilAdapter::Truncate(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::Truncate, Unretained(sync_file_util_.get()),
           context, url, length),
      callback);
}

bool AsyncFileUtilAdapter::CopyFileLocal(
    FileSystemOperationContext* context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::CopyOrMoveFile,
           Unretained(sync_file_util_.get()),
           context, src_url, dest_url, true /* copy */),
      callback);
}

bool AsyncFileUtilAdapter::MoveFileLocal(
    FileSystemOperationContext* context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::CopyOrMoveFile,
           Unretained(sync_file_util_.get()),
           context, src_url, dest_url, false /* copy */),
      callback);
}

bool AsyncFileUtilAdapter::CopyInForeignFile(
      FileSystemOperationContext* context,
      const base::FilePath& src_file_path,
      const FileSystemURL& dest_url,
      const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::CopyInForeignFile,
           Unretained(sync_file_util_.get()),
           context, src_file_path, dest_url),
      callback);
}

bool AsyncFileUtilAdapter::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::DeleteFile,
           Unretained(sync_file_util_.get()), context, url),
      callback);
}

bool AsyncFileUtilAdapter::DeleteDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  return base::PostTaskAndReplyWithResult(
      context->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::DeleteDirectory,
           Unretained(sync_file_util_.get()),
           context, url),
      callback);
}

bool AsyncFileUtilAdapter::CreateSnapshotFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  GetFileInfoHelper* helper = new GetFileInfoHelper;
  return context->task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&GetFileInfoHelper::CreateSnapshotFile, Unretained(helper),
             sync_file_util_.get(), context, url),
        Bind(&GetFileInfoHelper::ReplySnapshotFile, Owned(helper), callback));
}

}  // namespace fileapi
