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

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleNotifier.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/SecurityContext.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class SuspendableObject;
class ConsoleMessage;
class DOMTimerCoordinator;
class ErrorEvent;
class EventQueue;
class EventTarget;
class ExecutionContextTask;
class LocalDOMWindow;
class PublicURLManager;
class SecurityOrigin;
enum class TaskType : unsigned;

enum ReasonForCallingCanExecuteScripts {
  AboutToExecuteScript,
  NotAboutToExecuteScript
};

class CORE_EXPORT ExecutionContext : public ContextLifecycleNotifier,
                                     public Supplementable<ExecutionContext> {
  WTF_MAKE_NONCOPYABLE(ExecutionContext);

 public:
  DECLARE_VIRTUAL_TRACE();

  // Used to specify whether |isSecureContext| should walk the
  // ancestor tree to decide whether to restrict usage of a powerful
  // feature.
  enum SecureContextCheck {
    StandardSecureContextCheck,
    WebCryptoSecureContextCheck
  };

  virtual bool isDocument() const { return false; }
  virtual bool isWorkerOrWorkletGlobalScope() const { return false; }
  virtual bool isWorkerGlobalScope() const { return false; }
  virtual bool isWorkletGlobalScope() const { return false; }
  virtual bool isMainThreadWorkletGlobalScope() const { return false; }
  virtual bool isDedicatedWorkerGlobalScope() const { return false; }
  virtual bool isSharedWorkerGlobalScope() const { return false; }
  virtual bool isServiceWorkerGlobalScope() const { return false; }
  virtual bool isCompositorWorkerGlobalScope() const { return false; }
  virtual bool isAnimationWorkletGlobalScope() const { return false; }
  virtual bool isAudioWorkletGlobalScope() const { return false; }
  virtual bool isPaintWorkletGlobalScope() const { return false; }
  virtual bool isThreadedWorkletGlobalScope() const { return false; }
  virtual bool isJSExecutionForbidden() const { return false; }

  virtual bool isContextThread() const { return true; }

  SecurityOrigin* getSecurityOrigin();
  ContentSecurityPolicy* contentSecurityPolicy();
  const KURL& url() const;
  KURL completeURL(const String& url) const;
  virtual void disableEval(const String& errorMessage) = 0;
  virtual LocalDOMWindow* executingWindow() const { return 0; }
  virtual String userAgent() const = 0;
  // Executes the task on context's thread asynchronously.
  virtual void postTask(
      TaskType,
      const WebTraceLocation&,
      std::unique_ptr<ExecutionContextTask>,
      const String& taskNameForInstrumentation = emptyString) = 0;

  // Gets the DOMTimerCoordinator which maintains the "active timer
  // list" of tasks created by setTimeout and setInterval. The
  // DOMTimerCoordinator is owned by the ExecutionContext and should
  // not be used after the ExecutionContext is destroyed.
  virtual DOMTimerCoordinator* timers() = 0;

  virtual SecurityContext& securityContext() = 0;
  KURL contextURL() const { return virtualURL(); }
  KURL contextCompleteURL(const String& url) const {
    return virtualCompleteURL(url);
  }

  virtual bool canExecuteScripts(ReasonForCallingCanExecuteScripts) {
    return false;
  }

  bool shouldSanitizeScriptError(const String& sourceURL, AccessControlStatus);
  void dispatchErrorEvent(ErrorEvent*, AccessControlStatus);

  virtual void addConsoleMessage(ConsoleMessage*) = 0;
  virtual void exceptionThrown(ErrorEvent*) = 0;

  PublicURLManager& publicURLManager();

  virtual void removeURLFromMemoryCache(const KURL&);

  void suspendSuspendableObjects();
  void resumeSuspendableObjects();
  void stopSuspendableObjects();
  void notifyContextDestroyed() override;

  void suspendScheduledTasks();
  void resumeScheduledTasks();

  // TODO(haraken): Remove these methods by making the customers inherit from
  // SuspendableObject. SuspendableObject is a standard way to observe context
  // suspension/resumption.
  virtual bool tasksNeedSuspension() { return false; }
  virtual void tasksWereSuspended() {}
  virtual void tasksWereResumed() {}

  bool isContextSuspended() const { return m_isContextSuspended; }
  bool isContextDestroyed() const { return m_isContextDestroyed; }

  // Called after the construction of an SuspendableObject to synchronize
  // suspend
  // state.
  void suspendSuspendableObjectIfNeeded(SuspendableObject*);

  // Gets the next id in a circular sequence from 1 to 2^31-1.
  int circularSequentialID();

  virtual EventTarget* errorEventTarget() = 0;
  virtual EventQueue* getEventQueue() const = 0;

  // Methods related to window interaction. It should be used to manage window
  // focusing and window creation permission for an ExecutionContext.
  void allowWindowInteraction();
  void consumeWindowInteraction();
  bool isWindowInteractionAllowed() const;

  // Decides whether this context is privileged, as described in
  // https://w3c.github.io/webappsec/specs/powerfulfeatures/#settings-privileged.
  virtual bool isSecureContext(
      String& errorMessage,
      const SecureContextCheck = StandardSecureContextCheck) const = 0;
  virtual bool isSecureContext(
      const SecureContextCheck = StandardSecureContextCheck) const;

  virtual String outgoingReferrer() const;
  // Parses a comma-separated list of referrer policy tokens, and sets
  // the context's referrer policy to the last one that is a valid
  // policy. Logs a message to the console if none of the policy
  // tokens are valid policies.
  //
  // If |supportLegacyKeywords| is true, then the legacy keywords
  // "never", "default", "always", and "origin-when-crossorigin" are
  // parsed as valid policies.
  void parseAndSetReferrerPolicy(const String& policies,
                                 bool supportLegacyKeywords = false);
  void setReferrerPolicy(ReferrerPolicy);
  virtual ReferrerPolicy getReferrerPolicy() const { return m_referrerPolicy; }

 protected:
  ExecutionContext();
  virtual ~ExecutionContext();

  virtual const KURL& virtualURL() const = 0;
  virtual KURL virtualCompleteURL(const String&) const = 0;

 private:
  bool dispatchErrorEventInternal(ErrorEvent*, AccessControlStatus);

  unsigned m_circularSequentialID;

  bool m_inDispatchErrorEvent;
  HeapVector<Member<ErrorEvent>> m_pendingExceptions;

  bool m_isContextSuspended;
  bool m_isContextDestroyed;

  Member<PublicURLManager> m_publicURLManager;

  // Counter that keeps track of how many window interaction calls are allowed
  // for this ExecutionContext. Callers are expected to call
  // |allowWindowInteraction()| and |consumeWindowInteraction()| in order to
  // increment and decrement the counter.
  int m_windowInteractionTokens;

  ReferrerPolicy m_referrerPolicy;
};

}  // namespace blink

#endif  // ExecutionContext_h
