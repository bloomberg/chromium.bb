// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_util_proxy.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"

namespace {

using base::Bind;
using base::Callback;
using base::Owned;
using base::PlatformFileError;
using base::Unretained;

typedef fileapi::FileSystemFileUtilProxy Proxy;

class EnsureFileExistsHelper {
 public:
  EnsureFileExistsHelper() : error_(base::PLATFORM_FILE_OK) {}

  void RunWork(const Proxy::EnsureFileExistsTask& task) {
    error_ = task.Run(&created_);
  }

  void Reply(const Proxy::EnsureFileExistsCallback& callback) {
    if (!callback.is_null()) {
      callback.Run(error_, created_);
    }
  }

 private:
  base::PlatformFileError error_;
  bool created_;
  DISALLOW_COPY_AND_ASSIGN(EnsureFileExistsHelper);
};

class GetFileInfoHelper {
 public:
  GetFileInfoHelper() : error_(base::PLATFORM_FILE_OK) {}

  void RunWork(const Proxy::GetFileInfoTask& task) {
    error_ = task.Run(&file_info_, &platform_path_);
  }

  void Reply(const Proxy::GetFileInfoCallback& callback) {
    if (!callback.is_null()) {
      callback.Run(error_, file_info_, platform_path_);
    }
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

  void RunWork(const Proxy::ReadDirectoryTask& task) {
    error_ = task.Run(&entries_);
  }

  void Reply(const Proxy::ReadDirectoryCallback& callback) {
    if (!callback.is_null()) {
      callback.Run(error_, entries_);
    }
  }

 private:
  base::PlatformFileError error_;
  std::vector<Proxy::Entry> entries_;
  DISALLOW_COPY_AND_ASSIGN(ReadDirectoryHelper);
};

}  // namespace

namespace fileapi {

// static
bool FileSystemFileUtilProxy::RelayEnsureFileExists(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const EnsureFileExistsTask& task,
    const EnsureFileExistsCallback& callback) {
  EnsureFileExistsHelper* helper = new EnsureFileExistsHelper;
  return message_loop_proxy->PostTaskAndReply(
        FROM_HERE,
        Bind(&EnsureFileExistsHelper::RunWork, Unretained(helper), task),
        Bind(&EnsureFileExistsHelper::Reply, Owned(helper), callback));
}

// static
bool FileSystemFileUtilProxy::RelayGetFileInfo(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const GetFileInfoTask& task,
    const GetFileInfoCallback& callback) {
  GetFileInfoHelper* helper = new GetFileInfoHelper;
  return message_loop_proxy->PostTaskAndReply(
        FROM_HERE,
        Bind(&GetFileInfoHelper::RunWork, Unretained(helper), task),
        Bind(&GetFileInfoHelper::Reply, Owned(helper), callback));
}

// static
bool FileSystemFileUtilProxy::RelayReadDirectory(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const ReadDirectoryTask& task,
    const ReadDirectoryCallback& callback) {
  ReadDirectoryHelper* helper = new ReadDirectoryHelper;
  return message_loop_proxy->PostTaskAndReply(
        FROM_HERE,
        Bind(&ReadDirectoryHelper::RunWork, Unretained(helper), task),
        Bind(&ReadDirectoryHelper::Reply, Owned(helper), callback));
}

}  // namespace fileapi
