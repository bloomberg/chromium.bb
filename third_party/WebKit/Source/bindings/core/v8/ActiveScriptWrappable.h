// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ActiveScriptWrappable_h
#define ActiveScriptWrappable_h

#include "core/CoreExport.h"
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
class CORE_EXPORT ActiveScriptWrappableBase : public GarbageCollectedMixin {
  WTF_MAKE_NONCOPYABLE(ActiveScriptWrappableBase);

 public:
  ActiveScriptWrappableBase();

  static void TraceActiveScriptWrappables(v8::Isolate*,
                                          ScriptWrappableVisitor*);

 protected:
  virtual bool IsContextDestroyed(ActiveScriptWrappableBase*) const = 0;
  virtual bool DispatchHasPendingActivity(ActiveScriptWrappableBase*) const = 0;
  virtual ScriptWrappable* ToScriptWrappable(
      ActiveScriptWrappableBase*) const = 0;
};

template <typename T>
class ActiveScriptWrappable : public ActiveScriptWrappableBase {
  WTF_MAKE_NONCOPYABLE(ActiveScriptWrappable);

 public:
  ActiveScriptWrappable() {}

 protected:
  bool IsContextDestroyed(ActiveScriptWrappableBase* object) const final {
    return !(static_cast<T*>(object)->T::GetExecutionContext)() ||
           (static_cast<T*>(object)->T::GetExecutionContext)()
               ->IsContextDestroyed();
  }

  bool DispatchHasPendingActivity(
      ActiveScriptWrappableBase* object) const final {
    return static_cast<T*>(object)->T::HasPendingActivity();
  }

  ScriptWrappable* ToScriptWrappable(
      ActiveScriptWrappableBase* object) const final {
    return static_cast<T*>(object);
  }
};

}  // namespace blink

#endif  // ActiveScriptWrappable_h
