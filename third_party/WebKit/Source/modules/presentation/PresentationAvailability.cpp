// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationAvailability.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/dom/events/Event.h"
#include "core/frame/UseCounter.h"
#include "modules/event_target_modules_names.h"
#include "modules/presentation/PresentationAvailabilityState.h"
#include "modules/presentation/PresentationController.h"
#include "platform/wtf/Vector.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"
#include "third_party/WebKit/common/page/page_visibility_state.mojom-blink.h"

namespace blink {

// static
PresentationAvailability* PresentationAvailability::Take(
    PresentationAvailabilityProperty* resolver,
    const WTF::Vector<KURL>& urls,
    bool value) {
  PresentationAvailability* presentation_availability =
      new PresentationAvailability(resolver->GetExecutionContext(), urls,
                                   value);
  presentation_availability->PauseIfNeeded();
  presentation_availability->UpdateListening();
  return presentation_availability;
}

PresentationAvailability::PresentationAvailability(
    ExecutionContext* execution_context,
    const WTF::Vector<KURL>& urls,
    bool value)
    : PausableObject(execution_context),
      PageVisibilityObserver(ToDocument(execution_context)->GetPage()),
      urls_(urls),
      value_(value),
      state_(State::kActive) {
  DCHECK(execution_context->IsDocument());
}

PresentationAvailability::~PresentationAvailability() = default;

const AtomicString& PresentationAvailability::InterfaceName() const {
  return EventTargetNames::PresentationAvailability;
}

ExecutionContext* PresentationAvailability::GetExecutionContext() const {
  return PausableObject::GetExecutionContext();
}

void PresentationAvailability::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  if (event_type == EventTypeNames::change) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPresentationAvailabilityChangeEventListener);
  }
}

void PresentationAvailability::AvailabilityChanged(
    blink::mojom::ScreenAvailability availability) {
  bool value = availability == blink::mojom::ScreenAvailability::AVAILABLE;
  if (value_ == value)
    return;

  value_ = value;
  DispatchEvent(Event::Create(EventTypeNames::change));
}

bool PresentationAvailability::HasPendingActivity() const {
  return state_ != State::kInactive;
}

void PresentationAvailability::Unpause() {
  SetState(State::kActive);
}

void PresentationAvailability::Pause() {
  SetState(State::kSuspended);
}

void PresentationAvailability::ContextDestroyed(ExecutionContext*) {
  SetState(State::kInactive);
}

void PresentationAvailability::PageVisibilityChanged() {
  if (state_ == State::kInactive)
    return;
  UpdateListening();
}

void PresentationAvailability::SetState(State state) {
  state_ = state;
  UpdateListening();
}

void PresentationAvailability::UpdateListening() {
  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!controller)
    return;

  if (state_ == State::kActive &&
      (ToDocument(GetExecutionContext())->GetPageVisibilityState() ==
       mojom::PageVisibilityState::kVisible))
    controller->GetAvailabilityState()->AddObserver(this);
  else
    controller->GetAvailabilityState()->RemoveObserver(this);
}

const Vector<KURL>& PresentationAvailability::Urls() const {
  return urls_;
}

bool PresentationAvailability::value() const {
  return value_;
}

void PresentationAvailability::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  PausableObject::Trace(visitor);
}

}  // namespace blink
