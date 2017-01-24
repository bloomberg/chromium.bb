// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedObjectProxyBase_h
#define ThreadedObjectProxyBase_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/CoreExport.h"
#include "core/dom/MessagePort.h"
#include "core/workers/WorkerReportingProxy.h"

namespace blink {

class ParentFrameTaskRunners;
class ThreadedMessagingProxyBase;

// A proxy to talk to the parent object. This object is created and destroyed on
// the main thread, and used on the worker thread for proxying messages to the
// ThreadedMessagingProxyBase on the main thread. ThreadedMessagingProxyBase
// always outlives this proxy.
class CORE_EXPORT ThreadedObjectProxyBase : public WorkerReportingProxy {
  USING_FAST_MALLOC(ThreadedObjectProxyBase);
  WTF_MAKE_NONCOPYABLE(ThreadedObjectProxyBase);

 public:
  ~ThreadedObjectProxyBase() override = default;

  void reportPendingActivity(bool hasPendingActivity);

  // WorkerReportingProxy overrides.
  void countFeature(UseCounter::Feature) override;
  void countDeprecation(UseCounter::Feature) override;
  void reportConsoleMessage(MessageSource,
                            MessageLevel,
                            const String& message,
                            SourceLocation*) override;
  void postMessageToPageInspector(const String&) override;
  void didCloseWorkerGlobalScope() override;
  void didTerminateWorkerThread() override;

 protected:
  explicit ThreadedObjectProxyBase(ParentFrameTaskRunners*);
  virtual WeakPtr<ThreadedMessagingProxyBase> messagingProxyWeakPtr() = 0;
  ParentFrameTaskRunners* getParentFrameTaskRunners();

 private:
  // Used to post a task to ThreadedMessagingProxyBase on the parent context
  // thread.
  CrossThreadPersistent<ParentFrameTaskRunners> m_parentFrameTaskRunners;
};

}  // namespace blink

#endif  // ThreadedObjectProxyBase_h
