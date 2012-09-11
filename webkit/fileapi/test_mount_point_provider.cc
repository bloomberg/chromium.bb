// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/test_mount_point_provider.h"

#include <set>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/sequenced_task_runner.h"
#include "webkit/fileapi/file_observers.h"
#include "webkit/fileapi/file_system_file_stream_reader.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/local_file_util.h"
#include "webkit/fileapi/native_file_util.h"
#include "webkit/fileapi/sandbox_file_stream_writer.h"
#include "webkit/quota/quota_manager.h"

namespace fileapi {

// This only supports single origin.
class TestMountPointProvider::QuotaUtil
    : public FileSystemQuotaUtil,
      public FileUpdateObserver {
 public:
  QuotaUtil() : usage_(0) {}
  virtual ~QuotaUtil() {}

  // FileSystemQuotaUtil overrides.
  virtual void GetOriginsForTypeOnFileThread(
      FileSystemType type,
      std::set<GURL>* origins) OVERRIDE {
    NOTREACHED();
  }
  virtual void GetOriginsForHostOnFileThread(
      FileSystemType type,
      const std::string& host,
      std::set<GURL>* origins) OVERRIDE {
    NOTREACHED();
  }
  virtual int64 GetOriginUsageOnFileThread(
      FileSystemContext* context,
      const GURL& origin_url,
      FileSystemType type) OVERRIDE {
    return usage_;
  }
  virtual void InvalidateUsageCache(const GURL& origin_url,
                                    FileSystemType type) OVERRIDE {
    // Do nothing.
  }

  // FileUpdateObserver overrides.
  virtual void OnStartUpdate(const FileSystemURL& url) OVERRIDE {}
  virtual void OnUpdate(const FileSystemURL& url, int64 delta) OVERRIDE {
    usage_ += delta;
  }
  virtual void OnEndUpdate(const FileSystemURL& url) OVERRIDE {}

 private:
  int64 usage_;
};

TestMountPointProvider::TestMountPointProvider(
    base::SequencedTaskRunner* task_runner,
    const FilePath& base_path)
    : base_path_(base_path),
      task_runner_(task_runner),
      local_file_util_(new LocalFileUtil()),
      quota_util_(new QuotaUtil) {
  UpdateObserverList::Source source;
  source.AddObserver(quota_util_.get(), task_runner_);
  observers_ = UpdateObserverList(source);
}

TestMountPointProvider::~TestMountPointProvider() {
}

void TestMountPointProvider::ValidateFileSystemRoot(
    const GURL& origin_url,
    FileSystemType type,
    bool create,
    const ValidateFileSystemCallback& callback) {
  // This won't be called unless we add test code that opens a test
  // filesystem by OpenFileSystem.
  NOTREACHED();
}

FilePath TestMountPointProvider::GetFileSystemRootPathOnFileThread(
    const GURL& origin_url,
    FileSystemType type,
    const FilePath& virtual_path,
    bool create) {
  DCHECK_EQ(kFileSystemTypeTest, type);
  bool success = true;
  if (create)
    success = file_util::CreateDirectory(base_path_);
  else
    success = file_util::DirectoryExists(base_path_);
  return success ? base_path_ : FilePath();
}

bool TestMountPointProvider::IsAccessAllowed(const FileSystemURL& url) {
  return url.type() == fileapi::kFileSystemTypeTest;
}

bool TestMountPointProvider::IsRestrictedFileName(
    const FilePath& filename) const {
  return false;
}

FileSystemFileUtil* TestMountPointProvider::GetFileUtil(FileSystemType type) {
  return local_file_util_.get();
}

FilePath TestMountPointProvider::GetPathForPermissionsCheck(
    const FilePath& virtual_path) const {
  return base_path_.Append(virtual_path);
}

FileSystemOperation* TestMountPointProvider::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));
  operation_context->set_update_observers(observers_);
  return new LocalFileSystemOperation(context, operation_context.Pass());
}

webkit_blob::FileStreamReader* TestMountPointProvider::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return new FileSystemFileStreamReader(context, url, offset);
}

fileapi::FileStreamWriter* TestMountPointProvider::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return new SandboxFileStreamWriter(context, url, offset, observers_);
}

FileSystemQuotaUtil* TestMountPointProvider::GetQuotaUtil() {
  return quota_util_.get();
}

void TestMountPointProvider::DeleteFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    FileSystemContext* context,
    const DeleteFileSystemCallback& callback) {
  // This won't be called unless we add test code that opens a test
  // filesystem by OpenFileSystem.
  NOTREACHED();
  callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
}

const UpdateObserverList* TestMountPointProvider::GetUpdateObservers(
    FileSystemType type) const {
  return &observers_;
}

}  // namespace fileapi
