/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/BindingSecurity.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Location.h"
#include "bindings/core/v8/WrapperCreationSecurityCheck.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

namespace {

bool CanAccessFrameInternal(const LocalDOMWindow* accessing_window,
                            const SecurityOrigin* target_frame_origin,
                            const DOMWindow* target_window) {
  SECURITY_CHECK(!(target_window && target_window->GetFrame()) ||
                 target_window == target_window->GetFrame()->DomWindow());

  // It's important to check that target_window is a LocalDOMWindow: it's
  // possible for a remote frame and local frame to have the same security
  // origin, depending on the model being used to allocate Frames between
  // processes. See https://crbug.com/601629.
  if (!(accessing_window && target_window && target_window->IsLocalDOMWindow()))
    return false;

  const SecurityOrigin* accessing_origin =
      accessing_window->document()->GetSecurityOrigin();
  if (!accessing_origin->CanAccess(target_frame_origin))
    return false;

  // Notify the loader's client if the initial document has been accessed.
  LocalFrame* target_frame = ToLocalDOMWindow(target_window)->GetFrame();
  if (target_frame &&
      target_frame->Loader().StateMachine()->IsDisplayingInitialEmptyDocument())
    target_frame->Loader().DidAccessInitialDocument();

  return true;
}

bool CanAccessFrame(const LocalDOMWindow* accessing_window,
                    const SecurityOrigin* target_frame_origin,
                    const DOMWindow* target_window,
                    ExceptionState& exception_state) {
  if (CanAccessFrameInternal(accessing_window, target_frame_origin,
                             target_window))
    return true;

  if (target_window)
    exception_state.ThrowSecurityError(
        target_window->SanitizedCrossDomainAccessErrorMessage(accessing_window),
        target_window->CrossDomainAccessErrorMessage(accessing_window));
  return false;
}

bool CanAccessFrame(const LocalDOMWindow* accessing_window,
                    SecurityOrigin* target_frame_origin,
                    const DOMWindow* target_window,
                    BindingSecurity::ErrorReportOption reporting_option) {
  if (CanAccessFrameInternal(accessing_window, target_frame_origin,
                             target_window))
    return true;

  if (accessing_window && target_window &&
      reporting_option == BindingSecurity::ErrorReportOption::kReport)
    accessing_window->PrintErrorMessage(
        target_window->CrossDomainAccessErrorMessage(accessing_window));
  return false;
}

}  // namespace

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const DOMWindow* target,
    ExceptionState& exception_state) {
  DCHECK(target);
  const Frame* frame = target->GetFrame();
  if (!frame || !frame->GetSecurityContext())
    return false;
  return CanAccessFrame(accessing_window,
                        frame->GetSecurityContext()->GetSecurityOrigin(),
                        target, exception_state);
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const DOMWindow* target,
    ErrorReportOption reporting_option) {
  DCHECK(target);
  const Frame* frame = target->GetFrame();
  if (!frame || !frame->GetSecurityContext())
    return false;
  return CanAccessFrame(accessing_window,
                        frame->GetSecurityContext()->GetSecurityOrigin(),
                        target, reporting_option);
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const EventTarget* target,
    ExceptionState& exception_state) {
  DCHECK(target);
  const DOMWindow* window = target->ToDOMWindow();
  if (!window) {
    // We only need to check the access to Window objects which are
    // cross-origin accessible.  If it's not a Window, the object's
    // origin must always be the same origin (or it already leaked).
    return true;
  }
  const Frame* frame = window->GetFrame();
  if (!frame || !frame->GetSecurityContext())
    return false;
  return CanAccessFrame(accessing_window,
                        frame->GetSecurityContext()->GetSecurityOrigin(),
                        window, exception_state);
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const Location* target,
    ExceptionState& exception_state) {
  DCHECK(target);
  const Frame* frame = target->GetFrame();
  if (!frame || !frame->GetSecurityContext())
    return false;
  return CanAccessFrame(accessing_window,
                        frame->GetSecurityContext()->GetSecurityOrigin(),
                        frame->DomWindow(), exception_state);
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const Location* target,
    ErrorReportOption reporting_option) {
  DCHECK(target);
  const Frame* frame = target->GetFrame();
  if (!frame || !frame->GetSecurityContext())
    return false;
  return CanAccessFrame(accessing_window,
                        frame->GetSecurityContext()->GetSecurityOrigin(),
                        frame->DomWindow(), reporting_option);
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const Node* target,
    ExceptionState& exception_state) {
  if (!target)
    return false;
  return CanAccessFrame(accessing_window,
                        target->GetDocument().GetSecurityOrigin(),
                        target->GetDocument().domWindow(), exception_state);
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const Node* target,
    ErrorReportOption reporting_option) {
  if (!target)
    return false;
  return CanAccessFrame(accessing_window,
                        target->GetDocument().GetSecurityOrigin(),
                        target->GetDocument().domWindow(), reporting_option);
}

bool BindingSecurity::ShouldAllowAccessToFrame(
    const LocalDOMWindow* accessing_window,
    const Frame* target,
    ExceptionState& exception_state) {
  if (!target || !target->GetSecurityContext())
    return false;
  return CanAccessFrame(accessing_window,
                        target->GetSecurityContext()->GetSecurityOrigin(),
                        target->DomWindow(), exception_state);
}

bool BindingSecurity::ShouldAllowAccessToFrame(
    const LocalDOMWindow* accessing_window,
    const Frame* target,
    ErrorReportOption reporting_option) {
  if (!target || !target->GetSecurityContext())
    return false;
  return CanAccessFrame(accessing_window,
                        target->GetSecurityContext()->GetSecurityOrigin(),
                        target->DomWindow(), reporting_option);
}

bool BindingSecurity::ShouldAllowAccessToDetachedWindow(
    const LocalDOMWindow* accessing_window,
    const DOMWindow* target,
    ExceptionState& exception_state) {
  CHECK(target && !target->GetFrame())
      << "This version of shouldAllowAccessToFrame() must be used only for "
      << "detached windows.";
  if (!target->IsLocalDOMWindow())
    return false;
  Document* document = ToLocalDOMWindow(target)->document();
  if (!document)
    return false;
  return CanAccessFrame(accessing_window, document->GetSecurityOrigin(), target,
                        exception_state);
}

bool BindingSecurity::ShouldAllowNamedAccessTo(
    const DOMWindow* accessing_window,
    const DOMWindow* target_window) {
  const Frame* accessing_frame = accessing_window->GetFrame();
  DCHECK(accessing_frame);
  DCHECK(accessing_frame->GetSecurityContext());
  const SecurityOrigin* accessing_origin =
      accessing_frame->GetSecurityContext()->GetSecurityOrigin();

  const Frame* target_frame = target_window->GetFrame();
  DCHECK(target_frame);
  DCHECK(target_frame->GetSecurityContext());
  const SecurityOrigin* target_origin =
      target_frame->GetSecurityContext()->GetSecurityOrigin();
  SECURITY_CHECK(!(target_window && target_window->GetFrame()) ||
                 target_window == target_window->GetFrame()->DomWindow());

  if (!accessing_origin->CanAccess(target_origin))
    return false;

  // Note that there is no need to call back
  // FrameLoader::didAccessInitialDocument() because |targetWindow| must be
  // a child window inside iframe or frame and it doesn't have a URL bar,
  // so there is no need to worry about URL spoofing.

  return true;
}

bool BindingSecurity::ShouldAllowAccessToCreationContext(
    v8::Local<v8::Context> creation_context,
    const WrapperTypeInfo* type) {
  // According to
  // https://html.spec.whatwg.org/multipage/browsers.html#security-location,
  // cross-origin script access to a few properties of Location is allowed.
  // Location already implements the necessary security checks.
  if (type->Equals(&V8Location::wrapperTypeInfo))
    return true;

  v8::Isolate* isolate = creation_context->GetIsolate();
  LocalFrame* frame = ToLocalFrameIfNotDetached(creation_context);
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 type->interface_name);
  if (!frame) {
    // Sandbox detached frames - they can't create cross origin objects.
    LocalDOMWindow* calling_window = CurrentDOMWindow(isolate);
    LocalDOMWindow* target_window = ToLocalDOMWindow(creation_context);

    return ShouldAllowAccessToDetachedWindow(calling_window, target_window,
                                             exception_state);
  }
  const DOMWrapperWorld& current_world =
      DOMWrapperWorld::World(isolate->GetCurrentContext());
  CHECK_EQ(current_world.GetWorldId(),
           DOMWrapperWorld::World(creation_context).GetWorldId());

  return !current_world.IsMainWorld() ||
         ShouldAllowAccessToFrame(CurrentDOMWindow(isolate), frame,
                                  exception_state);
}

void BindingSecurity::RethrowCrossContextException(
    v8::Local<v8::Context> creation_context,
    const WrapperTypeInfo* type,
    v8::Local<v8::Value> cross_context_exception) {
  DCHECK(!cross_context_exception.IsEmpty());
  v8::Isolate* isolate = creation_context->GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 type->interface_name);
  if (type->Equals(&V8Location::wrapperTypeInfo)) {
    // Convert cross-context exception to security error
    LocalDOMWindow* calling_window = CurrentDOMWindow(isolate);
    LocalDOMWindow* target_window = ToLocalDOMWindow(creation_context);
    exception_state.ThrowSecurityError(
        target_window->SanitizedCrossDomainAccessErrorMessage(calling_window),
        target_window->CrossDomainAccessErrorMessage(calling_window));
    return;
  }
  exception_state.RethrowV8Exception(cross_context_exception);
}

void BindingSecurity::InitWrapperCreationSecurityCheck() {
  WrapperCreationSecurityCheck::SetSecurityCheckFunction(
      &ShouldAllowAccessToCreationContext);
  WrapperCreationSecurityCheck::SetRethrowExceptionFunction(
      &RethrowCrossContextException);
}

void BindingSecurity::FailedAccessCheckFor(v8::Isolate* isolate,
                                           const Frame* target) {
  // TODO(dcheng): See if this null check can be removed or hoisted to a
  // different location.
  if (!target)
    return;

  DOMWindow* target_window = target->DomWindow();

  // TODO(dcheng): Add ContextType, interface name, and property name as
  // arguments, so the generated exception can be more descriptive.
  ExceptionState exception_state(isolate, ExceptionState::kUnknownContext,
                                 nullptr, nullptr);
  exception_state.ThrowSecurityError(
      target_window->SanitizedCrossDomainAccessErrorMessage(
          CurrentDOMWindow(isolate)),
      target_window->CrossDomainAccessErrorMessage(CurrentDOMWindow(isolate)));
}

}  // namespace blink
