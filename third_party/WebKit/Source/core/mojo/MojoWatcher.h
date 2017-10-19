// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MojoWatcher_h
#define MojoWatcher_h

#include "core/dom/ContextLifecycleObserver.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/watcher.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"

namespace blink {

class ExecutionContext;
class MojoHandleSignals;
class V8MojoWatchCallback;

class MojoWatcher final : public GarbageCollectedFinalized<MojoWatcher>,
                          public ActiveScriptWrappable<MojoWatcher>,
                          public ScriptWrappable,
                          public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MojoWatcher);

 public:
  static MojoWatcher* Create(mojo::Handle,
                             const MojoHandleSignals&,
                             V8MojoWatchCallback*,
                             ExecutionContext*);
  ~MojoWatcher();

  MojoResult cancel();

  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

  // ActiveScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) final;

 private:
  friend class V8MojoWatcher;

  MojoWatcher(ExecutionContext*, V8MojoWatchCallback*);
  MojoResult Watch(mojo::Handle, const MojoHandleSignals&);
  MojoResult Arm(MojoResult* ready_result);

  static void OnHandleReady(uintptr_t context,
                            MojoResult,
                            MojoHandleSignalsState,
                            MojoWatcherNotificationFlags);
  void RunReadyCallback(MojoResult);

  RefPtr<WebTaskRunner> task_runner_;
  TraceWrapperMember<V8MojoWatchCallback> callback_;
  mojo::ScopedWatcherHandle watcher_handle_;
  mojo::Handle handle_;
};

}  // namespace blink

#endif  // MojoWatcher_h
