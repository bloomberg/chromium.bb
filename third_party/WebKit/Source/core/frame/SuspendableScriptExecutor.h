// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SuspendableScriptExecutor_h
#define SuspendableScriptExecutor_h

#include "core/CoreExport.h"
#include "core/frame/SuspendableTimer.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/heap/Handle.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "v8/include/v8.h"

namespace blink {

class LocalFrame;
class ScriptSourceCode;
class ScriptState;
class WebScriptExecutionCallback;

class CORE_EXPORT SuspendableScriptExecutor final
    : public GarbageCollectedFinalized<SuspendableScriptExecutor>,
      public SuspendableTimer {
  USING_GARBAGE_COLLECTED_MIXIN(SuspendableScriptExecutor);

 public:
  enum BlockingOption { kNonBlocking, kOnloadBlocking };

  static SuspendableScriptExecutor* Create(
      LocalFrame*,
      RefPtr<DOMWrapperWorld>,
      const HeapVector<ScriptSourceCode>& sources,
      bool user_gesture,
      WebScriptExecutionCallback*);
  static void CreateAndRun(LocalFrame*,
                           v8::Isolate*,
                           v8::Local<v8::Context>,
                           v8::Local<v8::Function>,
                           v8::Local<v8::Value> receiver,
                           int argc,
                           v8::Local<v8::Value> argv[],
                           WebScriptExecutionCallback*);
  ~SuspendableScriptExecutor() override;

  void Run();
  void RunAsync(BlockingOption);
  void ContextDestroyed(ExecutionContext*) override;

  virtual void Trace(blink::Visitor*);

  class Executor : public GarbageCollectedFinalized<Executor> {
   public:
    virtual ~Executor() {}

    virtual Vector<v8::Local<v8::Value>> Execute(LocalFrame*) = 0;

    virtual void Trace(blink::Visitor* visitor){};
  };

 private:
  SuspendableScriptExecutor(LocalFrame*,
                            ScriptState*,
                            WebScriptExecutionCallback*,
                            Executor*);

  void Fired() override;

  void ExecuteAndDestroySelf();
  void Dispose();

  RefPtr<ScriptState> script_state_;
  WebScriptExecutionCallback* callback_;
  BlockingOption blocking_option_;

  SelfKeepAlive<SuspendableScriptExecutor> keep_alive_;

  Member<Executor> executor_;
};

}  // namespace blink

#endif  // SuspendableScriptExecutor_h
