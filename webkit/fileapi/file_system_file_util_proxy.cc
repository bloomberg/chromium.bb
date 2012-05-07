// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_util_proxy.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
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
               const FileSystemPath& path) {
    error_ = file_util->EnsureFileExists(context, path, &created_);
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
  GetFileInfoHelper() : error_(base::PLATFORM_FILE_OK) {}

  void RunWork(FileSystemFileUtil* file_util,
               FileSystemOperationContext* context,
               const FileSystemPath& path) {
    error_ = file_util->GetFileInfo(
        context, path, &file_info_, &platform_path_);
  }

  void Reply(const Proxy::GetFileInfoCallback& callback) {
    if (!callback.is_null())
      callback.Run(error_, file_info_, platform_path_);
  }

 private:
  base::PlatformFileError error_;
  base::PlatformFileInfo file_info_;
  FilePath platform_path_;
  DISALLOW_COPY_AND_ASSIGN(GetFileInfoHelper);
};

class ReadDirectoryHelper {
 public:
  ReadDirectoryHelper() : error_(base::PLATFORM_FILE_OK) {}

  void RunWork(FileSystemFileUtil* file_util,
               FileSystemOperationContext* context,
               const FileSystemPath& path) {
    error_ = FileUtilHelper::ReadDirectory(context, file_util, path, &entries_);
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
    const FileSystemPath& path,
    bool recursive,
    const StatusCallback& callback) {
  return base::FileUtilProxy::RelayFileTask(
      context->file_task_runner(), FROM_HERE,
      Bind(&FileUtilHelper::Delete, context, file_util, path, recursive),
      callback);
}

// static
bool FileSystemFileUtilProxy::CreateOrOpen(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemPath& path,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  return base::FileUtilProxy::RelayCreateOrOpen(
      context->file_task_runner(),
      Bind(&FileSystemFileUtil::CreateOrOpen, Unretained(file_util),
           context, path, file_flags),
      Bind(&FileSystemFileUtil::Close, Unretained(file_util),
           context),
      callback);
}

// static
bool FileSystemFileUtilProxy::Copy(
    FileSystemOperationContext* context,
    FileSystemFileUtil* src_util,
    FileSystemFileUtil* dest_util,
    const FileSystemPath& src_path,
    const FileSystemPath& dest_path,
    const StatusCallback& callback) {
  return base::FileUtilProxy::RelayFileTask(
      context->file_task_runner(), FROM_HERE,
      Bind(&FileUtilHelper::Copy,
           context, src_util, dest_util, src_path, dest_path),
      callback);
}

// static
bool FileSystemFileUtilProxy::Move(
    FileSystemOperationContext* context,
      FileSystemFileUtil* src_util,
      FileSystemFileUtil* dest_util,
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path,
    const StatusCallback& callback) {
  return base::FileUtilProxy::RelayFileTask(
      context->file_task_runner(), FROM_HERE,
      Bind(&FileUtilHelper::Move,
           context, src_util, dest_util, src_path, dest_path),
      callback);
}

// static
bool FileSystemFileUtilProxy::EnsureFileExists(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemPath& path,
    const EnsureFileExistsCallback& callback) {
  EnsureFileExistsHelper* helper = new EnsureFileExistsHelper;
  return context->file_task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&EnsureFileExistsHelper::RunWork, Unretained(helper),
             file_util, context, path),
        Bind(&EnsureFileExistsHelper::Reply, Owned(helper), callback));
}

// static
bool FileSystemFileUtilProxy::CreateDirectory(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemPath& path,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  return base::FileUtilProxy::RelayFileTask(
      context->file_task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::CreateDirectory, Unretained(file_util),
           context, path, exclusive, recursive),
      callback);
}

// static
bool FileSystemFileUtilProxy::GetFileInfo(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemPath& path,
    const GetFileInfoCallback& callback) {
  GetFileInfoHelper* helper = new GetFileInfoHelper;
  return context->file_task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&GetFileInfoHelper::RunWork, Unretained(helper),
             file_util, context, path),
        Bind(&GetFileInfoHelper::Reply, Owned(helper), callback));
}

// static
bool FileSystemFileUtilProxy::ReadDirectory(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemPath& path,
    const ReadDirectoryCallback& callback) {
  ReadDirectoryHelper* helper = new ReadDirectoryHelper;
  return context->file_task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&ReadDirectoryHelper::RunWork, Unretained(helper),
             file_util, context, path),
        Bind(&ReadDirectoryHelper::Reply, Owned(helper), callback));
}

// static
bool FileSystemFileUtilProxy::Touch(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemPath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  return base::FileUtilProxy::RelayFileTask(
      context->file_task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::Touch, Unretained(file_util),
           context, path, last_access_time, last_modified_time),
      callback);
}

// static
bool FileSystemFileUtilProxy::Truncate(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemPath& path,
    int64 length,
    const StatusCallback& callback) {
  return base::FileUtilProxy::RelayFileTask(
      context->file_task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::Truncate, Unretained(file_util),
           context, path, length),
      callback);
}

}  // namespace fileapi
