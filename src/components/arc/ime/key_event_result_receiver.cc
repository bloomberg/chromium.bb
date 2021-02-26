// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/key_event_result_receiver.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/arc/ime/arc_ime_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"

namespace arc {

namespace {

constexpr base::TimeDelta kKeyEventDoneCallbackTimeout =
    base::TimeDelta::FromMilliseconds(100);

}  // namespace

KeyEventResultReceiver::KeyEventResultReceiver() = default;

KeyEventResultReceiver::~KeyEventResultReceiver() = default;

void KeyEventResultReceiver::DispatchKeyEventPostIME(ui::KeyEvent* event) {
  // This method is called by |ui::InputMethodChromeOS| when IME finishes
  // handling a key event coming from |ArcImeService::SendKeyEvent()|. If the
  // key event seems not to be consumed by IME, it's sent back to ARC to give it
  // to the focused View in ARC side.

  DLOG_IF(WARNING, !callback_)
      << "DispatchKeyEventPostIME is called without setting a callback";

  if (event->stopped_propagation()) {
    // The host IME wants to stop propagation of the event.
    RunCallbackIfNeeded(true);
    return;
  }

  if (event->key_code() == ui::VKEY_PROCESSKEY) {
    // This event is consumed by IME.
    RunCallbackIfNeeded(true);
    return;
  }

  if (!event->GetCharacter()) {
    // The event has no character and the host IME doesn't consume it.
    RunCallbackIfNeeded(false);
    return;
  }

  // Check whether the event is sent via |InsertChar()| later.
  // If it's sent via |InsertChar()|, let the proxy IME stop sending it to an
  // app.
  const bool sent_by_insert_char =
      !IsControlChar(event) && !ui::IsSystemKeyModifier(event->flags());
  RunCallbackIfNeeded(sent_by_insert_char);
  return;
}

void KeyEventResultReceiver::SetCallback(KeyEventDoneCallback callback) {
  // Cancel the obsolete callback if exist.
  RunCallbackIfNeeded(false);
  callback_ = std::move(callback);
  // Start expiring timer for the callback.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&KeyEventResultReceiver::ExpireCallback,
                     weak_ptr_factory_.GetWeakPtr()),
      kKeyEventDoneCallbackTimeout);
}

bool KeyEventResultReceiver::HasCallback() const {
  return !callback_.is_null();
}

void KeyEventResultReceiver::ExpireCallback() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  RunCallbackIfNeeded(false);
}

void KeyEventResultReceiver::RunCallbackIfNeeded(bool result) {
  if (callback_) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run(result);
    callback_.Reset();
  }
}

}  // namespace arc
