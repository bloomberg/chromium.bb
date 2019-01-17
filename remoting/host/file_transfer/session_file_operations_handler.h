// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_SESSION_FILE_OPERATIONS_HANDLER_H_
#define REMOTING_HOST_FILE_TRANSFER_SESSION_FILE_OPERATIONS_HANDLER_H_

#include <cstdint>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/file_transfer/ipc_file_operations.h"

namespace remoting {

// An implementation of IpcFileOperations::RequestHandler that forwards all
// operations to another FileOperations implementation.
class SessionFileOperationsHandler : public IpcFileOperations::RequestHandler {
 public:
  // |result_handler| must be valid for the entire lifetime of
  // SessionFileOperationsHandler.
  explicit SessionFileOperationsHandler(
      IpcFileOperations::ResultHandler* result_handler,
      std::unique_ptr<FileOperations> file_operations);
  ~SessionFileOperationsHandler() override;

  // IpcFileOperations::RequestHandler implementation
  void WriteFile(std::uint64_t file_id,
                 const base::FilePath& filename) override;
  void WriteChunk(std::uint64_t file_id, std::string data) override;
  void Close(std::uint64_t file_id) override;
  void Cancel(std::uint64_t file_id) override;

 private:
  void OnWriteFileResult(
      std::uint64_t file_id,
      protocol::FileTransferResult<std::unique_ptr<FileOperations::Writer>>
          result);
  void OnWriteChunkResult(std::uint64_t file_id,
                          protocol::FileTransferResult<Monostate> result);
  void OnCloseResult(std::uint64_t file_id,
                     protocol::FileTransferResult<Monostate> result);

  IpcFileOperations::ResultHandler* result_handler_;
  std::unique_ptr<FileOperations> file_operations_;
  base::flat_map<std::uint64_t, std::unique_ptr<FileOperations::Writer>>
      writers_;

  base::WeakPtrFactory<SessionFileOperationsHandler> weak_ptr_factory_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_SESSION_FILE_OPERATIONS_HANDLER_H_
