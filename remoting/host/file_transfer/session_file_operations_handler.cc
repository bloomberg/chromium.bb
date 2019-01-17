// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/session_file_operations_handler.h"
#include "base/bind.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

SessionFileOperationsHandler::SessionFileOperationsHandler(
    IpcFileOperations::ResultHandler* result_handler,
    std::unique_ptr<FileOperations> file_operations)
    : result_handler_(result_handler),
      file_operations_(std::move(file_operations)),
      weak_ptr_factory_(this) {}

void SessionFileOperationsHandler::WriteFile(uint64_t file_id,
                                             const base::FilePath& filename) {
  file_operations_->WriteFile(
      filename, base::BindOnce(&SessionFileOperationsHandler::OnWriteFileResult,
                               weak_ptr_factory_.GetWeakPtr(), file_id));
}

void SessionFileOperationsHandler::WriteChunk(uint64_t file_id,
                                              std::string data) {
  auto writer_iter = writers_.find(file_id);
  if (writer_iter == writers_.end()) {
    result_handler_->OnResult(
        file_id,
        protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }
  writer_iter->second->WriteChunk(
      std::move(data),
      base::BindOnce(&SessionFileOperationsHandler::OnWriteChunkResult,
                     weak_ptr_factory_.GetWeakPtr(), file_id));
}

void SessionFileOperationsHandler::Close(uint64_t file_id) {
  auto writer_iter = writers_.find(file_id);
  if (writer_iter == writers_.end()) {
    result_handler_->OnResult(
        file_id,
        protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    return;
  }
  writer_iter->second->Close(
      base::BindOnce(&SessionFileOperationsHandler::OnCloseResult,
                     weak_ptr_factory_.GetWeakPtr(), file_id));
}

void SessionFileOperationsHandler::Cancel(uint64_t file_id) {
  auto writer_iter = writers_.find(file_id);
  if (writer_iter == writers_.end()) {
    // No need to send an error in response to a Cancel request, as it does not
    // expect a response.
    return;
  }

  // Writer will implicitly Cancel when destroyed.
  writers_.erase(writer_iter);
}

void SessionFileOperationsHandler::OnWriteFileResult(
    uint64_t file_id,
    protocol::FileTransferResult<std::unique_ptr<FileOperations::Writer>>
        result) {
  if (!result_handler_) {
    return;
  }

  result_handler_->OnResult(file_id, std::move(result).Map([&](auto writer) {
    writers_.emplace(file_id, std::move(writer));
    return kMonostate;
  }));
}

void SessionFileOperationsHandler::OnWriteChunkResult(
    uint64_t file_id,
    protocol::FileTransferResult<Monostate> result) {
  if (!result_handler_) {
    return;
  }

  if (!result) {
    writers_.erase(file_id);
  }
  result_handler_->OnResult(file_id, std::move(result));
}

void SessionFileOperationsHandler::OnCloseResult(
    uint64_t file_id,
    protocol::FileTransferResult<Monostate> result) {
  if (!result_handler_) {
    return;
  }

  writers_.erase(file_id);
  result_handler_->OnResult(file_id, std::move(result));
}

SessionFileOperationsHandler::~SessionFileOperationsHandler() = default;

}  // namespace remoting
