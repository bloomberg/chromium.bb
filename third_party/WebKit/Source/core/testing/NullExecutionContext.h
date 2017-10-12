// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NullExecutionContext_h
#define NullExecutionContext_h

#include <memory>
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/SecurityContext.h"
#include "core/dom/events/EventQueue.h"
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

  void DisableEval(const String&) override {}
  String UserAgent() const override { return String(); }

  EventTarget* ErrorEventTarget() override { return nullptr; }
  EventQueue* GetEventQueue() const override { return queue_.Get(); }

  bool TasksNeedSuspension() override { return tasks_need_suspension_; }
  void SetTasksNeedSuspension(bool flag) { tasks_need_suspension_ = flag; }

  void DidUpdateSecurityOrigin() override {}
  SecurityContext& GetSecurityContext() override { return *this; }
  DOMTimerCoordinator* Timers() override { return nullptr; }

  void AddConsoleMessage(ConsoleMessage*) override {}
  void ExceptionThrown(ErrorEvent*) override {}

  void SetIsSecureContext(bool);
  bool IsSecureContext(String& error_message) const override;

  void SetUpSecurityContext();

  ResourceFetcher* Fetcher() const override { return nullptr; }

  using SecurityContext::GetSecurityOrigin;
  using SecurityContext::GetContentSecurityPolicy;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(queue_);
    SecurityContext::Trace(visitor);
    ExecutionContext::Trace(visitor);
  }

 protected:
  const KURL& VirtualURL() const override { return url_; }
  KURL VirtualCompleteURL(const String&) const override { return url_; }

 private:
  bool tasks_need_suspension_;
  bool is_secure_context_;
  Member<EventQueue> queue_;

  KURL url_;
};

}  // namespace blink

#endif  // NullExecutionContext_h
