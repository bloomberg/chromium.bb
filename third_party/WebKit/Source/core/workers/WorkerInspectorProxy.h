// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerInspectorProxy_h
#define WorkerInspectorProxy_h

#include "core/CoreExport.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ThreadDebugger.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class ExecutionContext;
class KURL;
class WorkerThread;

// A proxy for talking to the worker inspector on the worker thread.
// All of these methods should be called on the main thread.
class CORE_EXPORT WorkerInspectorProxy final
    : public GarbageCollectedFinalized<WorkerInspectorProxy> {
 public:
  enum class PauseOnWorkerStart { kPause, kDontPause };

  static WorkerInspectorProxy* Create();

  ~WorkerInspectorProxy();
  void Trace(blink::Visitor*);

  class CORE_EXPORT PageInspector {
   public:
    virtual ~PageInspector() {}
    virtual void DispatchMessageFromWorker(WorkerInspectorProxy*,
                                           int session_id,
                                           const String& message) = 0;
  };

  // Returns whether WorkerThread should pause to run debugger tasks on its
  // startup.
  PauseOnWorkerStart ShouldPauseOnWorkerStart(ExecutionContext*);

  void WorkerThreadCreated(ExecutionContext*, WorkerThread*, const KURL&);
  void WorkerThreadTerminated();
  void DispatchMessageFromWorker(int session_id, const String&);
  void AddConsoleMessageFromWorker(MessageLevel,
                                   const String& message,
                                   std::unique_ptr<SourceLocation>);

  void ConnectToInspector(int session_id,
                          const String& parent_instrumentation_token,
                          PageInspector*);
  void DisconnectFromInspector(int session_id, PageInspector*);
  void SendMessageToInspector(int session_id, const String& message);
  void WriteTimelineStartedEvent(const String& tracing_session_id);

  const String& Url() { return url_; }
  ExecutionContext* GetExecutionContext() { return execution_context_; }
  const String& InspectorId();

  using WorkerInspectorProxySet =
      PersistentHeapHashSet<WeakMember<WorkerInspectorProxy>>;
  static const WorkerInspectorProxySet& AllProxies();

 private:
  WorkerInspectorProxy();

  WorkerThread* worker_thread_;
  Member<ExecutionContext> execution_context_;
  HashMap<int, PageInspector*> page_inspectors_;
  String url_;
  String inspector_id_;
};

struct CORE_EXPORT GlobalScopeInspectorCreationParams final {
  WTF_MAKE_NONCOPYABLE(GlobalScopeInspectorCreationParams);
  USING_FAST_MALLOC(GlobalScopeInspectorCreationParams);

 public:
  explicit GlobalScopeInspectorCreationParams(
      WorkerInspectorProxy::PauseOnWorkerStart pause_on_start);
  GlobalScopeInspectorCreationParams(
      WorkerInspectorProxy::PauseOnWorkerStart pause_on_start,
      const v8_inspector::V8StackTraceId&);
  ~GlobalScopeInspectorCreationParams() = default;

  WorkerInspectorProxy::PauseOnWorkerStart pause_on_start;
  v8_inspector::V8StackTraceId stack_id;
};

}  // namespace blink

#endif  // WorkerInspectorProxy_h
