// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This stub is thread safe because of the use of BufferedSocketWriter.
// BufferedSocketWriter buffers messages and send them on them right thread.

#include "remoting/protocol/host_control_sender.h"

#include "base/task.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

HostControlSender::HostControlSender(base::MessageLoopProxy* message_loop,
                                     net::Socket* socket)
    : buffered_writer_(new BufferedSocketWriter(message_loop)) {
  buffered_writer_->Init(socket, BufferedSocketWriter::WriteFailedCallback());
}

HostControlSender::~HostControlSender() {
}

void HostControlSender::Close() {
  buffered_writer_->Close();
}

}  // namespace protocol
}  // namespace remoting
