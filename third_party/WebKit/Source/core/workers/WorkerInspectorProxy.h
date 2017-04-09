// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerInspectorProxy_h
#define WorkerInspectorProxy_h

#include "core/CoreExport.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerThread.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class Document;
class KURL;

// A proxy for talking to the worker inspector on the worker thread.
// All of these methods should be called on the main thread.
class CORE_EXPORT WorkerInspectorProxy final
    : public GarbageCollectedFinalized<WorkerInspectorProxy> {
 public:
  static WorkerInspectorProxy* Create();

  ~WorkerInspectorProxy();
  DECLARE_TRACE();

  class CORE_EXPORT PageInspector {
   public:
    virtual ~PageInspector() {}
    virtual void DispatchMessageFromWorker(WorkerInspectorProxy*,
                                           const String&) = 0;
  };

  WorkerThreadStartMode WorkerStartMode(Document*);
  void WorkerThreadCreated(Document*, WorkerThread*, const KURL&);
  void WorkerThreadTerminated();
  void DispatchMessageFromWorker(const String&);
  void AddConsoleMessageFromWorker(MessageLevel,
                                   const String& message,
                                   std::unique_ptr<SourceLocation>);

  void ConnectToInspector(PageInspector*);
  void DisconnectFromInspector(PageInspector*);
  void SendMessageToInspector(const String&);
  void WriteTimelineStartedEvent(const String& session_id);

  const String& Url() { return url_; }
  Document* GetDocument() { return document_; }
  const String& InspectorId();

  using WorkerInspectorProxySet =
      PersistentHeapHashSet<WeakMember<WorkerInspectorProxy>>;
  static const WorkerInspectorProxySet& AllProxies();

 private:
  WorkerInspectorProxy();

  WorkerThread* worker_thread_;
  Member<Document> document_;
  PageInspector* page_inspector_;
  String url_;
  String inspector_id_;
};

}  // namespace blink

#endif  // WorkerInspectorProxy_h
