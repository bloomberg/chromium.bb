// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/session_event_executor.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/win/windows_version.h"
#include "media/video/capture/screen/win/desktop.h"
#include "media/video/capture/screen/win/scoped_thread_desktop.h"
#include "remoting/host/sas_injector.h"
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

class SessionEventExecutorWin::Core
    : public base::RefCountedThreadSafe<SessionEventExecutorWin::Core>,
      public EventExecutor {
 public:
  Core(
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_ptr<EventExecutor> nested_executor,
      scoped_refptr<base::SingleThreadTaskRunner> inject_sas_task_runner,
      const base::Closure& inject_sas);

  // EventExecutor implementation.
  virtual void Start(
      scoped_ptr<protocol::ClipboardStub> client_clipboard) OVERRIDE;

  // protocol::ClipboardStub implementation.
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;

  // protocol::InputStub implementation.
  virtual void InjectKeyEvent(const protocol::KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const protocol::MouseEvent& event) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core();

  // Switches to the desktop receiving a user input if different from
  // the current one.
  void SwitchToInputDesktop();

  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;

  // Pointer to the next event executor.
  scoped_ptr<EventExecutor> nested_executor_;

  scoped_refptr<base::SingleThreadTaskRunner> inject_sas_task_runner_;

  media::ScopedThreadDesktop desktop_;

  // Used to inject Secure Attention Sequence on Vista+.
  base::Closure inject_sas_;

  // Used to inject Secure Attention Sequence on XP.
  scoped_ptr<SasInjector> sas_injector_;

  // Keys currently pressed by the client, used to detect Ctrl-Alt-Del.
  std::set<uint32> pressed_keys_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

SessionEventExecutorWin::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_ptr<EventExecutor> nested_executor,
    scoped_refptr<base::SingleThreadTaskRunner> inject_sas_task_runner,
    const base::Closure& inject_sas)
    : input_task_runner_(input_task_runner),
      nested_executor_(nested_executor.Pass()),
      inject_sas_task_runner_(inject_sas_task_runner),
      inject_sas_(inject_sas) {
}

void SessionEventExecutorWin::Core::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Core::Start, this, base::Passed(&client_clipboard)));
    return;
  }

  nested_executor_->Start(client_clipboard.Pass());
}

void SessionEventExecutorWin::Core::InjectClipboardEvent(
    const ClipboardEvent& event) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::InjectClipboardEvent, this, event));
    return;
  }

  nested_executor_->InjectClipboardEvent(event);
}

void SessionEventExecutorWin::Core::InjectKeyEvent(const KeyEvent& event) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::InjectKeyEvent, this, event));
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

        if (base::win::GetVersion() < base::win::VERSION_VISTA) {
          if (!sas_injector_)
            sas_injector_ = SasInjector::Create();
          if (!sas_injector_->InjectSas())
            LOG(ERROR) << "Failed to inject Secure Attention Sequence.";
        } else {
          inject_sas_task_runner_->PostTask(FROM_HERE, inject_sas_);
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

void SessionEventExecutorWin::Core::InjectMouseEvent(const MouseEvent& event) {
  if (!input_task_runner_->BelongsToCurrentThread()) {
    input_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::InjectMouseEvent, this, event));
    return;
  }

  SwitchToInputDesktop();
  nested_executor_->InjectMouseEvent(event);
}

SessionEventExecutorWin::Core::~Core() {
}

void SessionEventExecutorWin::Core::SwitchToInputDesktop() {
  // Switch to the desktop receiving user input if different from the current
  // one.
  scoped_ptr<media::Desktop> input_desktop = media::Desktop::GetInputDesktop();
  if (input_desktop.get() != NULL && !desktop_.IsSame(*input_desktop)) {
    // If SetThreadDesktop() fails, the thread is still assigned a desktop.
    // So we can continue capture screen bits, just from a diffected desktop.
    desktop_.SetThreadDesktop(input_desktop.Pass());
  }
}

SessionEventExecutorWin::SessionEventExecutorWin(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_ptr<EventExecutor> nested_executor,
    scoped_refptr<base::SingleThreadTaskRunner> inject_sas_task_runner,
    const base::Closure& inject_sas) {
  core_ = new Core(input_task_runner, nested_executor.Pass(),
                   inject_sas_task_runner, inject_sas);
}

SessionEventExecutorWin::~SessionEventExecutorWin() {
}

void SessionEventExecutorWin::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  core_->Start(client_clipboard.Pass());
}

void SessionEventExecutorWin::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  core_->InjectClipboardEvent(event);
}

void SessionEventExecutorWin::InjectKeyEvent(const protocol::KeyEvent& event) {
  core_->InjectKeyEvent(event);
}

void SessionEventExecutorWin::InjectMouseEvent(
    const protocol::MouseEvent& event) {
  core_->InjectMouseEvent(event);
}

}  // namespace remoting
