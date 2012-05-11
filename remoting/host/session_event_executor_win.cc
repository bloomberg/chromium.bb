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

#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_win.h"

namespace {

// The command line switch specifying the name of the Chromoting IPC channel.
const char kProcessChannelId[] = "chromoting-ipc";

const uint32 kUsbLeftControl =  0x0700e0;
const uint32 kUsbRightControl = 0x0700e4;
const uint32 kUsbLeftAlt =  0x0700e2;
const uint32 kUsbRightAlt = 0x0700e6;
const uint32 kUsbDelete = 0x07004c;

bool areCtrlAndAltPressed(const std::set<uint32>& pressed_keys) {
  size_t ctrl_keys = pressed_keys.count(kUsbLeftControl) +
    pressed_keys.count(kUsbRightControl);
  size_t alt_keys = pressed_keys.count(kUsbLeftAlt) +
    pressed_keys.count(kUsbRightAlt);
  return ctrl_keys != 0 && alt_keys != 0 &&
    (ctrl_keys + alt_keys == pressed_keys.size());
}

} // namespace

namespace remoting {

using protocol::ClipboardEvent;
using protocol::MouseEvent;
using protocol::KeyEvent;

SessionEventExecutorWin::SessionEventExecutorWin(
    MessageLoop* message_loop,
    base::MessageLoopProxy* io_message_loop,
    scoped_ptr<EventExecutor> nested_executor)
    : nested_executor_(nested_executor.Pass()),
      message_loop_(message_loop) {
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

void SessionEventExecutorWin::OnSessionStarted() {
  // TODO(simonmorris): Delegate to the nested executor.
}

void SessionEventExecutorWin::OnSessionFinished() {
  // TODO(simonmorris): Delegate to the nested executor.
}

void SessionEventExecutorWin::InjectClipboardEvent(
    const ClipboardEvent& event) {
  // TODO(simonmorris): Delegate to the nested executor.
}

void SessionEventExecutorWin::InjectKeyEvent(const KeyEvent& event) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SessionEventExecutorWin::InjectKeyEvent,
                   base::Unretained(this), event));
    return;
  }

  // HostEventDispatcher should drop events lacking the pressed field.
  DCHECK(event.has_pressed());

  if (event.has_usb_keycode()) {
    if (event.pressed()) {
      // Simulate secure attention sequence if Ctrl-Alt-Del was just pressed.
      if (event.usb_keycode() == kUsbDelete &&
          areCtrlAndAltPressed(pressed_keys_)) {
        VLOG(3) << "Sending Secure Attention Sequence to console";
        if (chromoting_channel_.get())
          chromoting_channel_->Send(new ChromotingHostMsg_SendSasToConsole());
      }

      pressed_keys_.insert(event.usb_keycode());
    } else {
      pressed_keys_.erase(event.usb_keycode());
    }
  }

  SwitchToInputDesktop();
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

  SwitchToInputDesktop();
  nested_executor_->InjectMouseEvent(event);
}

bool SessionEventExecutorWin::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void SessionEventExecutorWin::SwitchToInputDesktop() {
  // Switch to the desktop receiving user input if different from the current
  // one.
  scoped_ptr<DesktopWin> input_desktop = DesktopWin::GetInputDesktop();
  if (input_desktop.get() != NULL && !desktop_.IsSame(*input_desktop)) {
    // If SetThreadDesktop() fails, the thread is still assigned a desktop.
    // So we can continue capture screen bits, just from a diffected desktop.
    desktop_.SetThreadDesktop(input_desktop.Pass());
  }
}

}  // namespace remoting
