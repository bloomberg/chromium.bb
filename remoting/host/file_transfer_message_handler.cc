// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer_message_handler.h"

#include "remoting/base/compound_buffer.h"

namespace remoting {

FileTransferMessageHandler::FileTransferMessageHandler(
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe)
    : protocol::NamedMessagePipeHandler(name, std::move(pipe)) {}

FileTransferMessageHandler::~FileTransferMessageHandler() = default;

void FileTransferMessageHandler::OnConnected() {
  // TODO(jarhar): Implement open logic.
}

void FileTransferMessageHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {
  // TODO(jarhar): Implement message received logic.
}

void FileTransferMessageHandler::OnDisconnecting() {
  // TODO(jarhar): Implement close logic.
}

}  // namespace remoting
