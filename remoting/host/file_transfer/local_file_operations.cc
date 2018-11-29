// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/local_file_operations.h"

#include <cstdint>

#include "base/bind.h"
#include "base/files/file_proxy.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

namespace {

constexpr char kTempFileExtension[] = ".crdownload";

remoting::protocol::FileTransfer_Error_Type FileErrorToResponseErrorType(
    base::File::Error file_error) {
  switch (file_error) {
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return remoting::protocol::FileTransfer_Error_Type_PERMISSION_DENIED;
    case base::File::FILE_ERROR_NO_SPACE:
      return remoting::protocol::FileTransfer_Error_Type_OUT_OF_DISK_SPACE;
    default:
      return remoting::protocol::FileTransfer_Error_Type_IO_ERROR;
  }
}

class LocalFileWriter : public FileOperations::Writer {
 public:
  ~LocalFileWriter() override;

  // FileOperations::FileWriter implementation
  void WriteChunk(std::string data, Callback callback) override;
  void Close(Callback callback) override;
  void Cancel() override;
  FileOperations::State state() override;

  static void WriteFile(const base::FilePath& filename,
                        FileOperations::WriteFileCallback callback);

 private:
  LocalFileWriter(scoped_refptr<base::SequencedTaskRunner> file_task_runner,
                  std::unique_ptr<base::FileProxy> file_proxy,
                  const base::FilePath& destination_filepath);

  // Callbacks for CreateFile(). These are static because they're used in the
  // construction of TheadedFileWriter.
  static void CreateTempFile(std::unique_ptr<LocalFileWriter> writer,
                             FileOperations::WriteFileCallback callback,
                             int unique_path_number);
  static void OnCreateResult(std::unique_ptr<LocalFileWriter> writer,
                             FileOperations::WriteFileCallback callback,
                             base::File::Error error);

  void OnWriteResult(std::string data,
                     Callback callback,
                     base::File::Error error,
                     int bytes_written);

  // Callbacks for Close().
  void OnCloseResult(Callback callback, base::File::Error error);
  void MoveToDestination(Callback callback, int unique_path_number);
  void OnMoveResult(Callback callback, bool success);

  void SetState(FileOperations::State state);

  FileOperations::State state_ = FileOperations::kReady;

  base::FilePath destination_filepath_;
  base::FilePath temp_filepath_;
  std::uint64_t bytes_written_ = 0;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  std::unique_ptr<base::FileProxy> file_proxy_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LocalFileWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileWriter);
};

LocalFileWriter::LocalFileWriter(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    std::unique_ptr<base::FileProxy> file_proxy,
    const base::FilePath& destination_filepath)
    : destination_filepath_(std::move(destination_filepath)),
      temp_filepath_(
          destination_filepath_.AddExtensionASCII(kTempFileExtension)),
      file_task_runner_(std::move(file_task_runner)),
      file_proxy_(std::move(file_proxy)),
      weak_factory_(this) {}

LocalFileWriter::~LocalFileWriter() {
  Cancel();
}

void LocalFileWriter::WriteChunk(std::string data, Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(FileOperations::kReady, state_);
  SetState(FileOperations::kBusy);
  // TODO(rkjnsn): Under what circumstances can posting the task fail? Is it
  //               worth checking for? If so, what should we do in that case,
  //               given that callback is moved into the task and not returned
  //               on error?

  // Ensure buffer pointer is obtained before data is moved.
  const char* buffer = data.data();
  const std::size_t size = data.size();
  file_proxy_->Write(bytes_written_, buffer, size,
                     base::BindOnce(&LocalFileWriter::OnWriteResult,
                                    weak_factory_.GetWeakPtr(), std::move(data),
                                    std::move(callback)));
}

void LocalFileWriter::Close(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(FileOperations::kReady, state_);
  SetState(FileOperations::kBusy);
  file_proxy_->Close(base::BindOnce(&LocalFileWriter::OnCloseResult,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(callback)));
}

void LocalFileWriter::Cancel() {
  if (state_ == FileOperations::kClosed || state_ == FileOperations::kFailed) {
    return;
  }

  // Ensure we don't receive further callbacks.
  weak_factory_.InvalidateWeakPtrs();
  // Drop FileProxy, which will close the underlying file on the file sequence
  // after any possible pending operation is complete.
  file_proxy_.reset();
  // And finally delete the temp file.
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                temp_filepath_, false /* recursive */));
  SetState(FileOperations::kFailed);
}

FileOperations::State LocalFileWriter::state() {
  return state_;
}

void LocalFileWriter::WriteFile(const base::FilePath& filename,
                                FileOperations::WriteFileCallback callback) {
  base::FilePath target_directory;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &target_directory)) {
    LOG(ERROR) << "Failed to get DIR_USER_DESKTOP from base::PathService::Get";

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            protocol::MakeFileTransferError(
                FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR),
            nullptr));
    return;
  }

  scoped_refptr<base::SequencedTaskRunner> file_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  DCHECK(file_task_runner);
  base::SequencedTaskRunner* file_task_runner_ptr = file_task_runner.get();
  auto file_proxy = std::make_unique<base::FileProxy>(file_task_runner.get());
  base::FilePath destination_filepath =
      target_directory.Append(filename.BaseName());

  auto writer = base::WrapUnique(
      new LocalFileWriter(std::move(file_task_runner), std::move(file_proxy),
                          destination_filepath));

  // Ensure path reference is obtained before writer is moved.
  const base::FilePath& temp_filepath = writer->temp_filepath_;

  // Passing file_task_runner_ptr and temp_filepath is safe because they are
  // kept alive by writer, which is bound to the callback.
  PostTaskAndReplyWithResult(
      file_task_runner_ptr, FROM_HERE,
      base::BindOnce(&base::GetUniquePathNumber, temp_filepath,
                     base::FilePath::StringType()),
      base::BindOnce(&CreateTempFile, std::move(writer), std::move(callback)));
}

void LocalFileWriter::CreateTempFile(std::unique_ptr<LocalFileWriter> writer,
                                     FileOperations::WriteFileCallback callback,
                                     int unique_path_number) {
  if (unique_path_number > 0) {
    writer->temp_filepath_ = writer->temp_filepath_.InsertBeforeExtensionASCII(
        base::StringPrintf(" (%d)", unique_path_number));
  }

  // Ensure needed pointers/references are obtained before writer is moved.
  base::FileProxy* file_proxy_ptr = writer->file_proxy_.get();
  const base::FilePath& temp_filepath = writer->temp_filepath_;

  // FLAG_SHARE_DELETE allows the file to be marked as deleted on Windows while
  // the handle is still open. (Other OS's allow this by default.) This allows
  // Cancel to clean up the temporary file even if there are writes pending.
  file_proxy_ptr->CreateOrOpen(
      temp_filepath,
      base::File::FLAG_CREATE | base::File::FLAG_WRITE |
          base::File::FLAG_SHARE_DELETE,
      base::BindOnce(&OnCreateResult, std::move(writer), std::move(callback)));
}

void LocalFileWriter::OnCreateResult(std::unique_ptr<LocalFileWriter> writer,
                                     FileOperations::WriteFileCallback callback,
                                     base::File::Error error) {
  if (error != base::File::FILE_OK) {
    LOG(ERROR) << "Creating temp file failed with error: " << error;
    std::move(callback).Run(
        protocol::MakeFileTransferError(
            FROM_HERE, FileErrorToResponseErrorType(error), error),
        nullptr);
  } else {
    // Now that the temp file has been created successfully, we could lock it
    // using base::File::Lock(), but this would not prevent the file from being
    // deleted. When the file is deleted, WriteChunk() will continue to write to
    // the file as if the file was still there, and an error will occur when
    // calling base::Move() to move the temp file. Chrome exhibits the same
    // behavior with its downloads.
    std::move(callback).Run(base::nullopt, std::move(writer));
  }
}

void LocalFileWriter::OnWriteResult(std::string data,
                                    Callback callback,
                                    base::File::Error error,
                                    int bytes_written) {
  if (error != base::File::FILE_OK) {
    LOG(ERROR) << "Write failed with error: " << error;
    Cancel();
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, FileErrorToResponseErrorType(error), error));
    return;
  }

  SetState(FileOperations::kReady);
  bytes_written_ += bytes_written;

  // bytes_written should never be negative if error is FILE_OK.
  if (static_cast<std::size_t>(bytes_written) != data.size()) {
    // Write already makes a "best effort" to write all of the data, so this
    // probably means that an error occurred. Unfortunately, the only way to
    // find out what went wrong is to try again.
    // TODO(rkjnsn): Would it be better just to return a generic error, here?
    WriteChunk(data.substr(bytes_written), std::move(callback));
    return;
  }

  std::move(callback).Run(base::nullopt);
}

void LocalFileWriter::OnCloseResult(Callback callback,
                                    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    LOG(ERROR) << "Close failed with error: " << error;
    Cancel();
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, FileErrorToResponseErrorType(error), error));
    return;
  }

  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&base::GetUniquePathNumber, destination_filepath_,
                     base::FilePath::StringType()),
      base::BindOnce(&LocalFileWriter::MoveToDestination,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalFileWriter::MoveToDestination(Callback callback,
                                        int unique_path_number) {
  if (unique_path_number > 0) {
    destination_filepath_ = destination_filepath_.InsertBeforeExtensionASCII(
        base::StringPrintf(" (%d)", unique_path_number));
  }
  PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&base::Move, temp_filepath_, destination_filepath_),
      base::BindOnce(&LocalFileWriter::OnMoveResult, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void LocalFileWriter::OnMoveResult(Callback callback, bool success) {
  if (success) {
    SetState(FileOperations::kClosed);
    std::move(callback).Run(base::nullopt);
  } else {
    LOG(ERROR) << "Failed to move file to final destination.";
    Cancel();
    std::move(callback).Run(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_IO_ERROR));
  }
}

void LocalFileWriter::SetState(FileOperations::State state) {
  switch (state) {
    case FileOperations::kReady:
      DCHECK(state_ == FileOperations::kBusy);
      break;
    case FileOperations::kBusy:
      DCHECK_EQ(state_, FileOperations::kReady);
      break;
    case FileOperations::kClosed:
      DCHECK(state_ == FileOperations::kBusy);
      break;
    case FileOperations::kFailed:
      // Any state can change to kFailed.
      break;
  }

  state_ = state;
}

}  // namespace

void LocalFileOperations::WriteFile(
    const base::FilePath& filename,
    FileOperations::WriteFileCallback callback) {
  LocalFileWriter::WriteFile(filename, std::move(callback));
}

void LocalFileOperations::ReadFile(FileOperations::ReadFileCallback) {
  NOTIMPLEMENTED();
}

}  // namespace remoting
