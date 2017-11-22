// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PausableScriptExecutor_h
#define PausableScriptExecutor_h

#include "base/memory/scoped_refptr.h"
#include "core/CoreExport.h"
#include "core/frame/PausableTimer.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/heap/Handle.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/wtf/Vector.h"
#include "v8/include/v8.h"

namespace blink {

class LocalFrame;
class ScriptSourceCode;
class ScriptState;
class WebScriptExecutionCallback;

class CORE_EXPORT PausableScriptExecutor final
    : public GarbageCollectedFinalized<PausableScriptExecutor>,
      public PausableTimer {
  USING_GARBAGE_COLLECTED_MIXIN(PausableScriptExecutor);

 public:
  enum BlockingOption { kNonBlocking, kOnloadBlocking };

  static PausableScriptExecutor* Create(
      LocalFrame*,
      scoped_refptr<DOMWrapperWorld>,
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
  ~PausableScriptExecutor() override;

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
  PausableScriptExecutor(LocalFrame*,
                         ScriptState*,
                         WebScriptExecutionCallback*,
                         Executor*);

  void Fired() override;

  void ExecuteAndDestroySelf();
  void Dispose();

  scoped_refptr<ScriptState> script_state_;
  WebScriptExecutionCallback* callback_;
  BlockingOption blocking_option_;

  SelfKeepAlive<PausableScriptExecutor> keep_alive_;

  Member<Executor> executor_;
};

}  // namespace blink

#endif  // PausableScriptExecutor_h
