// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fileapi/drivefs_async_file_util.h"

#include <utility>

#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/local_file_util.h"

namespace drive {
namespace internal {
namespace {

class CopyOperation {
 public:
  CopyOperation(
      Profile* profile,
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      storage::AsyncFileUtil::CopyOrMoveOption option,
      storage::AsyncFileUtil::CopyFileProgressCallback progress_callback,
      storage::AsyncFileUtil::StatusCallback callback,
      scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
      base::WeakPtr<DriveFsAsyncFileUtil> async_file_util)
      : profile_(profile),
        context_(std::move(context)),
        src_url_(src_url),
        dest_url_(dest_url),
        option_(option),
        progress_callback_(std::move(progress_callback)),
        callback_(std::move(callback)),
        origin_task_runner_(std::move(origin_task_runner)),
        async_file_util_(std::move(async_file_util)) {
    DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  }

  void Start() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    auto* drive_integration_service =
        drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
    if (!drive_integration_service->IsMounted()) {
      return Fail();
    }
    base::FilePath path("/");
    if (!drive_integration_service->GetMountPointPath().AppendRelativePath(
            src_url_.path(), &path)) {
      return Fail();
    }
    drive_integration_service->GetDriveFsInterface()->GetMetadata(
        path, false,
        mojo::WrapCallbackWithDropHandler(
            base::BindOnce(&CopyOperation::GotMetadata, base::Unretained(this)),
            base::BindOnce(&CopyOperation::Fail, base::Unretained(this))));
  }

 private:
  void Fail() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    origin_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), base::File::FILE_ERROR_FAILED));
    delete this;
  }

  void FallbackToNativeCopy() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    origin_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CopyOperation::FallbackToNativeCopyOnOriginThread,
                       base::Unretained(this)));
  }

  void FallbackToNativeCopyOnOriginThread() {
    DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

    async_file_util_->AsyncFileUtilAdapter::CopyFileLocal(
        std::move(context_), src_url_, dest_url_, option_,
        std::move(progress_callback_), std::move(callback_));
    delete this;
  }

  void GotMetadata(drive::FileError error,
                   drivefs::mojom::FileMetadataPtr metadata) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (!metadata) {
      return Fail();
    }
    if (metadata->type != drivefs::mojom::FileMetadata::Type::kHosted) {
      return FallbackToNativeCopy();
    }

    // TODO(crbug.com/870009): Implement copy for hosted files once DriveFS
    // provides an API.
    Fail();
  }

  Profile* const profile_;
  std::unique_ptr<storage::FileSystemOperationContext> context_;
  const storage::FileSystemURL src_url_;
  const storage::FileSystemURL dest_url_;
  const storage::AsyncFileUtil::CopyOrMoveOption option_;
  storage::AsyncFileUtil::CopyFileProgressCallback progress_callback_;
  storage::AsyncFileUtil::StatusCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  base::WeakPtr<DriveFsAsyncFileUtil> async_file_util_;
};

}  // namespace

DriveFsAsyncFileUtil::DriveFsAsyncFileUtil(Profile* profile)
    : AsyncFileUtilAdapter(new storage::LocalFileUtil),
      profile_(profile),
      weak_factory_(this) {}

DriveFsAsyncFileUtil::~DriveFsAsyncFileUtil() = default;

void DriveFsAsyncFileUtil::CopyFileLocal(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& src_url,
    const storage::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    CopyFileProgressCallback progress_callback,
    StatusCallback callback) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &CopyOperation::Start,
          base::Unretained(new CopyOperation(
              profile_, std::move(context), src_url, dest_url, option,
              std::move(progress_callback), std::move(callback),
              base::SequencedTaskRunnerHandle::Get(),
              weak_factory_.GetWeakPtr()))));
}

}  // namespace internal
}  // namespace drive
