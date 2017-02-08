// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MojoWatcher_h
#define MojoWatcher_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/TraceWrapperMember.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "mojo/public/cpp/system/handle.h"

namespace blink {

class ExecutionContext;
class MojoHandleSignals;
class MojoWatchCallback;

class MojoWatcher final : public GarbageCollectedFinalized<MojoWatcher>,
                          public ActiveScriptWrappable<MojoWatcher>,
                          public ScriptWrappable,
                          public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MojoWatcher);

 public:
  static MojoWatcher* create(mojo::Handle,
                             const MojoHandleSignals&,
                             MojoWatchCallback*,
                             ExecutionContext*);
  ~MojoWatcher();

  MojoResult cancel();

  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

  // ActiveScriptWrappable
  bool hasPendingActivity() const final;

  // ContextLifecycleObserver
  void contextDestroyed(ExecutionContext*) final;

 private:
  friend class V8MojoWatcher;

  MojoWatcher(ExecutionContext*, MojoWatchCallback*);
  MojoResult watch(mojo::Handle, const MojoHandleSignals&);

  static void onHandleReady(uintptr_t context,
                            MojoResult,
                            MojoHandleSignalsState,
                            MojoWatchNotificationFlags);
  void runReadyCallback(MojoResult);

  RefPtr<WebTaskRunner> m_taskRunner;
  TraceWrapperMember<MojoWatchCallback> m_callback;
  mojo::Handle m_handle;
};

}  // namespace blink

#endif  // MojoWatcher_h
