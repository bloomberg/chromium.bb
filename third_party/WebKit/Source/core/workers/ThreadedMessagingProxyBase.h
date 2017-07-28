// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedMessagingProxyBase_h
#define ThreadedMessagingProxyBase_h

#include "core/CoreExport.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleTypes.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerBackingThreadStartupData.h"
#include "core/workers/WorkerClients.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Optional.h"

namespace blink {

class ExecutionContext;
class SourceLocation;
class ThreadableLoadingContext;
class WorkerInspectorProxy;
class WorkerThread;
struct GlobalScopeCreationParams;

// The base proxy class to talk to Worker/WorkletGlobalScope on a worker thread
// from the parent context thread (Note that this is always the main thread for
// now because nested worker is not supported yet). This must be created,
// accessed and destroyed on the parent context thread.
//
// This has a unique lifetime: this is co-owned by the parent object (e.g.,
// InProcessWorkerBase, AnimationWorklet) and by itself via SelfKeepAlive. The
// parent object releases the reference on its destructor and SelfKeepAlive is
// cleared when the worker thread is terminated.
//
// This co-ownership is necessary because the proxy needs to outlive components
// living on the worker thread (e.g., WorkerGlobalScope) but the parent object
// can be destroyed before the completion of worker thread termination.
class CORE_EXPORT ThreadedMessagingProxyBase
    : public GarbageCollectedFinalized<ThreadedMessagingProxyBase> {
 public:
  virtual ~ThreadedMessagingProxyBase();

  void TerminateGlobalScope();

  // This method should be called in the destructor of the object which
  // initially created it. This object could either be a Worker or a Worklet.
  // This may cause deletion of this via |keep_alive_|.
  void ParentObjectDestroyed();

  void CountFeature(WebFeature);
  void CountDeprecation(WebFeature);

  void ReportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            std::unique_ptr<SourceLocation>);
  void PostMessageToPageInspector(int session_id, const String&);

  void WorkerThreadTerminated();

  // Number of live messaging proxies, used by leak detection.
  static int ProxyCount();

  DECLARE_VIRTUAL_TRACE();

 protected:
  ThreadedMessagingProxyBase(ExecutionContext*, WorkerClients*);

  void InitializeWorkerThread(
      std::unique_ptr<GlobalScopeCreationParams>,
      const WTF::Optional<WorkerBackingThreadStartupData>&,
      const KURL& script_url);
  virtual void WorkerThreadCreated();

  ThreadableLoadingContext* CreateThreadableLoadingContext() const;

  ExecutionContext* GetExecutionContext() const;
  ParentFrameTaskRunners* GetParentFrameTaskRunners() const;
  WorkerInspectorProxy* GetWorkerInspectorProxy() const;
  WorkerThread* GetWorkerThread() const;

  bool AskedToTerminate() const { return asked_to_terminate_; }

  // Transfers ownership of the clients to the caller.
  WorkerClients* ReleaseWorkerClients();

  // Returns true if this is called on the parent context thread.
  bool IsParentContextThread() const;

 private:
  virtual std::unique_ptr<WorkerThread> CreateWorkerThread() = 0;

  Member<ExecutionContext> execution_context_;
  Member<WorkerClients> worker_clients_;
  Member<WorkerInspectorProxy> worker_inspector_proxy_;

  // Accessed cross-thread when worker thread posts tasks to the parent.
  CrossThreadPersistent<ParentFrameTaskRunners> parent_frame_task_runners_;

  std::unique_ptr<WorkerThread> worker_thread_;

  bool asked_to_terminate_;

  // Used to keep this alive until the worker thread gets terminated. This is
  // necessary because the co-owner (i.e., Worker or Worklet object) can be
  // destroyed before thread termination.
  SelfKeepAlive<ThreadedMessagingProxyBase> keep_alive_;
};

}  // namespace blink

#endif  // ThreadedMessagingProxyBase_h
