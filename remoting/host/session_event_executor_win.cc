// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/session_event_executor_win.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"
#include "base/win/windows_version.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/proto/event.pb.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_win.h"
#include "remoting/host/scoped_thread_desktop_win.h"

namespace {

// The command line switch specifying the name of the Chromoting IPC channel.
const char kProcessChannelId[] = "chromoting-ipc";

const uint32 kUsbLeftControl =  0x0700e0;
const uint32 kUsbRightControl = 0x0700e4;
const uint32 kUsbLeftAlt =  0x0700e2;
const uint32 kUsbRightAlt = 0x0700e6;
const uint32 kUsbDelete = 0x07004c;

bool CheckCtrlAndAltArePressed(const std::set<uint32>& pressed_keys) {
  size_t ctrl_keys = pressed_keys.count(kUsbLeftControl) +
    pressed_keys.count(kUsbRightControl);
  size_t alt_keys = pressed_keys.count(kUsbLeftAlt) +
    pressed_keys.count(kUsbRightAlt);
  return ctrl_keys != 0 && alt_keys != 0 &&
    (ctrl_keys + alt_keys == pressed_keys.size());
}

// Emulates Secure Attention Sequence (Ctrl+Alt+Del) by switching to
// the Winlogon desktop and injecting Ctrl+Alt+Del as a hot key.
// N.B. Windows XP/W2K3 only.
void EmulateSecureAttentionSequence() {
  const wchar_t kWinlogonDesktopName[] = L"Winlogon";
  const wchar_t kSasWindowClassName[] = L"SAS window class";
  const wchar_t kSasWindowTitle[] = L"SAS window";

  scoped_ptr<remoting::DesktopWin> winlogon_desktop(
      remoting::DesktopWin::GetDesktop(kWinlogonDesktopName));
  if (!winlogon_desktop.get())
    return;

  remoting::ScopedThreadDesktopWin desktop;
  if (!desktop.SetThreadDesktop(winlogon_desktop.Pass()))
    return;

  HWND window = FindWindow(kSasWindowClassName, kSasWindowTitle);
  if (!window)
    return;

  PostMessage(window,
              WM_HOTKEY,
              0,
              MAKELONG(MOD_ALT | MOD_CONTROL, VK_DELETE));
}

} // namespace

namespace remoting {

using protocol::ClipboardEvent;
using protocol::MouseEvent;
using protocol::KeyEvent;

SessionEventExecutorWin::SessionEventExecutorWin(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_ptr<EventExecutor> nested_executor)
    : nested_executor_(nested_executor.Pass()),
      task_runner_(main_task_runner),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_ptr_(weak_ptr_factory_.GetWeakPtr()) {
  // Let |weak_ptr_| be used on the |task_runner_| thread.
  // |weak_ptr_| and |weak_ptr_factory_| share a ThreadChecker, so the
  // following line affects both of them.
  weak_ptr_factory_.DetachFromThread();

  std::string channel_name =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kProcessChannelId);

  // Connect to the Chromoting IPC channel if the name was passed in the command
  // line.
  if (!channel_name.empty()) {
    chromoting_channel_.reset(new IPC::ChannelProxy(
        channel_name, IPC::Channel::MODE_CLIENT, this, io_task_runner));
  }
}

SessionEventExecutorWin::~SessionEventExecutorWin() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void SessionEventExecutorWin::OnSessionStarted(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SessionEventExecutorWin::OnSessionStarted,
                   weak_ptr_, base::Passed(&client_clipboard)));
    return;
  }

  nested_executor_->OnSessionStarted(client_clipboard.Pass());
}

void SessionEventExecutorWin::OnSessionFinished() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SessionEventExecutorWin::OnSessionFinished,
                   weak_ptr_));
    return;
  }

  nested_executor_->OnSessionFinished();
}

void SessionEventExecutorWin::InjectClipboardEvent(
    const ClipboardEvent& event) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SessionEventExecutorWin::InjectClipboardEvent,
                   weak_ptr_, event));
    return;
  }

  nested_executor_->InjectClipboardEvent(event);
}

void SessionEventExecutorWin::InjectKeyEvent(const KeyEvent& event) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SessionEventExecutorWin::InjectKeyEvent,
                   weak_ptr_, event));
    return;
  }

  // HostEventDispatcher should drop events lacking the pressed field.
  DCHECK(event.has_pressed());

  if (event.has_usb_keycode()) {
    if (event.pressed()) {
      // Simulate secure attention sequence if Ctrl-Alt-Del was just pressed.
      if (event.usb_keycode() == kUsbDelete &&
          CheckCtrlAndAltArePressed(pressed_keys_)) {
        VLOG(3) << "Sending Secure Attention Sequence to console";

        if (base::win::GetVersion() == base::win::VERSION_XP) {
          EmulateSecureAttentionSequence();
        } else if (chromoting_channel_.get()) {
          chromoting_channel_->Send(new ChromotingHostMsg_SendSasToConsole());
        }
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
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SessionEventExecutorWin::InjectMouseEvent,
                   weak_ptr_, event));
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
