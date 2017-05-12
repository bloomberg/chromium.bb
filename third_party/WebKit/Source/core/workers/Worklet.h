// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Worklet_h
#define Worklet_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/workers/WorkletOptions.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ScriptPromiseResolver;
class WorkletGlobalScopeProxy;

// This is the base implementation of Worklet interface defined in the spec:
// https://drafts.css-houdini.org/worklets/#worklet
// Although some worklets run off the main thread, this must be created and
// destroyed on the main thread.
class CORE_EXPORT Worklet : public GarbageCollectedFinalized<Worklet>,
                            public ScriptWrappable,
                            public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Worklet);
  WTF_MAKE_NONCOPYABLE(Worklet);

 public:
  virtual ~Worklet() = default;

  // Worklet.idl
  // addModule() imports ES6 module scripts.
  virtual ScriptPromise addModule(ScriptState*,
                                  const String& module_url,
                                  const WorkletOptions&);

  // Returns a proxy to WorkletGlobalScope on the context thread.
  virtual WorkletGlobalScopeProxy* GetWorkletGlobalScopeProxy() const = 0;

  DECLARE_VIRTUAL_TRACE();

 protected:
  // The Worklet inherits the url and userAgent from the frame->document().
  explicit Worklet(LocalFrame*);

 private:
  virtual void FetchAndInvokeScript(const KURL& module_url_record,
                                    const WorkletOptions&,
                                    ScriptPromiseResolver*) = 0;
};

}  // namespace blink

#endif  // Worklet_h
