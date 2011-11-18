// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/host_control_dispatcher.h"

#include "base/message_loop_proxy.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

HostControlDispatcher::HostControlDispatcher()
    : host_stub_(NULL),
      writer_(new BufferedSocketWriter(base::MessageLoopProxy::current())) {
}

HostControlDispatcher::~HostControlDispatcher() {
  writer_->Close();
}

void HostControlDispatcher::Init(Session* session) {
  DCHECK(session);

  reader_.Init(session->control_channel(), base::Bind(
      &HostControlDispatcher::OnMessageReceived, base::Unretained(this)));
  writer_->Init(session->control_channel(),
                BufferedSocketWriter::WriteFailedCallback());

  // Write legacy BeginSession message.
  // TODO(sergeyu): Remove it. See http://crbug.com/104670 .
  protocol::ControlMessage message;
  message.mutable_begin_session_deprecated()->mutable_login_status()->
      set_success(true);
  writer_->Write(SerializeAndFrameMessage(message), base::Closure());
}

void HostControlDispatcher::OnMessageReceived(
    ControlMessage* message, const base::Closure& done_task) {
  DCHECK(host_stub_);
  LOG(WARNING) << "Unknown control message received.";
  done_task.Run();
}

}  // namespace protocol
}  // namespace remoting
