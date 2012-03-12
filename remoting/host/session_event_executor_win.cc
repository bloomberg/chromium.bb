// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/session_event_executor_win.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/proto/event.pb.h"
#include "ui/base/keycodes/keyboard_codes.h"

#include "remoting/host/chromoting_messages.h"

namespace {

// The command line switch specifying the name of the session IPC channel.
const char kProcessChannelId[] = "channel";

} // namespace

namespace remoting {

using protocol::MouseEvent;
using protocol::KeyEvent;

SessionEventExecutorWin::SessionEventExecutorWin(
    MessageLoop* message_loop,
    base::MessageLoopProxy* io_message_loop,
    scoped_ptr<protocol::InputStub> nested_executor)
    : nested_executor_(nested_executor.Pass()),
      message_loop_(message_loop),
      scroll_pressed_(false) {
  std::string channel_name =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kProcessChannelId);

  // Connect to the Chromoting IPC channel if the name was passed in the command
  // line.
  if (!channel_name.empty()) {
    chromoting_channel_.reset(new IPC::ChannelProxy(
                                      channel_name,
                                      IPC::Channel::MODE_CLIENT,
                                      this,
                                      io_message_loop));
  }
}

SessionEventExecutorWin::~SessionEventExecutorWin() {
}

void SessionEventExecutorWin::InjectKeyEvent(const KeyEvent& event) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SessionEventExecutorWin::InjectKeyEvent,
                   base::Unretained(this), event));
    return;
  }

  // Poor man's Ctrl+Alt+Delete emulation: a double Scroll Lock is converted to
  // the Secure Attention Sequence.
  // TODO(alexeypa): replace this with proper SAS handling.
  if (chromoting_channel_.get() != NULL && event.keycode() == VK_SCROLL) {
    if (event.pressed()) {
      if (scroll_pressed_) {
        chromoting_channel_->Send(new ChromotingHostMsg_SendSasToConsole());
        scroll_pressed_ = false;
      } else {
        scroll_pressed_ = true;
      }
    }
  } else {
    scroll_pressed_ = false;
  }

  nested_executor_->InjectKeyEvent(event);
}

void SessionEventExecutorWin::InjectMouseEvent(const MouseEvent& event) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SessionEventExecutorWin::InjectMouseEvent,
                   base::Unretained(this), event));
    return;
  }

  nested_executor_->InjectMouseEvent(event);
}

bool SessionEventExecutorWin::OnMessageReceived(const IPC::Message& message) {
  return false;
}

}  // namespace remoting
