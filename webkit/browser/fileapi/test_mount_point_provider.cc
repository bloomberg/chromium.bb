// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/test_mount_point_provider.h"

#include <set>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/sequenced_task_runner.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/file_observers.h"
#include "webkit/browser/fileapi/file_system_file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_quota_util.h"
#include "webkit/browser/fileapi/local_file_system_operation.h"
#include "webkit/browser/fileapi/local_file_util.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/browser/fileapi/sandbox_file_stream_writer.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

// This only supports single origin.
class TestMountPointProvider::QuotaUtil
    : public FileSystemQuotaUtil,
      public FileUpdateObserver {
 public:
  QuotaUtil() : usage_(0) {}
  virtual ~QuotaUtil() {}

  // FileSystemQuotaUtil overrides.
  virtual base::PlatformFileError DeleteOriginDataOnFileThread(
      FileSystemContext* context,
      quota::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      FileSystemType type) OVERRIDE {
    NOTREACHED();
    return base::PLATFORM_FILE_OK;
  }
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
  virtual void StickyInvalidateUsageCache(
      const GURL& origin,
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
    const base::FilePath& base_path)
    : base_path_(base_path),
      task_runner_(task_runner),
      local_file_util_(new AsyncFileUtilAdapter(new LocalFileUtil())),
      quota_util_(new QuotaUtil),
      require_copy_or_move_validator_(false) {
  UpdateObserverList::Source source;
  source.AddObserver(quota_util_.get(), task_runner_.get());
  update_observers_ = UpdateObserverList(source);
}

TestMountPointProvider::~TestMountPointProvider() {
}

bool TestMountPointProvider::CanHandleType(FileSystemType type) const {
  return (type == kFileSystemTypeTest);
}

void TestMountPointProvider::OpenFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  callback.Run(base::PLATFORM_FILE_OK);
}

FileSystemFileUtil* TestMountPointProvider::GetFileUtil(FileSystemType type) {
  DCHECK(local_file_util_.get());
  return local_file_util_->sync_file_util();
}

AsyncFileUtil* TestMountPointProvider::GetAsyncFileUtil(FileSystemType type) {
  return local_file_util_.get();
}

CopyOrMoveFileValidatorFactory*
TestMountPointProvider::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type, base::PlatformFileError* error_code) {
  DCHECK(error_code);
  *error_code = base::PLATFORM_FILE_OK;
  if (require_copy_or_move_validator_) {
    if (!copy_or_move_file_validator_factory_)
      *error_code = base::PLATFORM_FILE_ERROR_SECURITY;
    return copy_or_move_file_validator_factory_.get();
  }
  return NULL;
}

void TestMountPointProvider::InitializeCopyOrMoveFileValidatorFactory(
    scoped_ptr<CopyOrMoveFileValidatorFactory> factory) {
  if (!require_copy_or_move_validator_) {
    DCHECK(!factory);
    return;
  }
  if (!copy_or_move_file_validator_factory_)
    copy_or_move_file_validator_factory_ = factory.Pass();
}

FilePermissionPolicy TestMountPointProvider::GetPermissionPolicy(
    const FileSystemURL& url, int permissions) const {
  return FILE_PERMISSION_ALWAYS_DENY;
}

FileSystemOperation* TestMountPointProvider::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));
  operation_context->set_update_observers(update_observers_);
  operation_context->set_change_observers(change_observers_);
  operation_context->set_root_path(base_path_);
  return new LocalFileSystemOperation(url, context, operation_context.Pass());
}

scoped_ptr<webkit_blob::FileStreamReader>
TestMountPointProvider::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  return scoped_ptr<webkit_blob::FileStreamReader>(
      new FileSystemFileStreamReader(
          context, url, offset, expected_modification_time));
}

scoped_ptr<fileapi::FileStreamWriter>
TestMountPointProvider::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return scoped_ptr<fileapi::FileStreamWriter>(
      new SandboxFileStreamWriter(context, url, offset, update_observers_));
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
  return &update_observers_;
}

void TestMountPointProvider::AddFileChangeObserver(
    FileChangeObserver* observer) {
  ChangeObserverList::Source source = change_observers_.source();
  source.AddObserver(observer, task_runner_.get());
  change_observers_ = ChangeObserverList(source);
}

}  // namespace fileapi
