// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer_message_handler.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

FileTransferMessageHandler::FileTransferMessageHandler(
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe,
    std::unique_ptr<FileProxyWrapper> file_proxy_wrapper)
    : protocol::NamedMessagePipeHandler(name, std::move(pipe)),
      file_proxy_wrapper_(std::move(file_proxy_wrapper)) {
  DCHECK(file_proxy_wrapper_);
}

FileTransferMessageHandler::~FileTransferMessageHandler() = default;

void FileTransferMessageHandler::OnConnected() {
  // base::Unretained is safe here because |file_proxy_wrapper_| is owned by
  // this class, so the callback cannot be run after this class is destroyed.
  file_proxy_wrapper_->Init(base::BindOnce(
      &FileTransferMessageHandler::SaveResultCallback, base::Unretained(this)));
}

void FileTransferMessageHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> buffer) {
  protocol::FileTransfer message;
  CompoundBufferInputStream buffer_stream(buffer.get());
  if (!message.ParseFromZeroCopyStream(&buffer_stream)) {
    CancelAndSendError(
        protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR),
        "Failed to parse message.");
    return;
  }

  if (message.has_metadata()) {
    StartFile(std::move(*message.mutable_metadata()));
    return;
  }

  switch (file_proxy_wrapper_->state()) {
    case FileProxyWrapper::kReady:
      // This is the expected state.
      break;
    case FileProxyWrapper::kFailed:
      // Ignore any messages that come in after we've returned an error.
      return;
    case FileProxyWrapper::kInitialized:
      // Don't send an error in response to an error.
      if (!message.has_error()) {
        CancelAndSendError(
            protocol::MakeFileTransferError(
                FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR),
            "First message must contain file metadata");
      }
      return;
    case FileProxyWrapper::kBusy:
    case FileProxyWrapper::kClosed:
      CancelAndSendError(
          protocol::MakeFileTransferError(
              FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR),
          "Message received after End");
      return;
    default:
      CancelAndSendError(
          protocol::MakeFileTransferError(
              FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR),
          base::StringPrintf("Unexpected FileProxyWrapper state: %d",
                             file_proxy_wrapper_->state()));
      return;
  }

  switch (message.message_case()) {
    case protocol::FileTransfer::kData:
      file_proxy_wrapper_->WriteChunk(
          std::move(*message.mutable_data()->mutable_data()));
      break;
    case protocol::FileTransfer::kEnd:
      file_proxy_wrapper_->Close();
      break;
    case protocol::FileTransfer::kCancel:
    case protocol::FileTransfer::kError:
      file_proxy_wrapper_->Cancel();
      break;
    default:
      CancelAndSendError(
          protocol::MakeFileTransferError(
              FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR),
          "Received invalid file-transfer message.");
      break;
  }
}

void FileTransferMessageHandler::OnDisconnecting() {
  FileProxyWrapper::State proxy_state = file_proxy_wrapper_->state();
  if (proxy_state != FileProxyWrapper::kClosed &&
      proxy_state != FileProxyWrapper::kFailed) {
    // Channel was closed earlier than expected, cancel the transfer.
    file_proxy_wrapper_->Cancel();
  }
}

void FileTransferMessageHandler::SaveResultCallback(
    base::Optional<protocol::FileTransfer_Error> error) {
  protocol::FileTransfer result_message;
  if (error) {
    *result_message.mutable_error() = std::move(*error);
  } else {
    result_message.mutable_success();
  }
  Send(result_message, base::Closure());
}

void FileTransferMessageHandler::StartFile(
    protocol::FileTransfer::Metadata metadata) {
  if (file_proxy_wrapper_->state() != FileProxyWrapper::kInitialized) {
    CancelAndSendError(
        protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR),
        "Only one file per connection is supported.");
    return;
  }

  base::FilePath target_directory;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &target_directory)) {
    CancelAndSendError(
        protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR),
        "Failed to get DIR_USER_DESKTOP from base::PathService::Get");
    return;
  }
  file_proxy_wrapper_->CreateFile(target_directory, metadata.filename());
}

void FileTransferMessageHandler::CancelAndSendError(
    protocol::FileTransfer_Error error,
    const std::string& log_message) {
  LOG(ERROR) << log_message;
  file_proxy_wrapper_->Cancel();
  SaveResultCallback(error);
}

}  // namespace remoting
