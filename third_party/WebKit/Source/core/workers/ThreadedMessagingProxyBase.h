// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedMessagingProxyBase_h
#define ThreadedMessagingProxyBase_h

#include "core/CoreExport.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleTypes.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "wtf/Forward.h"

namespace blink {

class ExecutionContext;
class SourceLocation;
class WorkerInspectorProxy;
class WorkerLoaderProxy;
class WorkerThread;
class WorkerThreadStartupData;

class CORE_EXPORT ThreadedMessagingProxyBase
    : private WorkerLoaderProxyProvider {
 public:
  void terminateGlobalScope();

  virtual void workerThreadCreated();

  // This method should be called in the destructor of the object which
  // initially created it. This object could either be a Worker or a Worklet.
  virtual void parentObjectDestroyed();

  void countFeature(UseCounter::Feature);
  void countDeprecation(UseCounter::Feature);

  void reportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            std::unique_ptr<SourceLocation>);
  void postMessageToPageInspector(const String&);

  // 'virtual' for testing.
  virtual void workerThreadTerminated();

  // Accessed only from the parent thread.
  ExecutionContext* getExecutionContext() const {
    return m_executionContext.get();
  }

  // Accessed from both the parent thread and the worker.
  ParentFrameTaskRunners* getParentFrameTaskRunners() {
    return m_parentFrameTaskRunners.get();
  }

  // Number of live messaging proxies, used by leak detection.
  static int proxyCount();

 protected:
  ThreadedMessagingProxyBase(ExecutionContext*);
  ~ThreadedMessagingProxyBase() override;

  void initializeWorkerThread(std::unique_ptr<WorkerThreadStartupData>);
  virtual std::unique_ptr<WorkerThread> createWorkerThread(
      double originTime) = 0;

  WorkerThread* workerThread() const { return m_workerThread.get(); }

  bool askedToTerminate() const { return m_askedToTerminate; }

  PassRefPtr<WorkerLoaderProxy> loaderProxy() { return m_loaderProxy; }
  WorkerInspectorProxy* workerInspectorProxy() const {
    return m_workerInspectorProxy.get();
  }

  // Returns true if this is called on the parent context thread.
  bool isParentContextThread() const;

  // WorkerLoaderProxyProvider
  // These methods are called on different threads to schedule loading
  // requests and to send callbacks back to WorkerGlobalScope.
  void postTaskToLoader(const WebTraceLocation&,
                        std::unique_ptr<WTF::CrossThreadClosure>) override;
  void postTaskToWorkerGlobalScope(
      const WebTraceLocation&,
      std::unique_ptr<WTF::CrossThreadClosure>) override;
  ThreadableLoadingContext* getThreadableLoadingContext() override;

 private:
  friend class InProcessWorkerMessagingProxyForTest;
  friend class ThreadedWorkletMessagingProxyForTest;

  void parentObjectDestroyedInternal();

  Persistent<ExecutionContext> m_executionContext;
  Persistent<ThreadableLoadingContext> m_loadingContext;
  Persistent<WorkerInspectorProxy> m_workerInspectorProxy;
  // Accessed cross-thread when worker thread posts tasks to the parent.
  CrossThreadPersistent<ParentFrameTaskRunners> m_parentFrameTaskRunners;

  std::unique_ptr<WorkerThread> m_workerThread;

  RefPtr<WorkerLoaderProxy> m_loaderProxy;

  bool m_mayBeDestroyed;
  bool m_askedToTerminate;
};

}  // namespace blink

#endif  // ThreadedMessagingProxyBase_h
