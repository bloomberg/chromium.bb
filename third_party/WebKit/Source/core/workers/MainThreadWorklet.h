// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MainThreadWorklet_h
#define MainThreadWorklet_h

#include "core/workers/Worklet.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ScriptPromiseResolver;
class WorkletGlobalScopeProxy;

// A MainThreadWorklet is a worklet that runs only on the main thread.
// TODO(nhiroki): This is a temporary class to support module loading for main
// thread worklets. This and ThreadedWorklet will be merged into the base
// Worklet class once threaded worklets are ready to use module loading.
class CORE_EXPORT MainThreadWorklet : public Worklet {
  USING_GARBAGE_COLLECTED_MIXIN(MainThreadWorklet);
  WTF_MAKE_NONCOPYABLE(MainThreadWorklet);

 public:
  virtual ~MainThreadWorklet() = default;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) final;

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit MainThreadWorklet(LocalFrame*);

  // Returns one of available global scopes.
  WorkletGlobalScopeProxy* FindAvailableGlobalScope() const;

  size_t GetNumberOfGlobalScopes() const { return proxies_.size(); }

 private:
  // Worklet.
  void FetchAndInvokeScript(const KURL& module_url_record,
                            const WorkletOptions&,
                            ScriptPromiseResolver*) override;

  // Returns true if there are no global scopes or additional global scopes are
  // necessary. CreateGlobalScope() will be called in that case. Each worklet
  // can define how to pool global scopes here.
  virtual bool NeedsToCreateGlobalScope() = 0;
  virtual std::unique_ptr<WorkletGlobalScopeProxy> CreateGlobalScope() = 0;

  // "A Worklet has a list of the worklet's WorkletGlobalScopes. Initially this
  // list is empty; it is populated when the user agent chooses to create its
  // WorkletGlobalScope."
  // https://drafts.css-houdini.org/worklets/#worklet-section
  // TODO(nhiroki): Make (Paint)WorkletGlobalScopeProxy GC-managed object to
  // avoid that GC graphs are disjointed (https://crbug.com/719775).
  HashSet<std::unique_ptr<WorkletGlobalScopeProxy>> proxies_;
};

}  // namespace blink

#endif  // MainThreadWorklet_h
