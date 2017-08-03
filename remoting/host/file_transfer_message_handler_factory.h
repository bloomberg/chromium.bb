// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_MESSAGE_HANDLER_FACTORY_H_
#define REMOTING_HOST_FILE_TRANSFER_MESSAGE_HANDLER_FACTORY_H_

#include <memory>
#include <string>

#include "remoting/protocol/message_pipe.h"

namespace remoting {

constexpr char kFileTransferDataChannelPrefix[] = "filetransfer-";

class FileTransferMessageHandlerFactory final {
 public:
  FileTransferMessageHandlerFactory();
  ~FileTransferMessageHandlerFactory();

  void CreateDataChannelHandler(const std::string& channel_name,
                                std::unique_ptr<protocol::MessagePipe> pipe);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_MESSAGE_HANDLER_FACTORY_H_
