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
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/SuspendableObject.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/events/EventTarget.h"
#include "core/events/ErrorEvent.h"
#include "core/frame/UseCounter.h"
#include "core/html/PublicURLManager.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

ExecutionContext::ExecutionContext()
    : circular_sequential_id_(0),
      in_dispatch_error_event_(false),
      is_context_suspended_(false),
      is_context_destroyed_(false),
      window_interaction_tokens_(0),
      referrer_policy_(kReferrerPolicyDefault) {}

ExecutionContext::~ExecutionContext() {}

// static
ExecutionContext* ExecutionContext::From(const ScriptState* script_state) {
  v8::HandleScope scope(script_state->GetIsolate());
  return ToExecutionContext(script_state->GetContext());
}

// static
ExecutionContext* ExecutionContext::ForCurrentRealm(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return ToExecutionContext(info.GetIsolate()->GetCurrentContext());
}

// static
ExecutionContext* ExecutionContext::ForRelevantRealm(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return ToExecutionContext(info.Holder()->CreationContext());
}

void ExecutionContext::SuspendSuspendableObjects() {
  DCHECK(!is_context_suspended_);
  NotifySuspendingSuspendableObjects();
  is_context_suspended_ = true;
}

void ExecutionContext::ResumeSuspendableObjects() {
  DCHECK(is_context_suspended_);
  is_context_suspended_ = false;
  NotifyResumingSuspendableObjects();
}

void ExecutionContext::NotifyContextDestroyed() {
  is_context_destroyed_ = true;
  ContextLifecycleNotifier::NotifyContextDestroyed();
}

void ExecutionContext::SuspendScheduledTasks() {
  SuspendSuspendableObjects();
  TasksWereSuspended();
}

void ExecutionContext::ResumeScheduledTasks() {
  ResumeSuspendableObjects();
  TasksWereResumed();
}

void ExecutionContext::SuspendSuspendableObjectIfNeeded(
    SuspendableObject* object) {
#if DCHECK_IS_ON()
  DCHECK(Contains(object));
#endif
  // Ensure all SuspendableObjects are suspended also newly created ones.
  if (is_context_suspended_)
    object->Suspend();
}

bool ExecutionContext::ShouldSanitizeScriptError(
    const String& source_url,
    AccessControlStatus cors_status) {
  if (cors_status == kOpaqueResource)
    return true;
  const KURL& url = CompleteURL(source_url);
  if (url.ProtocolIsData())
    return false;
  return !(GetSecurityOrigin()->CanRequestNoSuborigin(url) ||
           cors_status == kSharableCrossOrigin);
}

void ExecutionContext::DispatchErrorEvent(ErrorEvent* error_event,
                                          AccessControlStatus cors_status) {
  if (in_dispatch_error_event_) {
    pending_exceptions_.push_back(error_event);
    return;
  }

  // First report the original exception and only then all the nested ones.
  if (!DispatchErrorEventInternal(error_event, cors_status))
    ExceptionThrown(error_event);

  if (pending_exceptions_.IsEmpty())
    return;
  for (ErrorEvent* e : pending_exceptions_)
    ExceptionThrown(e);
  pending_exceptions_.clear();
}

bool ExecutionContext::DispatchErrorEventInternal(
    ErrorEvent* error_event,
    AccessControlStatus cors_status) {
  EventTarget* target = ErrorEventTarget();
  if (!target)
    return false;

  if (ShouldSanitizeScriptError(error_event->filename(), cors_status))
    error_event = ErrorEvent::CreateSanitizedError(error_event->World());

  DCHECK(!in_dispatch_error_event_);
  in_dispatch_error_event_ = true;
  target->DispatchEvent(error_event);
  in_dispatch_error_event_ = false;
  return error_event->defaultPrevented();
}

int ExecutionContext::CircularSequentialID() {
  ++circular_sequential_id_;
  if (circular_sequential_id_ > ((1U << 31) - 1U))
    circular_sequential_id_ = 1;

  return circular_sequential_id_;
}

PublicURLManager& ExecutionContext::GetPublicURLManager() {
  if (!public_url_manager_)
    public_url_manager_ = PublicURLManager::Create(this);
  return *public_url_manager_;
}

SecurityOrigin* ExecutionContext::GetSecurityOrigin() {
  return GetSecurityContext().GetSecurityOrigin();
}

ContentSecurityPolicy* ExecutionContext::GetContentSecurityPolicy() {
  return GetSecurityContext().GetContentSecurityPolicy();
}

void ExecutionContext::AllowWindowInteraction() {
  ++window_interaction_tokens_;
}

void ExecutionContext::ConsumeWindowInteraction() {
  if (window_interaction_tokens_ == 0)
    return;
  --window_interaction_tokens_;
}

bool ExecutionContext::IsWindowInteractionAllowed() const {
  return window_interaction_tokens_ > 0;
}

bool ExecutionContext::IsSecureContext() const {
  String unused_error_message;
  return IsSecureContext(unused_error_message);
}

String ExecutionContext::OutgoingReferrer() const {
  return Url().StrippedForUseAsReferrer();
}

void ExecutionContext::ParseAndSetReferrerPolicy(const String& policies,
                                                 bool support_legacy_keywords) {
  ReferrerPolicy referrer_policy;

  if (!SecurityPolicy::ReferrerPolicyFromHeaderValue(
          policies,
          support_legacy_keywords ? kSupportReferrerPolicyLegacyKeywords
                                  : kDoNotSupportReferrerPolicyLegacyKeywords,
          &referrer_policy)) {
    AddConsoleMessage(ConsoleMessage::Create(
        kRenderingMessageSource, kErrorMessageLevel,
        "Failed to set referrer policy: The value '" + policies +
            "' is not one of " +
            (support_legacy_keywords
                 ? "'always', 'default', 'never', 'origin-when-crossorigin', "
                 : "") +
            "'no-referrer', 'no-referrer-when-downgrade', 'origin', "
            "'origin-when-cross-origin', 'same-origin', 'strict-origin', "
            "'strict-origin-when-cross-origin', or 'unsafe-url'. The referrer "
            "policy "
            "has been left unchanged."));
    return;
  }

  SetReferrerPolicy(referrer_policy);
}

void ExecutionContext::SetReferrerPolicy(ReferrerPolicy referrer_policy) {
  // When a referrer policy has already been set, the latest value takes
  // precedence.
  UseCounter::Count(this, WebFeature::kSetReferrerPolicy);
  if (referrer_policy_ != kReferrerPolicyDefault)
    UseCounter::Count(this, WebFeature::kResetReferrerPolicy);

  referrer_policy_ = referrer_policy;
}

void ExecutionContext::RemoveURLFromMemoryCache(const KURL& url) {
  GetMemoryCache()->RemoveURLFromCache(url);
}

void ExecutionContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(public_url_manager_);
  visitor->Trace(pending_exceptions_);
  ContextLifecycleNotifier::Trace(visitor);
  Supplementable<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
