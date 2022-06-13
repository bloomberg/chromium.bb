// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/fileapi/drivefs_async_file_util.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/local_file_util.h"
#include "storage/browser/file_system/native_file_util.h"
#include "storage/common/file_system/file_system_util.h"

namespace drive {
namespace internal {
namespace {

class DriveFsFileUtil : public storage::LocalFileUtil {
 public:
  DriveFsFileUtil() = default;

  DriveFsFileUtil(const DriveFsFileUtil&) = delete;
  DriveFsFileUtil& operator=(const DriveFsFileUtil&) = delete;

  ~DriveFsFileUtil() override = default;

 protected:
  bool IsHiddenItem(const base::FilePath& local_file_path) const override {
    // DriveFS is a trusted filesystem, allow symlinks.
    return false;
  }
};

class CopyOperation {
 public:
  CopyOperation(
      Profile* profile,
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      storage::AsyncFileUtil::CopyOrMoveOptionSet options,
      storage::AsyncFileUtil::CopyFileProgressCallback progress_callback,
      storage::AsyncFileUtil::StatusCallback callback,
      scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
      base::WeakPtr<DriveFsAsyncFileUtil> async_file_util)
      : profile_(profile),
        context_(std::move(context)),
        src_url_(src_url),
        dest_url_(dest_url),
        options_(options),
        progress_callback_(std::move(progress_callback)),
        callback_(std::move(callback)),
        origin_task_runner_(std::move(origin_task_runner)),
        async_file_util_(std::move(async_file_util)) {
    DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  }

  CopyOperation(const CopyOperation&) = delete;
  CopyOperation& operator=(const CopyOperation&) = delete;

  void Start() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    auto* drive_integration_service =
        drive::util::GetIntegrationServiceByProfile(profile_);
    base::FilePath source_path("/");
    base::FilePath destination_path("/");
    if (!drive_integration_service ||
        !drive_integration_service->GetMountPointPath().AppendRelativePath(
            src_url_.path(), &source_path) ||
        !drive_integration_service->GetMountPointPath().AppendRelativePath(
            dest_url_.path(), &destination_path)) {
      origin_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_),
                                    base::File::FILE_ERROR_INVALID_OPERATION));
      origin_task_runner_->DeleteSoon(FROM_HERE, this);
      return;
    }
    drive_integration_service->GetDriveFsInterface()->CopyFile(
        source_path, destination_path,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&CopyOperation::CopyComplete,
                           base::Unretained(this)),
            drive::FILE_ERROR_ABORT));
  }

 private:
  void CopyComplete(drive::FileError error) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    switch (error) {
      case drive::FILE_ERROR_NOT_FOUND:
      case drive::FILE_ERROR_NO_CONNECTION:
        origin_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&CopyOperation::FallbackToNativeCopyOnOriginThread,
                           base::Unretained(this)));
        break;

      default:
        origin_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback_),
                                      FileErrorToBaseFileError(error)));
        origin_task_runner_->DeleteSoon(FROM_HERE, this);
    }
  }

  void FallbackToNativeCopyOnOriginThread() {
    DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

    if (!async_file_util_) {
      std::move(callback_).Run(base::File::FILE_ERROR_ABORT);
      return;
    }
    async_file_util_->AsyncFileUtilAdapter::CopyFileLocal(
        std::move(context_), src_url_, dest_url_, options_,
        std::move(progress_callback_), std::move(callback_));
    delete this;
  }

  Profile* const profile_;
  std::unique_ptr<storage::FileSystemOperationContext> context_;
  const storage::FileSystemURL src_url_;
  const storage::FileSystemURL dest_url_;
  const storage::AsyncFileUtil::CopyOrMoveOptionSet options_;
  storage::AsyncFileUtil::CopyFileProgressCallback progress_callback_;
  storage::AsyncFileUtil::StatusCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  base::WeakPtr<DriveFsAsyncFileUtil> async_file_util_;
};

// Recursively deletes a folder locally. The folder will still be available in
// Drive cloud Trash.
class DeleteOperation {
 public:
  DeleteOperation(Profile* profile,
                  const base::FilePath& path,
                  storage::AsyncFileUtil::StatusCallback callback,
                  scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
                  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
      : profile_(profile),
        path_(path),
        callback_(std::move(callback)),
        origin_task_runner_(std::move(origin_task_runner)),
        blocking_task_runner_(std::move(blocking_task_runner)) {
    DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  }

  DeleteOperation(const DeleteOperation&) = delete;
  DeleteOperation& operator=(const DeleteOperation&) = delete;

  void Start() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    auto* drive_integration_service =
        drive::util::GetIntegrationServiceByProfile(profile_);
    base::FilePath relative_path;
    if (!drive_integration_service ||
        !drive_integration_service->GetMountPointPath().IsParent(path_)) {
      origin_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback_), base::File::FILE_ERROR_FAILED));
      origin_task_runner_->DeleteSoon(FROM_HERE, this);
      return;
    }

    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DeleteOperation::Delete, base::Unretained(this)));
  }

  void Delete() {
    base::File::Error error = base::DeletePathRecursively(path_)
                                  ? base::File::FILE_OK
                                  : base::File::FILE_ERROR_FAILED;
    origin_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(callback_), error));
    origin_task_runner_->DeleteSoon(FROM_HERE, this);
  }

  Profile* const profile_;
  const base::FilePath path_;
  storage::AsyncFileUtil::StatusCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::FilePath path_in_trash_;
};

}  // namespace

DriveFsAsyncFileUtil::DriveFsAsyncFileUtil(Profile* profile)
    : AsyncFileUtilAdapter(std::make_unique<DriveFsFileUtil>()),
      profile_(profile) {}

DriveFsAsyncFileUtil::~DriveFsAsyncFileUtil() = default;

void DriveFsAsyncFileUtil::CopyFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    CopyFileProgressCallback progress_callback,
    StatusCallback callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CopyOperation::Start,
          base::Unretained(new CopyOperation(
              profile_, std::move(context), src_url, dest_url, options,
              std::move(progress_callback), std::move(callback),
              base::SequencedTaskRunnerHandle::Get(),
              weak_factory_.GetWeakPtr()))));
}

void DriveFsAsyncFileUtil::DeleteRecursively(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DeleteOperation::Start,
                                base::Unretained(new DeleteOperation(
                                    profile_, url.path(), std::move(callback),
                                    base::SequencedTaskRunnerHandle::Get(),
                                    context->task_runner()))));
}

}  // namespace internal
}  // namespace drive
