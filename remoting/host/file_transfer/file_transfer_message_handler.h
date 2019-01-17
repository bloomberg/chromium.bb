// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FILE_TRANSFER_MESSAGE_HANDLER_H_
#define REMOTING_HOST_FILE_TRANSFER_FILE_TRANSFER_MESSAGE_HANDLER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "remoting/host/file_transfer/buffered_file_writer.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "remoting/protocol/named_message_pipe_handler.h"

namespace remoting {

constexpr char kFileTransferDataChannelPrefix[] = "filetransfer-";

class FileTransferMessageHandler : public protocol::NamedMessagePipeHandler {
 public:
  FileTransferMessageHandler(const std::string& name,
                             std::unique_ptr<protocol::MessagePipe> pipe,
                             std::unique_ptr<FileOperations> file_operations);
  ~FileTransferMessageHandler() override;

  // protocol::NamedMessagePipeHandler implementation.
  void OnConnected() override;
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;
  void OnDisconnecting() override;

 private:
  enum State {
    // Initial state.
    kConnected,
    // We are writing a file.
    kWriting,
    // End states
    // File successfully written.
    kClosed,
    // An error occured or the transfer was canceled.
    kFailed,
  };
  void StartFile(protocol::FileTransfer_Metadata metadata);
  void Write(std::string data);
  void Close();
  void Cancel();
  void OnComplete();
  void OnError(protocol::FileTransfer_Error error);
  void SendResult(protocol::FileTransferResult<Monostate> result);
  void CancelAndSendError(protocol::FileTransfer_Error error);
  void SetState(State state);

  State state_ = kConnected;
  std::unique_ptr<FileOperations> file_operations_;
  base::Optional<BufferedFileWriter> buffered_file_writer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FILE_TRANSFER_MESSAGE_HANDLER_H_
