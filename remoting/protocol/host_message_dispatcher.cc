// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop_proxy.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/host_message_dispatcher.h"
#include "remoting/protocol/host_control_message_handler.h"
#include "remoting/protocol/host_event_message_handler.h"
#include "remoting/protocol/stream_reader.h"

namespace remoting {

HostMessageDispatcher::HostMessageDispatcher(
    base::MessageLoopProxy* message_loop_proxy,
    ChromotingConnection* connection,
    HostControlMessageHandler* control_message_handler,
    HostEventMessageHandler* event_message_handler)
    : message_loop_proxy_(message_loop_proxy),
      control_message_handler_(control_message_handler),
      event_message_handler_(event_message_handler) {
}

HostMessageDispatcher::~HostMessageDispatcher() {
}

}  // namespace remoting
