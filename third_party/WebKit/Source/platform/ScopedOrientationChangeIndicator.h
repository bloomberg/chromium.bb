// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedOrientationChangeIndicator_h
#define ScopedOrientationChangeIndicator_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class PLATFORM_EXPORT ScopedOrientationChangeIndicator final {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ScopedOrientationChangeIndicator);

 public:
  static bool ProcessingOrientationChange();

  explicit ScopedOrientationChangeIndicator();
  ~ScopedOrientationChangeIndicator();

 private:
  enum class State {
    kProcessing,
    kNotProcessing,
  };

  static State state_;

  State previous_state_ = State::kNotProcessing;
};

}  // namespace blink

#endif
