// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/host_event_dispatcher.h"

#include "base/callback_helpers.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/input_stub.h"

namespace remoting {
namespace protocol {

HostEventDispatcher::HostEventDispatcher()
    : ChannelDispatcherBase(kEventChannelName),
      input_stub_(NULL) {
}

HostEventDispatcher::~HostEventDispatcher() {
}

void HostEventDispatcher::OnInitialized() {
  reader_.Init(channel(), base::Bind(
      &HostEventDispatcher::OnMessageReceived, base::Unretained(this)));
}

void HostEventDispatcher::OnMessageReceived(
    scoped_ptr<EventMessage> message, const base::Closure& done_task) {
  DCHECK(input_stub_);

  base::ScopedClosureRunner done_runner(done_task);

  if (message->has_sequence_number() && !sequence_number_callback_.is_null())
    sequence_number_callback_.Run(message->sequence_number());

  if (message->has_key_event()) {
    const KeyEvent& event = message->key_event();
    if (event.has_usb_keycode() && event.has_pressed()) {
      input_stub_->InjectKeyEvent(event);
    } else {
      LOG(WARNING) << "Received invalid key event.";
    }
  } else if (message->has_text_event()) {
    const TextEvent& event = message->text_event();
    if (event.has_text()) {
      input_stub_->InjectTextEvent(event);
    } else {
      LOG(WARNING) << "Received invalid text event.";
    }
  } else if (message->has_mouse_event()) {
    input_stub_->InjectMouseEvent(message->mouse_event());
  } else {
    LOG(WARNING) << "Unknown event message received.";
  }
}

}  // namespace protocol
}  // namespace remoting
