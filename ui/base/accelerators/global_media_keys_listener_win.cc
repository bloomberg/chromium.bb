// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_media_keys_listener_win.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/keyboard_hook.h"

namespace ui {

// static
bool GlobalMediaKeysListenerWin::has_instance_ = false;

GlobalMediaKeysListenerWin::GlobalMediaKeysListenerWin(
    MediaKeysListener::Delegate* delegate)
    : delegate_(delegate),
      thread_("KeyboardHookThread"),
      main_thread_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DCHECK(delegate_);
  DCHECK(!has_instance_);
  has_instance_ = true;
}

GlobalMediaKeysListenerWin::~GlobalMediaKeysListenerWin() {
  // This will reset |hook_| (if necessary) and then join on |thread_| if it's
  // currently running. This guarantees that |hook_| will be empty before its
  // destructor is called on this thread, so we will avoid accessing |hook_| on
  // this thread.
  DeleteKeyboardHookIfNecessary();
  has_instance_ = false;
}

void GlobalMediaKeysListenerWin::StartWatchingMediaKey(KeyboardCode key_code) {
  DCHECK(IsMediaKeycode(key_code));

  {
    base::AutoLock lock(key_codes_lock_);
    key_codes_.insert(key_code);
  }
  CreateKeyboardHookIfNecessary();
}

void GlobalMediaKeysListenerWin::StopWatchingMediaKey(KeyboardCode key_code) {
  DCHECK(IsMediaKeycode(key_code));

  bool should_delete;
  // Don't hold the lock when we delete the hook, or else we may deadlock while
  // joining on |thread_| if |thread_| attempts to access |key_codes_|.
  {
    base::AutoLock lock(key_codes_lock_);
    key_codes_.erase(key_code);
    should_delete = key_codes_.empty();
  }

  // If we're not watching any more keys, delete the hook.
  if (should_delete)
    DeleteKeyboardHookIfNecessary();
}

void GlobalMediaKeysListenerWin::CreateKeyboardHookIfNecessary() {
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

void GlobalMediaKeysListenerWin::DeleteKeyboardHookIfNecessary() {
  // Delete the keyboard hook if it exists.
  if (thread_.IsRunning()) {
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&GlobalMediaKeysListenerWin::DeleteHookOnInternalThread,
                       base::Unretained(this)));
    thread_.Stop();
  }
}

void GlobalMediaKeysListenerWin::EnsureHookOnInternalThread() {
  DCHECK(thread_.task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!hook_);
  hook_ = KeyboardHook::CreateMediaKeyboardHook(base::BindRepeating(
      &GlobalMediaKeysListenerWin::KeyEventCallbackOnInternalThread,
      base::Unretained(this)));

  if (hook_) {
    // Notify the delegate that we're ready to receive events for that key.
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &GlobalMediaKeysListenerWin::OnStartedWatchingMediaKeysOnMainThread,
            weak_factory_.GetWeakPtr()));
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

  {
    // Ignore key events that we don't care about.
    base::AutoLock lock(key_codes_lock_);
    if (!key_codes_.contains(event->key_code()))
      return;
  }

  Accelerator::KeyState state = (event->type() == ET_KEY_PRESSED)
                                    ? Accelerator::KeyState::PRESSED
                                    : Accelerator::KeyState::RELEASED;
  const ui::Accelerator accelerator(event->key_code(), 0, state);

  // Send the key event to the delegate on the main thread.
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GlobalMediaKeysListenerWin::KeyEventCallbackOnMainThread,
                     weak_factory_.GetWeakPtr(), accelerator));

  // Marking the event as handled will prevent propagation in the
  // KeyboardHook.
  event->SetHandled();
}

void GlobalMediaKeysListenerWin::KeyEventCallbackOnMainThread(
    const Accelerator& accelerator) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  // Don't hold the lock for |delegate_|'s callback in case delegate_ calls
  // Start/StopWatchingMediaKey().
  {
    // If we get a key event that we no longer care about, ignore it.
    base::AutoLock lock(key_codes_lock_);
    if (!key_codes_.contains(accelerator.key_code()))
      return;
  }

  delegate_->OnMediaKeysAccelerator(accelerator);
}

void GlobalMediaKeysListenerWin::OnStartedWatchingMediaKeysOnMainThread() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  delegate_->OnStartedWatchingMediaKeys();
}

}  // namespace ui