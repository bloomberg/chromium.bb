// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_IME_KEY_EVENT_RESULT_RECEIVER_H_
#define COMPONENTS_ARC_IME_KEY_EVENT_RESULT_RECEIVER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/events/event.h"

namespace arc {

class KeyEventResultReceiver {
 public:
  using KeyEventDoneCallback = base::OnceCallback<void(bool)>;

  KeyEventResultReceiver();
  KeyEventResultReceiver(const KeyEventResultReceiver&) = delete;
  ~KeyEventResultReceiver();

  // Called when the host IME receives the event and passes it to the later
  // stage. This method has to call the callback exactly once.
  // Returns true when |key_event| is the waiting key event and the callback is
  // called. Returns false otherwise.
  bool DispatchKeyEventPostIME(ui::KeyEvent* key_event);

  void SetCallback(KeyEventDoneCallback callback, const ui::KeyEvent* event);
  bool HasCallback() const;

 private:
  // Called when |DispatchKeyEventPostIME| has not called before timeout.
  void ExpireCallback();

  void RunCallbackIfNeeded(bool result);

  void RecordImeLatency();

  KeyEventDoneCallback callback_{};
  absl::optional<ui::KeyEvent> expected_key_event_{};
  absl::optional<base::TimeTicks> callback_set_time_{};
  base::WeakPtrFactory<KeyEventResultReceiver> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // COMPONENTS_ARC_IME_KEY_EVENT_RESULT_RECEIVER_H_
