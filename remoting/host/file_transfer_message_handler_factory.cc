// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer_message_handler_factory.h"

namespace remoting {

FileTransferMessageHandlerFactory::FileTransferMessageHandlerFactory() =
    default;
FileTransferMessageHandlerFactory::~FileTransferMessageHandlerFactory() =
    default;

void FileTransferMessageHandlerFactory::CreateDataChannelHandler(
    const std::string& channel_name,
    std::unique_ptr<protocol::MessagePipe> pipe) {
  // TODO(jarhar): Implement FileTransferMessageHandler and pass pipe to it.
}

}  // namespace remoting
