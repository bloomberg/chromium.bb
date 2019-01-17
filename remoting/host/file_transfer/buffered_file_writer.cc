// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/buffered_file_writer.h"
#include "base/bind.h"

#include <utility>

namespace remoting {

BufferedFileWriter::BufferedFileWriter(
    base::OnceClosure on_complete,
    base::OnceCallback<void(protocol::FileTransfer_Error)> on_error)
    : on_complete_(std::move(on_complete)),
      on_error_(std::move(on_error)),
      weak_ptr_factory_(this) {}

BufferedFileWriter::~BufferedFileWriter() = default;

void BufferedFileWriter::Start(FileOperations* file_operations,
                               const base::FilePath& filename) {
  DCHECK_EQ(kNotStarted, state_);
  SetState(kWorking);
  file_operations->WriteFile(
      filename, base::BindOnce(&BufferedFileWriter::OnWriteFileResult,
                               weak_ptr_factory_.GetWeakPtr()));
}

void BufferedFileWriter::Write(std::string data) {
  if (state_ == kFailed) {
    return;
  }
  DCHECK(state_ == kWorking || state_ == kWaiting);
  chunks_.push(std::move(data));

  if (state_ == kWaiting) {
    SetState(kWorking);
    WriteNextChunk();
  }
}

void BufferedFileWriter::Close() {
  if (state_ == kFailed) {
    return;
  }
  DCHECK(state_ == kWorking || state_ == kWaiting);

  State old_state = state_;
  SetState(kClosing);
  if (old_state != kWorking) {
    DoClose();
  }
}

void BufferedFileWriter::Cancel() {
  SetState(kFailed);
  // Will implicitly cancel if still in progress.
  writer_.reset();
}

void BufferedFileWriter::OnWriteFileResult(
    protocol::FileTransferResult<std::unique_ptr<FileOperations::Writer>>
        result) {
  OnWriteResult(std::move(result).Map([&](auto writer) {
    writer_ = std::move(writer);
    return kMonostate;
  }));
}

void BufferedFileWriter::WriteNextChunk() {
  DCHECK(!chunks_.empty());
  DCHECK(state_ == kWorking || state_ == kClosing);
  std::string data = std::move(chunks_.front());
  chunks_.pop();
  writer_->WriteChunk(std::move(data),
                      base::BindOnce(&BufferedFileWriter::OnWriteResult,
                                     weak_ptr_factory_.GetWeakPtr()));
}

// Handles the result from both WriteFile and WriteChunk. For the former, it is
// called by OnWriteFileResult after setting writer_.
void BufferedFileWriter::OnWriteResult(
    protocol::FileTransferResult<Monostate> result) {
  if (!result) {
    SetState(kFailed);
    std::move(on_error_).Run(std::move(result.error()));
    return;
  }

  if (!chunks_.empty()) {
    WriteNextChunk();
  } else if (state_ == kClosing) {
    DoClose();
  } else {
    SetState(kWaiting);
  }
}

void BufferedFileWriter::DoClose() {
  DCHECK(chunks_.empty());
  DCHECK_EQ(kClosing, state_);
  writer_->Close(base::BindOnce(&BufferedFileWriter::OnCloseResult,
                                weak_ptr_factory_.GetWeakPtr()));
}

void BufferedFileWriter::OnCloseResult(
    protocol::FileTransferResult<Monostate> result) {
  if (!result) {
    SetState(kFailed);
    std::move(on_error_).Run(std::move(result.error()));
    return;
  }

  SetState(kClosed);
  std::move(on_complete_).Run();
}

void BufferedFileWriter::SetState(BufferedFileWriter::State state) {
  switch (state) {
    case kNotStarted:
      // This is the initial state, but should never be reached again.
      NOTREACHED();
      break;
    case kWorking:
      DCHECK(state_ == kNotStarted || state_ == kWaiting);
      break;
    case kWaiting:
      DCHECK(state_ == kWorking);
      break;
    case kClosing:
      DCHECK(state_ == kWorking || state_ == kWaiting);
      break;
    case kClosed:
      DCHECK(state_ == kClosing);
      break;
    case kFailed:
      // Any state can change to kFailed.
      break;
  }

  state_ = state;
}

}  // namespace remoting
