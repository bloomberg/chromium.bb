/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef InProcessWorkerMessagingProxy_h
#define InProcessWorkerMessagingProxy_h

#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MessagePort.h"
#include "core/workers/ThreadedMessagingProxyBase.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/WeakPtr.h"
#include <memory>

namespace blink {

class ExecutionContext;
class InProcessWorkerBase;
class InProcessWorkerObjectProxy;
class SerializedScriptValue;
class WorkerClients;

class CORE_EXPORT InProcessWorkerMessagingProxy
    : public ThreadedMessagingProxyBase {
  WTF_MAKE_NONCOPYABLE(InProcessWorkerMessagingProxy);

 public:
  // These methods should only be used on the parent context thread.
  void startWorkerGlobalScope(const KURL& scriptURL,
                              const String& userAgent,
                              const String& sourceCode,
                              ContentSecurityPolicy*,
                              const String& referrerPolicy);
  void postMessageToWorkerGlobalScope(PassRefPtr<SerializedScriptValue>,
                                      MessagePortChannelArray);

  void workerThreadCreated() override;
  void parentObjectDestroyed() override;

  bool hasPendingActivity() const;

  // These methods come from worker context thread via
  // InProcessWorkerObjectProxy and are called on the parent context thread.
  void postMessageToWorkerObject(PassRefPtr<SerializedScriptValue>,
                                 MessagePortChannelArray);
  void dispatchErrorEvent(const String& errorMessage,
                          std::unique_ptr<SourceLocation>,
                          int exceptionId);

  // 'virtual' for testing.
  virtual void confirmMessageFromWorkerObject();

  // Called from InProcessWorkerObjectProxy when all pending activities on the
  // worker context are finished. See InProcessWorkerObjectProxy.h for details.
  virtual void pendingActivityFinished();

 protected:
  InProcessWorkerMessagingProxy(InProcessWorkerBase*, WorkerClients*);
  ~InProcessWorkerMessagingProxy() override;

  InProcessWorkerObjectProxy& workerObjectProxy() {
    return *m_workerObjectProxy.get();
  }

 private:
  friend class InProcessWorkerMessagingProxyForTest;
  InProcessWorkerMessagingProxy(ExecutionContext*,
                                InProcessWorkerBase*,
                                WorkerClients*);

  std::unique_ptr<InProcessWorkerObjectProxy> m_workerObjectProxy;
  WeakPersistent<InProcessWorkerBase> m_workerObject;
  Persistent<WorkerClients> m_workerClients;

  struct QueuedTask {
    RefPtr<SerializedScriptValue> message;
    MessagePortChannelArray channels;

    QueuedTask(RefPtr<SerializedScriptValue> message,
               MessagePortChannelArray channels);
    ~QueuedTask();
  };

  // Tasks are queued here until there's a thread object created.
  Vector<std::unique_ptr<QueuedTask>> m_queuedEarlyTasks;

  // Unconfirmed messages from the parent context thread to the worker thread.
  // When this is greater than 0, |m_workerGlobalScopeHasPendingActivity| should
  // be true.
  unsigned m_unconfirmedMessageCount = 0;

  // Indicates whether there are pending activities (e.g, MessageEvent,
  // setTimeout) on the worker context.
  bool m_workerGlobalScopeHasPendingActivity = false;

  WeakPtrFactory<InProcessWorkerMessagingProxy> m_weakPtrFactory;
};

}  // namespace blink

#endif  // InProcessWorkerMessagingProxy_h
