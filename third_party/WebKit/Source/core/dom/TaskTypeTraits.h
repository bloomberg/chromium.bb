// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TaskTypeTraits_h
#define TaskTypeTraits_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashTraits.h"
#include "public/platform/TaskType.h"

namespace blink {

// HashTraits for TaskType.
struct TaskTypeTraits : WTF::GenericHashTraits<TaskType> {
  static const bool kEmptyValueIsZero = false;
  static TaskType EmptyValue() { return static_cast<TaskType>(-1); }
  static void ConstructDeletedValue(TaskType& slot, bool) {
    slot = static_cast<TaskType>(-2);
  }
  static bool IsDeletedValue(TaskType value) {
    return value == static_cast<TaskType>(-2);
  }
};

}  // namespace blink

#endif
