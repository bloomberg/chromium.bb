// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/session_event_executor.h"

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/win/windows_version.h"
#include "remoting/host/sas_injector.h"
#include "remoting/host/win/desktop.h"
#include "remoting/proto/event.pb.h"

namespace {

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

} // namespace

namespace remoting {

using protocol::ClipboardEvent;
using protocol::MouseEvent;
using protocol::KeyEvent;

SessionEventExecutorWin::SessionEventExecutorWin(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_ptr<EventExecutor> nested_executor)
    : nested_executor_(nested_executor.Pass()),
      task_runner_(main_task_runner),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_ptr_(weak_ptr_factory_.GetWeakPtr()) {
  // Let |weak_ptr_| be used on the |task_runner_| thread.
  // |weak_ptr_| and |weak_ptr_factory_| share a ThreadChecker, so the
  // following line affects both of them.
  weak_ptr_factory_.DetachFromThread();
}

SessionEventExecutorWin::~SessionEventExecutorWin() {
}

void SessionEventExecutorWin::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SessionEventExecutorWin::Start,
                   weak_ptr_, base::Passed(&client_clipboard)));
    return;
  }

  nested_executor_->Start(client_clipboard.Pass());
}

void SessionEventExecutorWin::StopAndDelete() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SessionEventExecutorWin::StopAndDelete,
                   weak_ptr_));
    return;
  }

  nested_executor_.release()->StopAndDelete();
  delete this;
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

        if (sas_injector_.get() == NULL)
          sas_injector_ = SasInjector::Create();
        sas_injector_->InjectSas();
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

void SessionEventExecutorWin::SwitchToInputDesktop() {
  // Switch to the desktop receiving user input if different from the current
  // one.
  scoped_ptr<Desktop> input_desktop = Desktop::GetInputDesktop();
  if (input_desktop.get() != NULL && !desktop_.IsSame(*input_desktop)) {
    // If SetThreadDesktop() fails, the thread is still assigned a desktop.
    // So we can continue capture screen bits, just from a diffected desktop.
    desktop_.SetThreadDesktop(input_desktop.Pass());
  }
}

}  // namespace remoting
