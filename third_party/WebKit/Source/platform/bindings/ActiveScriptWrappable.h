// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ActiveScriptWrappable_h
#define ActiveScriptWrappable_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace v8 {
class Isolate;
}

namespace blink {

class ScriptWrappable;
class ScriptWrappableVisitor;

/**
 * Classes deriving from ActiveScriptWrappable will be registered in a
 * thread-specific list. They keep their wrappers and dependant objects alive
 * as long as they have pending activity.
 */
class PLATFORM_EXPORT ActiveScriptWrappableBase : public GarbageCollectedMixin {
  WTF_MAKE_NONCOPYABLE(ActiveScriptWrappableBase);

 public:
  ActiveScriptWrappableBase();

  static void TraceActiveScriptWrappables(v8::Isolate*,
                                          ScriptWrappableVisitor*);

 protected:
  virtual bool IsContextDestroyed() const = 0;
  virtual bool DispatchHasPendingActivity() const = 0;
  virtual ScriptWrappable* ToScriptWrappable() = 0;
};

template <typename T>
class ActiveScriptWrappable : public ActiveScriptWrappableBase {
  WTF_MAKE_NONCOPYABLE(ActiveScriptWrappable);

 public:
  ActiveScriptWrappable() {}

 protected:
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
