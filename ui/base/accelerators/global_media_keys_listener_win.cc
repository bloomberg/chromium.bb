// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_media_keys_listener_win.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/keyboard_hook.h"

namespace ui {

// static
bool GlobalMediaKeysListenerWin::has_instance_ = false;

GlobalMediaKeysListenerWin::GlobalMediaKeysListenerWin(
    MediaKeysListener::Delegate* delegate)
    : delegate_(delegate), thread_("KeyboardHookThread") {
  DCHECK(delegate_);
  DCHECK(!has_instance_);
  has_instance_ = true;
}

GlobalMediaKeysListenerWin::~GlobalMediaKeysListenerWin() {
  // This will reset |hook_| (if necessary) and then join on |thread_| if it's
  // currently running. This guarantees that |hook_| will be empty before its
  // destructor is called on this thread, so we will avoid accessing |hook_| on
  // this thread.
  StopWatchingMediaKeys();
  has_instance_ = false;
}

void GlobalMediaKeysListenerWin::StartWatchingMediaKeys() {
  // Create the keyboard hook if it doesn't already exist.
  if (!thread_.IsRunning()) {
    bool started = thread_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_UI, 0));
    LOG_IF(ERROR, !started) << "failed to start KeyboardHookThread";
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&GlobalMediaKeysListenerWin::EnsureHookOnInternalThread,
                       base::Unretained(this)));
  }
}

void GlobalMediaKeysListenerWin::StopWatchingMediaKeys() {
  // Delete the keyboard hook if it exists.
  if (thread_.IsRunning()) {
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&GlobalMediaKeysListenerWin::DeleteHookOnInternalThread,
                       base::Unretained(this)));
    thread_.Stop();
  }
}

bool GlobalMediaKeysListenerWin::IsWatchingMediaKeys() const {
  return thread_.IsRunning();
}

void GlobalMediaKeysListenerWin::EnsureHookOnInternalThread() {
  DCHECK(thread_.task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!hook_);
  hook_ = KeyboardHook::CreateMediaKeyboardHook(base::BindRepeating(
      &GlobalMediaKeysListenerWin::KeyEventCallbackOnInternalThread,
      base::Unretained(this)));

  if (hook_) {
    // Notify the delegate that the keyboard hook has been registered.
    delegate_->OnStartedWatchingMediaKeys();
  } else {
    LOG(ERROR) << "failed to create keyboard hook";
  }
}

void GlobalMediaKeysListenerWin::DeleteHookOnInternalThread() {
  DCHECK(thread_.task_runner()->RunsTasksInCurrentSequence());
  hook_.reset();
}

void GlobalMediaKeysListenerWin::KeyEventCallbackOnInternalThread(
    KeyEvent* event) {
  DCHECK(thread_.task_runner()->RunsTasksInCurrentSequence());
  Accelerator::KeyState state = (event->type() == ET_KEY_PRESSED)
                                    ? Accelerator::KeyState::PRESSED
                                    : Accelerator::KeyState::RELEASED;
  const ui::Accelerator accelerator(event->key_code(), 0, state);
  if (delegate_->OnMediaKeysAccelerator(accelerator) ==
      MediaKeysListener::MediaKeysHandleResult::kSuppressPropagation) {
    // Marking the event as handled will prevent propagation in the
    // KeyboardHook.
    event->SetHandled();
  }
}

}  // namespace ui