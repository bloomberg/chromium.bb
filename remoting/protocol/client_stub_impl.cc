// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This stub is thread safe because of the use of BufferedSocketWriter.
// BufferedSocketWriter buffers messages and send them on them right thread.

#include "remoting/protocol/client_stub_impl.h"

#include "base/task.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

ClientStubImpl::ClientStubImpl(net::Socket* socket)
    : buffered_writer_(new BufferedSocketWriter()) {
  buffered_writer_->Init(socket, NULL);
}

ClientStubImpl::~ClientStubImpl() {
}

void ClientStubImpl::NotifyResolution(
    const NotifyResolutionRequest& msg, Task* done) {
  ControlMessage message;
  message.mutable_notify_resolution()->CopyFrom(msg);
  buffered_writer_->Write(SerializeAndFrameMessage(message));
  if (done) {
    done->Run();
    delete done;
  }
}

}  // namespace protocol
}  // namespace remoting
