// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IdleDeadline_h
#define IdleDeadline_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT IdleDeadline : public GarbageCollected<IdleDeadline>,
                                 public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class CallbackType { kCalledWhenIdle, kCalledByTimeout };

  static IdleDeadline* Create(double deadline_seconds,
                              CallbackType callback_type) {
    return new IdleDeadline(deadline_seconds, callback_type);
  }

  void Trace(blink::Visitor* visitor) {}

  double timeRemaining() const;

  bool didTimeout() const {
    return callback_type_ == CallbackType::kCalledByTimeout;
  }

 private:
  IdleDeadline(double deadline_seconds, CallbackType);

  double deadline_seconds_;
  CallbackType callback_type_;
};

}  // namespace blink

#endif  // IdleDeadline_h
