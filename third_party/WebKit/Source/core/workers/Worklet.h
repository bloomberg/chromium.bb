// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Worklet_h
#define Worklet_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "core/workers/WorkletModuleResponsesMap.h"
#include "core/workers/WorkletOptions.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ScriptPromiseResolver;

// This is the base implementation of Worklet interface defined in the spec:
// https://drafts.css-houdini.org/worklets/#worklet
// Although some worklets run off the main thread, this must be created and
// destroyed on the main thread.
class CORE_EXPORT Worklet : public GarbageCollectedFinalized<Worklet>,
                            public ScriptWrappable,
                            public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Worklet);
  // Eager finalization is needed to notify parent object destruction of the
  // GC-managed messaging proxy and to initiate worklet termination.
  EAGERLY_FINALIZE();
  WTF_MAKE_NONCOPYABLE(Worklet);

 public:
  virtual ~Worklet();

  // Worklet.idl
  // addModule() imports ES6 module scripts.
  ScriptPromise addModule(ScriptState*,
                          const String& module_url,
                          const WorkletOptions&);

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  virtual void Trace(blink::Visitor*);

 protected:
  // The Worklet inherits the url and userAgent from the frame->document().
  explicit Worklet(LocalFrame*);

  // Returns one of available global scopes.
  WorkletGlobalScopeProxy* FindAvailableGlobalScope() const;

  size_t GetNumberOfGlobalScopes() const { return proxies_.size(); }

 private:
  virtual void FetchAndInvokeScript(const KURL& module_url_record,
                                    const WorkletOptions&,
                                    ScriptPromiseResolver*);

  // Returns true if there are no global scopes or additional global scopes are
  // necessary. CreateGlobalScope() will be called in that case. Each worklet
  // can define how to pool global scopes here.
  virtual bool NeedsToCreateGlobalScope() = 0;
  virtual WorkletGlobalScopeProxy* CreateGlobalScope() = 0;

  // A worklet may or may not have more than one global scope. In the case where
  // there are multiple global scopes, this function MUST be overriden. The
  // default behavior is to return the global scope at index 0, which is for the
  // case where there is only one global scope.
  virtual size_t SelectGlobalScope() const;

  // "A Worklet has a list of the worklet's WorkletGlobalScopes. Initially this
  // list is empty; it is populated when the user agent chooses to create its
  // WorkletGlobalScope."
  // https://drafts.css-houdini.org/worklets/#worklet-section
  HeapVector<Member<WorkletGlobalScopeProxy>> proxies_;

  // "A Worklet has a module responses map. This is a ordered map of module URLs
  // to values that are a fetch responses. The map's entries are ordered based
  // on their insertion order. Access to this map should be thread-safe."
  // https://drafts.css-houdini.org/worklets/#module-responses-map
  Member<WorkletModuleResponsesMap> module_responses_map_;
};

}  // namespace blink

#endif  // Worklet_h
