// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_MESSAGE_HANDLER_H_
#define REMOTING_HOST_FILE_TRANSFER_MESSAGE_HANDLER_H_

#include "remoting/protocol/named_message_pipe_handler.h"

namespace remoting {

class FileTransferMessageHandler : public protocol::NamedMessagePipeHandler {
 public:
  FileTransferMessageHandler(const std::string& name,
                             std::unique_ptr<protocol::MessagePipe> pipe);
  ~FileTransferMessageHandler() override;

  // protocol::NamedMessagePipeHandler implementation.
  void OnConnected() override;
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;
  void OnDisconnecting() override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_MESSAGE_HANDLER_H_
