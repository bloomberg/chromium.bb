// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/remote_security_key_message_handler.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "remoting/host/security_key/remote_security_key_ipc_client.h"
#include "remoting/host/security_key/remote_security_key_ipc_constants.h"
#include "remoting/host/security_key/remote_security_key_message_reader_impl.h"
#include "remoting/host/security_key/remote_security_key_message_writer_impl.h"

namespace remoting {

RemoteSecurityKeyMessageHandler::RemoteSecurityKeyMessageHandler() {}

RemoteSecurityKeyMessageHandler::~RemoteSecurityKeyMessageHandler() {}

void RemoteSecurityKeyMessageHandler::Start(
    base::File message_read_stream,
    base::File message_write_stream,
    std::unique_ptr<RemoteSecurityKeyIpcClient> ipc_client,
    const base::Closure& error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(message_read_stream.IsValid());
  DCHECK(message_write_stream.IsValid());
  DCHECK(ipc_client);
  DCHECK(!error_callback.is_null());
  DCHECK(error_callback_.is_null());

  if (!reader_) {
    reader_.reset(
        new RemoteSecurityKeyMessageReaderImpl(std::move(message_read_stream)));
  }

  if (!writer_) {
    writer_.reset(new RemoteSecurityKeyMessageWriterImpl(
        std::move(message_write_stream)));
  }

  ipc_client_ = std::move(ipc_client);
  error_callback_ = error_callback;

  reader_->Start(
      base::Bind(
          &RemoteSecurityKeyMessageHandler::ProcessRemoteSecurityKeyMessage,
          base::Unretained(this)),
      base::Bind(&RemoteSecurityKeyMessageHandler::OnError,
                 base::Unretained(this)));
}

void RemoteSecurityKeyMessageHandler::SetRemoteSecurityKeyMessageReaderForTest(
    std::unique_ptr<RemoteSecurityKeyMessageReader> reader) {
  DCHECK(!reader_);
  reader_ = std::move(reader);
}

void RemoteSecurityKeyMessageHandler::SetRemoteSecurityKeyMessageWriterForTest(
    std::unique_ptr<RemoteSecurityKeyMessageWriter> writer) {
  DCHECK(!writer_);
  writer_ = std::move(writer);
}

void RemoteSecurityKeyMessageHandler::ProcessRemoteSecurityKeyMessage(
    std::unique_ptr<SecurityKeyMessage> message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  RemoteSecurityKeyMessageType message_type = message->type();
  if (message_type == RemoteSecurityKeyMessageType::CONNECT) {
    HandleConnectRequest(message->payload());
  } else if (message_type == RemoteSecurityKeyMessageType::REQUEST) {
    HandleSecurityKeyRequest(message->payload());
  } else {
    LOG(ERROR) << "Unknown message type: "
               << static_cast<uint8_t>(message_type);
    SendMessage(RemoteSecurityKeyMessageType::UNKNOWN_COMMAND);
  }
}

void RemoteSecurityKeyMessageHandler::HandleIpcConnectionChange(
    bool connection_established) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (connection_established) {
    SendMessageWithPayload(RemoteSecurityKeyMessageType::CONNECT_RESPONSE,
                           std::string(1, kConnectResponseActiveSession));
  } else {
    SendMessageWithPayload(
        RemoteSecurityKeyMessageType::CONNECT_ERROR,
        "Unknown error occurred while establishing connection.");
  }
}

void RemoteSecurityKeyMessageHandler::HandleSecurityKeyResponse(
    const std::string& response_data) {
  if (response_data.compare(kRemoteSecurityKeyConnectionError) == 0) {
    SendMessageWithPayload(RemoteSecurityKeyMessageType::REQUEST_ERROR,
                           "An error occurred during the request.");
    return;
  }

  if (response_data.empty()) {
    SendMessageWithPayload(RemoteSecurityKeyMessageType::REQUEST_ERROR,
                           "Invalid client response received.");
    return;
  }

  SendMessageWithPayload(RemoteSecurityKeyMessageType::REQUEST_RESPONSE,
                         response_data);
}

void RemoteSecurityKeyMessageHandler::HandleConnectRequest(
    const std::string& message_payload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!message_payload.empty()) {
    SendMessageWithPayload(RemoteSecurityKeyMessageType::CONNECT_ERROR,
                           "Unexpected payload data received.");
    return;
  }

  if (ipc_client_->WaitForSecurityKeyIpcServerChannel()) {
    // If we find an IPC server, then attempt to establish a connection.
    ipc_client_->EstablishIpcConnection(
        base::Bind(&RemoteSecurityKeyMessageHandler::HandleIpcConnectionChange,
                   base::Unretained(this), true),
        base::Bind(&RemoteSecurityKeyMessageHandler::HandleIpcConnectionChange,
                   base::Unretained(this), false));
  } else {
    SendMessageWithPayload(RemoteSecurityKeyMessageType::CONNECT_RESPONSE,
                           std::string(1, kConnectResponseNoSession));
  }
}

void RemoteSecurityKeyMessageHandler::HandleSecurityKeyRequest(
    const std::string& message_payload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (message_payload.empty()) {
    SendMessageWithPayload(RemoteSecurityKeyMessageType::REQUEST_ERROR,
                           "Request sent without request data.");
    return;
  }

  if (!ipc_client_->SendSecurityKeyRequest(
          message_payload,
          base::Bind(
              &RemoteSecurityKeyMessageHandler::HandleSecurityKeyResponse,
              base::Unretained(this)))) {
    SendMessageWithPayload(RemoteSecurityKeyMessageType::REQUEST_ERROR,
                           "Failed to send request data.");
  }
}

void RemoteSecurityKeyMessageHandler::SendMessage(
    RemoteSecurityKeyMessageType message_type) {
  if (!writer_->WriteMessage(message_type)) {
    OnError();
  }
}

void RemoteSecurityKeyMessageHandler::SendMessageWithPayload(
    RemoteSecurityKeyMessageType message_type,
    const std::string& message_payload) {
  if (!writer_->WriteMessageWithPayload(message_type, message_payload)) {
    OnError();
  }
}

void RemoteSecurityKeyMessageHandler::OnError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ipc_client_.reset();
  writer_.reset();
  reader_.reset();

  if (!error_callback_.is_null()) {
    base::ResetAndReturn(&error_callback_).Run();
  }
}

}  // namespace remoting
