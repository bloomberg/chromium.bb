// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_proxy_wrapper.h"

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_checker.h"
#include "remoting/base/compound_buffer.h"

namespace {

void EmptyStatusCallback(base::File::Error error) {}

constexpr char kTempFileExtension[] = ".crdownload";

remoting::protocol::FileTransferResponse_ErrorCode FileErrorToResponseError(
    base::File::Error file_error) {
  switch (file_error) {
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return remoting::protocol::
          FileTransferResponse_ErrorCode_PERMISSIONS_ERROR;
    case base::File::FILE_ERROR_NO_SPACE:
      return remoting::protocol::
          FileTransferResponse_ErrorCode_OUT_OF_DISK_SPACE;
    default:
      return remoting::protocol::FileTransferResponse_ErrorCode_FILE_IO_ERROR;
  }
}

}  // namespace

namespace remoting {

class FileProxyWrapperLinux : public FileProxyWrapper {
 public:
  FileProxyWrapperLinux();
  ~FileProxyWrapperLinux() override;

  // FileProxyWrapper implementation.
  void Init(StatusCallback status_callback) override;
  void CreateFile(const base::FilePath& directory,
                  const std::string& filename) override;
  void WriteChunk(std::unique_ptr<CompoundBuffer> buffer) override;
  void Close() override;
  void Cancel() override;
  State state() override;

 private:
  struct FileChunk {
    int64_t write_offset;
    std::vector<char> data;
  };

  // Callbacks for CreateFile().
  void CreateTempFile(int unique_path_number);
  void CreateTempFileCallback(base::File::Error error);

  // Callbacks for WriteChunk().
  void WriteFileChunk(std::unique_ptr<FileChunk> chunk);
  void WriteCallback(base::File::Error error, int bytes_written);

  // Callbacks for Close().
  void CloseFileAndMoveToDestination();
  void CloseCallback(base::File::Error error);
  void MoveToDestination(int unique_path_number);
  void MoveFileCallback(bool success);

  void CancelWithError(protocol::FileTransferResponse_ErrorCode error);
  void SetState(State state);

  State state_ = kUninitialized;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  std::unique_ptr<base::FileProxy> file_proxy_;

  StatusCallback status_callback_;

  bool temp_file_created_ = false;
  base::FilePath temp_filepath_;
  base::FilePath destination_filepath_;

  int64_t next_write_file_offset_ = 0;
  std::queue<std::unique_ptr<FileChunk>> file_chunks_;
  // active_file_chunk_ is the chunk currently being written to disk. It is
  // empty if nothing is being written to disk right now.
  std::unique_ptr<FileChunk> active_file_chunk_;

  base::ThreadChecker thread_checker_;
  base::WeakPtr<FileProxyWrapperLinux> weak_ptr_;
  base::WeakPtrFactory<FileProxyWrapperLinux> weak_factory_;
};

FileProxyWrapperLinux::FileProxyWrapperLinux() : weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

FileProxyWrapperLinux::~FileProxyWrapperLinux() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void FileProxyWrapperLinux::Init(StatusCallback status_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SetState(kInitialized);
  status_callback_ = std::move(status_callback);

  file_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BACKGROUND});
  DCHECK(file_task_runner_);

  file_proxy_.reset(new base::FileProxy(file_task_runner_.get()));

  if (!file_task_runner_) {
    CancelWithError(protocol::FileTransferResponse_ErrorCode_UNEXPECTED_ERROR);
  }
}

void FileProxyWrapperLinux::CreateFile(const base::FilePath& directory,
                                       const std::string& filename) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SetState(kFileCreated);

  destination_filepath_ = directory.Append(filename);
  temp_filepath_ = destination_filepath_.AddExtension(kTempFileExtension);

  PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&base::GetUniquePathNumber, temp_filepath_,
                     base::FilePath::StringType()),
      base::BindOnce(&FileProxyWrapperLinux::CreateTempFile, weak_ptr_));
}

void FileProxyWrapperLinux::CreateTempFile(int unique_path_number) {
  if (unique_path_number > 0) {
    temp_filepath_ = temp_filepath_.InsertBeforeExtensionASCII(
        base::StringPrintf(" (%d)", unique_path_number));
  }
  if (!file_proxy_->CreateOrOpen(
          temp_filepath_, base::File::FLAG_CREATE | base::File::FLAG_WRITE,
          base::Bind(&FileProxyWrapperLinux::CreateTempFileCallback,
                     weak_ptr_))) {
    // file_proxy_ failed to post a task to file_task_runner_.
    CancelWithError(protocol::FileTransferResponse_ErrorCode_UNEXPECTED_ERROR);
  }
}

void FileProxyWrapperLinux::CreateTempFileCallback(base::File::Error error) {
  if (error) {
    LOG(ERROR) << "Creating the temp file failed with error: " << error;
    CancelWithError(FileErrorToResponseError(error));
  } else {
    temp_file_created_ = true;
    // Chunks to write may have been queued while we were creating the file,
    // start writing them now if there were any.
    if (!file_chunks_.empty()) {
      std::unique_ptr<FileChunk> chunk_to_write =
          std::move(file_chunks_.front());
      file_chunks_.pop();
      WriteFileChunk(std::move(chunk_to_write));
    }
  }
}

void FileProxyWrapperLinux::WriteChunk(std::unique_ptr<CompoundBuffer> buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, kFileCreated);

  std::unique_ptr<FileChunk> new_file_chunk = base::WrapUnique(new FileChunk());
  new_file_chunk->data.resize(buffer->total_bytes());
  // This copy could be avoided if CompoundBuffer were updated to allowed us to
  // access the individual buffers in |buffer|.
  // TODO(jarhar): Update CompoundBuffer to allow data transfer without a
  // memcopy.
  buffer->CopyTo(new_file_chunk->data.data(), new_file_chunk->data.size());

  new_file_chunk->write_offset = next_write_file_offset_;
  next_write_file_offset_ += new_file_chunk->data.size();

  // If the file hasn't been created yet or there is another chunk currently
  // being written, we have to queue this chunk to be written later.
  if (!temp_file_created_ || active_file_chunk_) {
    // TODO(jarhar): When flow control enabled QUIC-based WebRTC data channels
    // are implemented, block the flow of incoming chunks here if
    // file_chunks_ has reached a maximum size. This implementation will
    // allow file_chunks_ to grow without limits.
    file_chunks_.push(std::move(new_file_chunk));
  } else {
    WriteFileChunk(std::move(new_file_chunk));
  }
}

void FileProxyWrapperLinux::WriteFileChunk(std::unique_ptr<FileChunk> chunk) {
  active_file_chunk_ = std::move(chunk);
  DCHECK(active_file_chunk_);
  if (!file_proxy_->Write(
          active_file_chunk_->write_offset, active_file_chunk_->data.data(),
          active_file_chunk_->data.size(),
          base::Bind(&FileProxyWrapperLinux::WriteCallback, weak_ptr_))) {
    // file_proxy_ failed to post a task to file_task_runner_.
    CancelWithError(protocol::FileTransferResponse_ErrorCode_UNEXPECTED_ERROR);
  }
}

void FileProxyWrapperLinux::WriteCallback(base::File::Error error,
                                          int bytes_written) {
  if (active_file_chunk_->data.size() != (unsigned)bytes_written || error) {
    if (!error) {
      error = base::File::FILE_ERROR_FAILED;
    }
    LOG(ERROR) << "Write failed with error: " << error;
    CancelWithError(FileErrorToResponseError(error));
    return;
  }

  active_file_chunk_.reset();
  if (!file_chunks_.empty()) {
    std::unique_ptr<FileChunk> chunk_to_write = std::move(file_chunks_.front());
    file_chunks_.pop();
    WriteFileChunk(std::move(chunk_to_write));
  } else if (state_ == kClosing) {
    // All writes are complete and we have gotten the signal to move the file.
    CloseFileAndMoveToDestination();
  }
}

void FileProxyWrapperLinux::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());

  SetState(kClosing);

  if (!active_file_chunk_ && file_chunks_.empty()) {
    // All writes are complete, so we can finish up now.
    CloseFileAndMoveToDestination();
  }
}

void FileProxyWrapperLinux::CloseFileAndMoveToDestination() {
  DCHECK_EQ(state_, kClosing);
  file_proxy_->Close(
      base::Bind(&FileProxyWrapperLinux::CloseCallback, weak_ptr_));
}

void FileProxyWrapperLinux::CloseCallback(base::File::Error error) {
  if (error) {
    CancelWithError(FileErrorToResponseError(error));
    return;
  }

  PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&base::GetUniquePathNumber, destination_filepath_,
                     base::FilePath::StringType()),
      base::BindOnce(&FileProxyWrapperLinux::MoveToDestination, weak_ptr_));
}

void FileProxyWrapperLinux::MoveToDestination(int unique_path_number) {
  if (unique_path_number > 0) {
    destination_filepath_ = destination_filepath_.InsertBeforeExtensionASCII(
        base::StringPrintf(" (%d)", unique_path_number));
  }
  PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&base::Move, temp_filepath_, destination_filepath_),
      base::BindOnce(&FileProxyWrapperLinux::MoveFileCallback, weak_ptr_));
}

void FileProxyWrapperLinux::MoveFileCallback(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (success) {
    SetState(kClosed);
    std::move(status_callback_)
        .Run(state_,
             base::Optional<protocol::FileTransferResponse_ErrorCode>());
  } else {
    CancelWithError(protocol::FileTransferResponse_ErrorCode_FILE_IO_ERROR);
  }
}

void FileProxyWrapperLinux::Cancel() {
  if (file_proxy_->IsValid()) {
    file_proxy_->Close(base::Bind(&EmptyStatusCallback));
  }

  if (state_ == kFileCreated || state_ == kClosing) {
    file_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                  temp_filepath_, false /* recursive */));
  }

  if (state_ == kClosing || state_ == kClosed) {
    file_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                       destination_filepath_, false /* recursive */));
  }

  SetState(kFailed);
}

void FileProxyWrapperLinux::CancelWithError(
    protocol::FileTransferResponse_ErrorCode error) {
  Cancel();
  std::move(status_callback_)
      .Run(state_,
           base::Optional<protocol::FileTransferResponse_ErrorCode>(error));
}

void FileProxyWrapperLinux::SetState(State state) {
  switch (state) {
    case kUninitialized:
      // No state can change to kUninitialized.
      NOTREACHED();
      break;
    case kInitialized:
      DCHECK_EQ(state_, kUninitialized);
      break;
    case kFileCreated:
      DCHECK_EQ(state_, kInitialized);
      break;
    case kClosing:
      DCHECK_EQ(state_, kFileCreated);
      break;
    case kClosed:
      DCHECK_EQ(state_, kClosing);
      break;
    case kFailed:
      // Any state can change to kFailed.
      break;
  }

  state_ = state;
}

FileProxyWrapper::State FileProxyWrapperLinux::state() {
  return state_;
}

// static
std::unique_ptr<FileProxyWrapper> FileProxyWrapper::Create() {
  return base::WrapUnique(new FileProxyWrapperLinux());
}

}  // namespace remoting
