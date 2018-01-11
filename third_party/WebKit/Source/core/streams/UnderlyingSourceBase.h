// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnderlyingSourceBase_h
#define UnderlyingSourceBase_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"

namespace blink {

class ReadableStreamDefaultControllerWrapper;

class CORE_EXPORT UnderlyingSourceBase
    : public ScriptWrappable,
      public ActiveScriptWrappable<UnderlyingSourceBase>,
      public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(UnderlyingSourceBase);

 public:
  void Trace(blink::Visitor*) override;
  virtual ~UnderlyingSourceBase() = default;

  ScriptPromise startWrapper(ScriptState*, ScriptValue stream);
  virtual ScriptPromise Start(ScriptState*);

  virtual ScriptPromise pull(ScriptState*);

  ScriptPromise cancelWrapper(ScriptState*, ScriptValue reason);
  virtual ScriptPromise Cancel(ScriptState*, ScriptValue reason);

  void notifyLockAcquired();
  void notifyLockReleased();

  // ScriptWrappable
  bool HasPendingActivity() const;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

 protected:
  explicit UnderlyingSourceBase(ScriptState* script_state)
      : ContextLifecycleObserver(ExecutionContext::From(script_state)) {}

  ReadableStreamDefaultControllerWrapper* Controller() const {
    return controller_;
  }

 private:
  Member<ReadableStreamDefaultControllerWrapper> controller_;
  bool is_stream_locked_ = false;
};

}  // namespace blink

#endif  // UnderlyingSourceBase_h
