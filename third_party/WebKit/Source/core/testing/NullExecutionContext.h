// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NullExecutionContext_h
#define NullExecutionContext_h

#include <memory>
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/events/EventQueue.h"
#include "core/execution_context/ExecutionContext.h"
#include "core/execution_context/SecurityContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class NullExecutionContext
    : public GarbageCollectedFinalized<NullExecutionContext>,
      public SecurityContext,
      public ExecutionContext {
  USING_GARBAGE_COLLECTED_MIXIN(NullExecutionContext);

 public:
  NullExecutionContext();

  void SetURL(const KURL& url) { url_ = url; }

  const KURL& Url() const override { return url_; }
  const KURL& BaseURL() const override { return url_; }
  KURL CompleteURL(const String&) const override { return url_; }

  void DisableEval(const String&) override {}
  String UserAgent() const override { return String(); }

  EventTarget* ErrorEventTarget() override { return nullptr; }
  EventQueue* GetEventQueue() const override { return queue_.Get(); }

  bool TasksNeedPause() override { return tasks_need_pause_; }
  void SetTasksNeedPause(bool flag) { tasks_need_pause_ = flag; }

  void DidUpdateSecurityOrigin() override {}
  SecurityContext& GetSecurityContext() override { return *this; }
  DOMTimerCoordinator* Timers() override { return nullptr; }

  void AddConsoleMessage(ConsoleMessage*) override {}
  void ExceptionThrown(ErrorEvent*) override {}

  void SetIsSecureContext(bool);
  bool IsSecureContext(String& error_message) const override;

  void SetUpSecurityContext();

  ResourceFetcher* Fetcher() const override { return nullptr; }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override;

  using SecurityContext::GetSecurityOrigin;
  using SecurityContext::GetContentSecurityPolicy;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(queue_);
    SecurityContext::Trace(visitor);
    ExecutionContext::Trace(visitor);
  }

 private:
  bool tasks_need_pause_;
  bool is_secure_context_;
  Member<EventQueue> queue_;

  KURL url_;
};

}  // namespace blink

#endif  // NullExecutionContext_h
