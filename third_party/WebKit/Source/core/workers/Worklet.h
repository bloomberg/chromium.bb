// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Worklet_h
#define Worklet_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/loader/WorkletScriptLoader.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

class LocalFrame;
class WorkletGlobalScopeProxy;

class CORE_EXPORT Worklet : public GarbageCollectedFinalized<Worklet>,
                            public WorkletScriptLoader::Client,
                            public ScriptWrappable,
                            public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Worklet);
  WTF_MAKE_NONCOPYABLE(Worklet);

 public:
  virtual ~Worklet() = default;

  virtual void initialize() {}
  virtual bool isInitialized() const { return true; }

  virtual WorkletGlobalScopeProxy* workletGlobalScopeProxy() const = 0;

  // Worklet
  ScriptPromise import(ScriptState*, const String& url);

  // WorkletScriptLoader::Client
  void notifyWorkletScriptLoadingFinished(WorkletScriptLoader*,
                                          const ScriptSourceCode&) final;

  // ContextLifecycleObserver
  void contextDestroyed(ExecutionContext*) final;

  DECLARE_VIRTUAL_TRACE();

 protected:
  // The Worklet inherits the url and userAgent from the frame->document().
  explicit Worklet(LocalFrame*);

 private:
  Member<LocalFrame> m_frame;
  HeapHashMap<Member<WorkletScriptLoader>, Member<ScriptPromiseResolver>>
      m_loaderAndResolvers;
};

}  // namespace blink

#endif  // Worklet_h
