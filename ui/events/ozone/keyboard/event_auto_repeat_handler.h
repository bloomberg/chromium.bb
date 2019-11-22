// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_KEYBOARD_EVENT_AUTO_REPEAT_HANDLER_H
#define UI_EVENTS_OZONE_KEYBOARD_EVENT_AUTO_REPEAT_HANDLER_H

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace ui {

class COMPONENT_EXPORT(EVENTS_OZONE) EventAutoRepeatHandler {
 public:
  class Delegate {
   public:
    // Gives the client a chance to flush the input queue
    // cancelling possible spurios auto repeat keys.
    // Useful under janky situations.
    virtual void FlushInput(base::OnceClosure closure) = 0;
    virtual void DispatchKey(unsigned int key,
                             bool down,
                             bool repeat,
                             base::TimeTicks timestamp,
                             int device_id) = 0;
  };

  explicit EventAutoRepeatHandler(Delegate* delegate);
  ~EventAutoRepeatHandler();

  void UpdateKeyRepeat(unsigned int key,
                       bool down,
                       bool suppress_auto_repeat,
                       int device_id);
  void StopKeyRepeat();

  // Configuration for key repeat.
  bool IsAutoRepeatEnabled();
  void SetAutoRepeatEnabled(bool enabled);
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval);
  void GetAutoRepeatRate(base::TimeDelta* delay, base::TimeDelta* interval);

 private:
  static constexpr unsigned int kInvalidKey = 0;

  void StartKeyRepeat(unsigned int key, int device_id);
  void ScheduleKeyRepeat(const base::TimeDelta& delay);
  void OnRepeatTimeout(unsigned int sequence);
  void OnRepeatCommit(unsigned int sequence);

  // Key repeat state.
  bool auto_repeat_enabled_ = true;
  unsigned int repeat_key_ = kInvalidKey;
  unsigned int repeat_sequence_ = 0;
  int repeat_device_id_ = 0;
  base::TimeDelta repeat_delay_;
  base::TimeDelta repeat_interval_;

  Delegate* delegate_ = nullptr;

  base::WeakPtrFactory<EventAutoRepeatHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EventAutoRepeatHandler);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_KEYBOARD_EVENT_AUTO_REPEAT_HANDLER_H
