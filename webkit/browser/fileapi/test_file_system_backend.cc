// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/test_file_system_backend.h"

#include <set>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/sequenced_task_runner.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/file_observers.h"
#include "webkit/browser/fileapi/file_system_file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_operation_impl.h"
#include "webkit/browser/fileapi/file_system_quota_util.h"
#include "webkit/browser/fileapi/local_file_util.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/browser/fileapi/sandbox_file_stream_writer.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

// This only supports single origin.
class TestFileSystemBackend::QuotaUtil
    : public FileSystemQuotaUtil,
      public FileUpdateObserver {
 public:
  QuotaUtil(base::SequencedTaskRunner* task_runner)
      : usage_(0),
        task_runner_(task_runner) {
    update_observers_ = update_observers_.AddObserver(this, task_runner_.get());
  }
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

  virtual void AddFileUpdateObserver(
      FileSystemType type,
      FileUpdateObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void AddFileChangeObserver(
      FileSystemType type,
      FileChangeObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE {
    change_observers_ = change_observers_.AddObserver(observer, task_runner);
  }

  virtual void AddFileAccessObserver(
      FileSystemType type,
      FileAccessObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual const UpdateObserverList* GetUpdateObservers(
      FileSystemType type) const OVERRIDE {
    return &update_observers_;
  }

  virtual const ChangeObserverList* GetChangeObservers(
      FileSystemType type) const OVERRIDE {
    return &change_observers_;
  }

  virtual const AccessObserverList* GetAccessObservers(
      FileSystemType type) const OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  // FileUpdateObserver overrides.
  virtual void OnStartUpdate(const FileSystemURL& url) OVERRIDE {}
  virtual void OnUpdate(const FileSystemURL& url, int64 delta) OVERRIDE {
    usage_ += delta;
  }
  virtual void OnEndUpdate(const FileSystemURL& url) OVERRIDE {}

  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  int64 usage_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  UpdateObserverList update_observers_;
  ChangeObserverList change_observers_;
};

TestFileSystemBackend::TestFileSystemBackend(
    base::SequencedTaskRunner* task_runner,
    const base::FilePath& base_path)
    : base_path_(base_path),
      local_file_util_(new AsyncFileUtilAdapter(new LocalFileUtil())),
      quota_util_(new QuotaUtil(task_runner)),
      require_copy_or_move_validator_(false) {
}

TestFileSystemBackend::~TestFileSystemBackend() {
}

bool TestFileSystemBackend::CanHandleType(FileSystemType type) const {
  return (type == kFileSystemTypeTest);
}

void TestFileSystemBackend::Initialize(FileSystemContext* context) {
}

void TestFileSystemBackend::OpenFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  callback.Run(GetFileSystemRootURI(origin_url, type),
               GetFileSystemName(origin_url, type),
               base::PLATFORM_FILE_OK);
}

FileSystemFileUtil* TestFileSystemBackend::GetFileUtil(FileSystemType type) {
  DCHECK(local_file_util_.get());
  return local_file_util_->sync_file_util();
}

AsyncFileUtil* TestFileSystemBackend::GetAsyncFileUtil(FileSystemType type) {
  return local_file_util_.get();
}

CopyOrMoveFileValidatorFactory*
TestFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
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

void TestFileSystemBackend::InitializeCopyOrMoveFileValidatorFactory(
    scoped_ptr<CopyOrMoveFileValidatorFactory> factory) {
  if (!copy_or_move_file_validator_factory_)
    copy_or_move_file_validator_factory_ = factory.Pass();
}

FileSystemOperation* TestFileSystemBackend::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));
  operation_context->set_update_observers(*GetUpdateObservers(url.type()));
  operation_context->set_change_observers(
      *quota_util_->GetChangeObservers(url.type()));
  operation_context->set_root_path(base_path_);
  return new FileSystemOperationImpl(url, context, operation_context.Pass());
}

scoped_ptr<webkit_blob::FileStreamReader>
TestFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  return scoped_ptr<webkit_blob::FileStreamReader>(
      new FileSystemFileStreamReader(
          context, url, offset, expected_modification_time));
}

scoped_ptr<fileapi::FileStreamWriter>
TestFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return scoped_ptr<fileapi::FileStreamWriter>(
      new SandboxFileStreamWriter(context, url, offset,
                                  *GetUpdateObservers(url.type())));
}

FileSystemQuotaUtil* TestFileSystemBackend::GetQuotaUtil() {
  return quota_util_.get();
}

const UpdateObserverList* TestFileSystemBackend::GetUpdateObservers(
    FileSystemType type) const {
  return quota_util_->GetUpdateObservers(type);
}

void TestFileSystemBackend::AddFileChangeObserver(
    FileChangeObserver* observer) {
  quota_util_->AddFileChangeObserver(
      kFileSystemTypeTest, observer, quota_util_->task_runner());
}

}  // namespace fileapi
