// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/app_banner/BeforeInstallPromptEvent.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/UseCounter.h"
#include "modules/app_banner/BeforeInstallPromptEventInit.h"

namespace blink {

BeforeInstallPromptEvent::BeforeInstallPromptEvent(
    const AtomicString& name,
    LocalFrame& frame,
    mojom::blink::AppBannerServicePtr service_ptr,
    mojom::blink::AppBannerEventRequest event_request,
    const Vector<String>& platforms)
    : Event(name, false, true),
      banner_service_(std::move(service_ptr)),
      binding_(this, std::move(event_request)),
      platforms_(platforms),
      user_choice_(new UserChoiceProperty(frame.GetDocument(),
                                          this,
                                          UserChoiceProperty::kUserChoice)),
      prompt_called_(false) {
  DCHECK(banner_service_);
  DCHECK(binding_.is_bound());
  UseCounter::Count(&frame, UseCounter::kBeforeInstallPromptEvent);
}

BeforeInstallPromptEvent::BeforeInstallPromptEvent(
    const AtomicString& name,
    const BeforeInstallPromptEventInit& init)
    : Event(name, init), binding_(this), prompt_called_(false) {
  if (init.hasPlatforms())
    platforms_ = init.platforms();
}

BeforeInstallPromptEvent::~BeforeInstallPromptEvent() {}

void BeforeInstallPromptEvent::Dispose() {
  banner_service_.reset();
  binding_.Close();
}

Vector<String> BeforeInstallPromptEvent::platforms() const {
  return platforms_;
}

ScriptPromise BeforeInstallPromptEvent::userChoice(ScriptState* script_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    UseCounter::kBeforeInstallPromptEventUserChoice);
  // |m_binding| must be bound to allow the AppBannerService to resolve the
  // userChoice promise.
  if (user_choice_ && binding_.is_bound())
    return user_choice_->Promise(script_state->World());
  return ScriptPromise::RejectWithDOMException(
      script_state,
      DOMException::Create(kInvalidStateError,
                           "userChoice cannot be accessed on this event."));
}

ScriptPromise BeforeInstallPromptEvent::prompt(ScriptState* script_state) {
  // |m_bannerService| must be bound to allow us to inform the AppBannerService
  // to display the banner now.
  if (!defaultPrevented() || prompt_called_ || !banner_service_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError,
                             "The prompt() method may only be called once, "
                             "following preventDefault()."));
  }

  UseCounter::Count(ExecutionContext::From(script_state),
                    UseCounter::kBeforeInstallPromptEventPrompt);

  prompt_called_ = true;
  banner_service_->DisplayAppBanner();
  return ScriptPromise::CastUndefined(script_state);
}

const AtomicString& BeforeInstallPromptEvent::InterfaceName() const {
  return EventNames::BeforeInstallPromptEvent;
}

void BeforeInstallPromptEvent::preventDefault() {
  Event::preventDefault();
  if (target()) {
    UseCounter::Count(target()->GetExecutionContext(),
                      UseCounter::kBeforeInstallPromptEventPreventDefault);
  }
}

void BeforeInstallPromptEvent::BannerAccepted(const String& platform) {
  user_choice_->Resolve(AppBannerPromptResult::Create(
      platform, AppBannerPromptResult::Outcome::kAccepted));
}

void BeforeInstallPromptEvent::BannerDismissed() {
  user_choice_->Resolve(AppBannerPromptResult::Create(
      g_empty_atom, AppBannerPromptResult::Outcome::kDismissed));
}

DEFINE_TRACE(BeforeInstallPromptEvent) {
  visitor->Trace(user_choice_);
  Event::Trace(visitor);
}

}  // namespace blink
