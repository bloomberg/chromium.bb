/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
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

#ifndef ExecutionContext_h
#define ExecutionContext_h

#include <memory>

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleNotifier.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/SecurityContext.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebTraceLocation.h"
#include "v8/include/v8.h"

namespace service_manager {
class InterfaceProvider;
}

namespace blink {

class ConsoleMessage;
class CoreProbeSink;
class DOMTimerCoordinator;
class ErrorEvent;
class EventQueue;
class EventTarget;
class LocalDOMWindow;
class SuspendableObject;
class PublicURLManager;
class ResourceFetcher;
class SecurityOrigin;
class ScriptState;
enum class TaskType : unsigned;

enum ReasonForCallingCanExecuteScripts {
  kAboutToExecuteScript,
  kNotAboutToExecuteScript
};

class CORE_EXPORT ExecutionContext : public ContextLifecycleNotifier,
                                     public Supplementable<ExecutionContext> {
  WTF_MAKE_NONCOPYABLE(ExecutionContext);
  MERGE_GARBAGE_COLLECTED_MIXINS();

 public:
  DECLARE_VIRTUAL_TRACE();

  static ExecutionContext* From(const ScriptState*);

  // Returns the ExecutionContext of the current realm.
  static ExecutionContext* ForCurrentRealm(
      const v8::FunctionCallbackInfo<v8::Value>&);
  // Returns the ExecutionContext of the relevant realm for the receiver object.
  static ExecutionContext* ForRelevantRealm(
      const v8::FunctionCallbackInfo<v8::Value>&);

  virtual bool IsDocument() const { return false; }
  virtual bool IsWorkerOrWorkletGlobalScope() const { return false; }
  virtual bool IsWorkerGlobalScope() const { return false; }
  virtual bool IsWorkletGlobalScope() const { return false; }
  virtual bool IsMainThreadWorkletGlobalScope() const { return false; }
  virtual bool IsDedicatedWorkerGlobalScope() const { return false; }
  virtual bool IsSharedWorkerGlobalScope() const { return false; }
  virtual bool IsServiceWorkerGlobalScope() const { return false; }
  virtual bool IsAnimationWorkletGlobalScope() const { return false; }
  virtual bool IsAudioWorkletGlobalScope() const { return false; }
  virtual bool IsPaintWorkletGlobalScope() const { return false; }
  virtual bool IsThreadedWorkletGlobalScope() const { return false; }
  virtual bool IsJSExecutionForbidden() const { return false; }

  virtual bool IsContextThread() const { return true; }

  SecurityOrigin* GetSecurityOrigin();
  ContentSecurityPolicy* GetContentSecurityPolicy();
  const KURL& Url() const;
  KURL CompleteURL(const String& url) const;
  virtual const KURL& BaseURL() const = 0;
  virtual void DisableEval(const String& error_message) = 0;
  virtual LocalDOMWindow* ExecutingWindow() const { return nullptr; }
  virtual String UserAgent() const = 0;

  // Gets the DOMTimerCoordinator which maintains the "active timer
  // list" of tasks created by setTimeout and setInterval. The
  // DOMTimerCoordinator is owned by the ExecutionContext and should
  // not be used after the ExecutionContext is destroyed.
  virtual DOMTimerCoordinator* Timers() = 0;

  virtual ResourceFetcher* Fetcher() const = 0;

  virtual SecurityContext& GetSecurityContext() = 0;

  virtual bool CanExecuteScripts(ReasonForCallingCanExecuteScripts) {
    return false;
  }

  bool ShouldSanitizeScriptError(const String& source_url, AccessControlStatus);
  void DispatchErrorEvent(ErrorEvent*, AccessControlStatus);

  virtual void AddConsoleMessage(ConsoleMessage*) = 0;
  virtual void ExceptionThrown(ErrorEvent*) = 0;

  PublicURLManager& GetPublicURLManager();

  virtual void RemoveURLFromMemoryCache(const KURL&);

  void SuspendSuspendableObjects();
  void ResumeSuspendableObjects();
  void StopSuspendableObjects();
  void NotifyContextDestroyed() override;

  void SuspendScheduledTasks();
  void ResumeScheduledTasks();

  // TODO(haraken): Remove these methods by making the customers inherit from
  // SuspendableObject. SuspendableObject is a standard way to observe context
  // suspension/resumption.
  virtual bool TasksNeedSuspension() { return false; }
  virtual void TasksWereSuspended() {}
  virtual void TasksWereResumed() {}

  bool IsContextSuspended() const { return is_context_suspended_; }
  bool IsContextDestroyed() const { return is_context_destroyed_; }

  // Called after the construction of an SuspendableObject to synchronize
  // suspend
  // state.
  void SuspendSuspendableObjectIfNeeded(SuspendableObject*);

  // Gets the next id in a circular sequence from 1 to 2^31-1.
  int CircularSequentialID();

  virtual EventTarget* ErrorEventTarget() = 0;
  virtual EventQueue* GetEventQueue() const = 0;

  // Methods related to window interaction. It should be used to manage window
  // focusing and window creation permission for an ExecutionContext.
  void AllowWindowInteraction();
  void ConsumeWindowInteraction();
  bool IsWindowInteractionAllowed() const;

  // Decides whether this context is privileged, as described in
  // https://w3c.github.io/webappsec/specs/powerfulfeatures/#settings-privileged.
  virtual bool IsSecureContext(String& error_message) const = 0;
  virtual bool IsSecureContext() const;

  virtual String OutgoingReferrer() const;
  // Parses a comma-separated list of referrer policy tokens, and sets
  // the context's referrer policy to the last one that is a valid
  // policy. Logs a message to the console if none of the policy
  // tokens are valid policies.
  //
  // If |supportLegacyKeywords| is true, then the legacy keywords
  // "never", "default", "always", and "origin-when-crossorigin" are
  // parsed as valid policies.
  void ParseAndSetReferrerPolicy(const String& policies,
                                 bool support_legacy_keywords = false);
  void SetReferrerPolicy(ReferrerPolicy);
  virtual ReferrerPolicy GetReferrerPolicy() const { return referrer_policy_; }

  virtual CoreProbeSink* GetProbeSink() { return nullptr; }

  virtual service_manager::InterfaceProvider* GetInterfaceProvider() {
    return nullptr;
  }

 protected:
  ExecutionContext();
  virtual ~ExecutionContext();

  virtual const KURL& VirtualURL() const = 0;
  virtual KURL VirtualCompleteURL(const String&) const = 0;

 private:
  bool DispatchErrorEventInternal(ErrorEvent*, AccessControlStatus);

  unsigned circular_sequential_id_;

  bool in_dispatch_error_event_;
  HeapVector<Member<ErrorEvent>> pending_exceptions_;

  bool is_context_suspended_;
  bool is_context_destroyed_;

  Member<PublicURLManager> public_url_manager_;

  // Counter that keeps track of how many window interaction calls are allowed
  // for this ExecutionContext. Callers are expected to call
  // |allowWindowInteraction()| and |consumeWindowInteraction()| in order to
  // increment and decrement the counter.
  int window_interaction_tokens_;

  ReferrerPolicy referrer_policy_;
};

}  // namespace blink

#endif  // ExecutionContext_h
