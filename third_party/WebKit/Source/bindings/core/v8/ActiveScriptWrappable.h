// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ActiveScriptWrappable_h
#define ActiveScriptWrappable_h

#include "core/execution_context/ExecutionContext.h"
#include "platform/bindings/ActiveScriptWrappableBase.h"

namespace blink {

class ScriptWrappable;

template <typename T>
class ActiveScriptWrappable : public ActiveScriptWrappableBase {
  WTF_MAKE_NONCOPYABLE(ActiveScriptWrappable);

 public:
  ~ActiveScriptWrappable() override = default;

 protected:
  ActiveScriptWrappable() = default;

  bool IsContextDestroyed() const final {
    const auto* execution_context =
        static_cast<const T*>(this)->GetExecutionContext();
    return !execution_context || execution_context->IsContextDestroyed();
  }

  bool DispatchHasPendingActivity() const final {
    return static_cast<const T*>(this)->HasPendingActivity();
  }

  ScriptWrappable* ToScriptWrappable() final { return static_cast<T*>(this); }
};

}  // namespace blink

#endif  // ActiveScriptWrappable_h
