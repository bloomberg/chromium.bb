// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/client_control_dispatcher.h"

#include "base/message_loop_proxy.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/client_stub.h"

namespace remoting {
namespace protocol {

ClientControlDispatcher::ClientControlDispatcher()
    : ChannelDispatcherBase(kControlChannelName),
      client_stub_(NULL),
      writer_(new BufferedSocketWriter(base::MessageLoopProxy::current())) {
}

ClientControlDispatcher::~ClientControlDispatcher() {
  writer_->Close();
}

void ClientControlDispatcher::OnInitialized() {
  // TODO(garykac): Set write failed callback.
  writer_->Init(channel(), BufferedSocketWriter::WriteFailedCallback());
  reader_.Init(channel(), base::Bind(
      &ClientControlDispatcher::OnMessageReceived, base::Unretained(this)));
}

void ClientControlDispatcher::OnMessageReceived(
    ControlMessage* message, const base::Closure& done_task) {
  DCHECK(client_stub_);

  base::ScopedClosureRunner done_runner(done_task);

  if (message->has_begin_session_deprecated()) {
    // Host sends legacy BeginSession message for compatibility with
    // older clients. Ignore it without warning.
  } else {
    LOG(WARNING) << "Unknown control message received.";
  }
}

}  // namespace protocol
}  // namespace remoting
