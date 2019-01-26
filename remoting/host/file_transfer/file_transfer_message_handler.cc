// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_transfer_message_handler.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/filename_util.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "url/gurl.h"

namespace remoting {

namespace {

// Used if the provided filename can't be used. (E.g., if it is empty, or if
// it consists entirely of disallowed characters.)
constexpr char kDefaultFileName[] = "crd_transfer";

}  // namespace

FileTransferMessageHandler::FileTransferMessageHandler(
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe,
    std::unique_ptr<FileOperations> file_operations)
    : protocol::NamedMessagePipeHandler(name, std::move(pipe)),
      file_operations_(std::move(file_operations)) {
  DCHECK(file_operations_);
}

FileTransferMessageHandler::~FileTransferMessageHandler() = default;

void FileTransferMessageHandler::OnConnected() {}

void FileTransferMessageHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> buffer) {
  protocol::FileTransfer message;
  CompoundBufferInputStream buffer_stream(buffer.get());
  if (!message.ParseFromZeroCopyStream(&buffer_stream)) {
    LOG(ERROR) << "Failed to parse message.";
    CancelAndSendError(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR));
    return;
  }

  if (message.has_metadata()) {
    StartFile(std::move(*message.mutable_metadata()));
    return;
  }

  switch (state_) {
    case kWriting:
      // This is the expected state.
      break;
    case kFailed:
      // Ignore any messages that come in after cancel or error.
      return;
    case kConnected:
      // Don't send an error in response to an error.
      if (!message.has_error()) {
        LOG(ERROR) << "First message must contain file metadata";
        CancelAndSendError(protocol::MakeFileTransferError(
            FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR));
      }
      return;
    case kClosed:
      LOG(ERROR) << "Message received after End";
      CancelAndSendError(protocol::MakeFileTransferError(
          FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR));
      return;
  }

  switch (message.message_case()) {
    case protocol::FileTransfer::kData:
      Write(std::move(*message.mutable_data()->mutable_data()));
      break;
    case protocol::FileTransfer::kEnd:
      Close();
      break;
    case protocol::FileTransfer::kError:
      if (message.error().type() !=
          protocol::FileTransfer_Error_Type_CANCELED) {
        LOG(ERROR) << "File transfer error from client: " << message.error();
      }
      Cancel();
      break;
    default:
      LOG(ERROR) << "Received invalid file-transfer message.";
      CancelAndSendError(protocol::MakeFileTransferError(
          FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR));
      break;
  }
}

void FileTransferMessageHandler::OnDisconnecting() {}

void FileTransferMessageHandler::StartFile(
    protocol::FileTransfer::Metadata metadata) {
  if (state_ != kConnected) {
    LOG(ERROR) << "Only one file per connection is supported.";
    CancelAndSendError(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_PROTOCOL_ERROR));
    return;
  }

  SetState(kWriting);
  // Unretained is sound because the callbacks won't be called after
  // BufferedFileWriter is destroyed, which is in turn owned by this
  // FileTransferMessageHandler.
  buffered_file_writer_.emplace(
      base::BindOnce(&FileTransferMessageHandler::OnComplete,
                     base::Unretained(this)),
      base::BindOnce(&FileTransferMessageHandler::OnError,
                     base::Unretained(this)));
  buffered_file_writer_->Start(
      file_operations_.get(),
      // Ensure filename is safe, and convert from UTF-8 to a FilePath.
      net::GenerateFileName(GURL(), std::string(), std::string(),
                            metadata.filename(), std::string(),
                            kDefaultFileName));
}

void FileTransferMessageHandler::Write(std::string data) {
  DCHECK(state_ == kWriting);
  buffered_file_writer_->Write(std::move(data));
}

void FileTransferMessageHandler::Close() {
  DCHECK(state_ == kWriting);
  SetState(kClosed);
  buffered_file_writer_->Close();
}

void FileTransferMessageHandler::Cancel() {
  SetState(kFailed);
  // Will implicitly cancel if still in progress.
  buffered_file_writer_.reset();
}

void FileTransferMessageHandler::OnComplete() {
  SendResult(kSuccessTag);  // Success
}

void FileTransferMessageHandler::OnError(protocol::FileTransfer_Error error) {
  CancelAndSendError(std::move(error));
}

void FileTransferMessageHandler::SendResult(
    protocol::FileTransferResult<Monostate> result) {
  protocol::FileTransfer result_message;
  if (result) {
    result_message.mutable_success();
  } else {
    *result_message.mutable_error() = std::move(result.error());
  }
  Send(result_message, base::Closure());
}

void FileTransferMessageHandler::CancelAndSendError(
    protocol::FileTransfer_Error error) {
  Cancel();
  SendResult(error);
}

void FileTransferMessageHandler::SetState(State state) {
  switch (state) {
    case kConnected:
      // This is the initial state, but should never be reached again.
      NOTREACHED();
      break;
    case kWriting:
      DCHECK_EQ(kConnected, state_);
      break;
    case kClosed:
      DCHECK_EQ(kWriting, state_);
      break;
    case kFailed:
      // Any state can change to kFailed.
      break;
  }

  state_ = state;
}

}  // namespace remoting
