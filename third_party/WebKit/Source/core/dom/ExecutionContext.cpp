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

#include "core/dom/ExecutionContext.h"

#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/SuspendableObject.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/ErrorEvent.h"
#include "core/events/EventTarget.h"
#include "core/frame/UseCounter.h"
#include "core/html/PublicURLManager.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

ExecutionContext::ExecutionContext()
    : m_circularSequentialID(0),
      m_inDispatchErrorEvent(false),
      m_isContextSuspended(false),
      m_isContextDestroyed(false),
      m_windowInteractionTokens(0),
      m_referrerPolicy(ReferrerPolicyDefault) {}

ExecutionContext::~ExecutionContext() {}

void ExecutionContext::suspendSuspendableObjects() {
  DCHECK(!m_isContextSuspended);
  notifySuspendingSuspendableObjects();
  m_isContextSuspended = true;
}

void ExecutionContext::resumeSuspendableObjects() {
  DCHECK(m_isContextSuspended);
  m_isContextSuspended = false;
  notifyResumingSuspendableObjects();
}

void ExecutionContext::notifyContextDestroyed() {
  m_isContextDestroyed = true;
  ContextLifecycleNotifier::notifyContextDestroyed();
}

void ExecutionContext::suspendScheduledTasks() {
  suspendSuspendableObjects();
  tasksWereSuspended();
}

void ExecutionContext::resumeScheduledTasks() {
  resumeSuspendableObjects();
  tasksWereResumed();
}

void ExecutionContext::suspendSuspendableObjectIfNeeded(
    SuspendableObject* object) {
#if DCHECK_IS_ON()
  DCHECK(contains(object));
#endif
  // Ensure all SuspendableObjects are suspended also newly created ones.
  if (m_isContextSuspended)
    object->suspend();
}

bool ExecutionContext::shouldSanitizeScriptError(
    const String& sourceURL,
    AccessControlStatus corsStatus) {
  if (corsStatus == OpaqueResource)
    return true;
  const KURL& url = completeURL(sourceURL);
  if (url.protocolIsData())
    return false;
  return !(getSecurityOrigin()->canRequestNoSuborigin(url) ||
           corsStatus == SharableCrossOrigin);
}

void ExecutionContext::dispatchErrorEvent(ErrorEvent* errorEvent,
                                          AccessControlStatus corsStatus) {
  if (m_inDispatchErrorEvent) {
    m_pendingExceptions.push_back(errorEvent);
    return;
  }

  // First report the original exception and only then all the nested ones.
  if (!dispatchErrorEventInternal(errorEvent, corsStatus))
    exceptionThrown(errorEvent);

  if (m_pendingExceptions.isEmpty())
    return;
  for (ErrorEvent* e : m_pendingExceptions)
    exceptionThrown(e);
  m_pendingExceptions.clear();
}

bool ExecutionContext::dispatchErrorEventInternal(
    ErrorEvent* errorEvent,
    AccessControlStatus corsStatus) {
  EventTarget* target = errorEventTarget();
  if (!target)
    return false;

  if (shouldSanitizeScriptError(errorEvent->filename(), corsStatus))
    errorEvent = ErrorEvent::createSanitizedError(errorEvent->world());

  DCHECK(!m_inDispatchErrorEvent);
  m_inDispatchErrorEvent = true;
  target->dispatchEvent(errorEvent);
  m_inDispatchErrorEvent = false;
  return errorEvent->defaultPrevented();
}

int ExecutionContext::circularSequentialID() {
  ++m_circularSequentialID;
  if (m_circularSequentialID > ((1U << 31) - 1U))
    m_circularSequentialID = 1;

  return m_circularSequentialID;
}

PublicURLManager& ExecutionContext::publicURLManager() {
  if (!m_publicURLManager)
    m_publicURLManager = PublicURLManager::create(this);
  return *m_publicURLManager;
}

SecurityOrigin* ExecutionContext::getSecurityOrigin() {
  return securityContext().getSecurityOrigin();
}

ContentSecurityPolicy* ExecutionContext::contentSecurityPolicy() {
  return securityContext().contentSecurityPolicy();
}

const KURL& ExecutionContext::url() const {
  return virtualURL();
}

KURL ExecutionContext::completeURL(const String& url) const {
  return virtualCompleteURL(url);
}

void ExecutionContext::allowWindowInteraction() {
  ++m_windowInteractionTokens;
}

void ExecutionContext::consumeWindowInteraction() {
  if (m_windowInteractionTokens == 0)
    return;
  --m_windowInteractionTokens;
}

bool ExecutionContext::isWindowInteractionAllowed() const {
  return m_windowInteractionTokens > 0;
}

bool ExecutionContext::isSecureContext(
    const SecureContextCheck privilegeContextCheck) const {
  String unusedErrorMessage;
  return isSecureContext(unusedErrorMessage, privilegeContextCheck);
}

String ExecutionContext::outgoingReferrer() const {
  return url().strippedForUseAsReferrer();
}

void ExecutionContext::parseAndSetReferrerPolicy(const String& policies,
                                                 bool supportLegacyKeywords) {
  ReferrerPolicy referrerPolicy;

  if (!SecurityPolicy::referrerPolicyFromHeaderValue(
          policies,
          supportLegacyKeywords ? SupportReferrerPolicyLegacyKeywords
                                : DoNotSupportReferrerPolicyLegacyKeywords,
          &referrerPolicy)) {
    addConsoleMessage(ConsoleMessage::create(
        RenderingMessageSource, ErrorMessageLevel,
        "Failed to set referrer policy: The value '" + policies +
            "' is not one of " +
            (supportLegacyKeywords
                 ? "'always', 'default', 'never', 'origin-when-crossorigin', "
                 : "") +
            "'no-referrer', 'no-referrer-when-downgrade', 'origin', "
            "'origin-when-cross-origin', or 'unsafe-url'. The referrer policy "
            "has been left unchanged."));
    return;
  }

  setReferrerPolicy(referrerPolicy);
}

void ExecutionContext::setReferrerPolicy(ReferrerPolicy referrerPolicy) {
  // When a referrer policy has already been set, the latest value takes
  // precedence.
  UseCounter::count(this, UseCounter::SetReferrerPolicy);
  if (m_referrerPolicy != ReferrerPolicyDefault)
    UseCounter::count(this, UseCounter::ResetReferrerPolicy);

  m_referrerPolicy = referrerPolicy;
}

void ExecutionContext::removeURLFromMemoryCache(const KURL& url) {
  memoryCache()->removeURLFromCache(url);
}

DEFINE_TRACE(ExecutionContext) {
  visitor->trace(m_publicURLManager);
  visitor->trace(m_pendingExceptions);
  ContextLifecycleNotifier::trace(visitor);
  Supplementable<ExecutionContext>::trace(visitor);
}

}  // namespace blink
