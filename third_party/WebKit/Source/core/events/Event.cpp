/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/events/Event.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/StaticNodeList.h"
#include "core/events/EventDispatchMediator.h"
#include "core/events/EventTarget.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/UseCounter.h"
#include "core/svg/SVGElement.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

static bool IsEventTypeScopedInV0(const AtomicString& event_type) {
  // WebKit never allowed selectstart event to cross the the shadow DOM
  // boundary.  Changing this breaks existing sites.
  // See https://bugs.webkit.org/show_bug.cgi?id=52195 for details.
  return event_type == EventTypeNames::abort ||
         event_type == EventTypeNames::change ||
         event_type == EventTypeNames::error ||
         event_type == EventTypeNames::load ||
         event_type == EventTypeNames::reset ||
         event_type == EventTypeNames::resize ||
         event_type == EventTypeNames::scroll ||
         event_type == EventTypeNames::select ||
         event_type == EventTypeNames::selectstart ||
         event_type == EventTypeNames::slotchange;
}

Event::Event() : Event("", false, false) {
  was_initialized_ = false;
}

Event::Event(const AtomicString& event_type,
             bool can_bubble_arg,
             bool cancelable_arg,
             TimeTicks platform_time_stamp)
    : Event(event_type,
            can_bubble_arg,
            cancelable_arg,
            ComposedMode::kScoped,
            platform_time_stamp) {}

Event::Event(const AtomicString& event_type,
             bool can_bubble_arg,
             bool cancelable_arg,
             ComposedMode composed_mode)
    : Event(event_type,
            can_bubble_arg,
            cancelable_arg,
            composed_mode,
            TimeTicks::Now()) {}

Event::Event(const AtomicString& event_type,
             bool can_bubble_arg,
             bool cancelable_arg,
             ComposedMode composed_mode,
             TimeTicks platform_time_stamp)
    : type_(event_type),
      can_bubble_(can_bubble_arg),
      cancelable_(cancelable_arg),
      composed_(composed_mode == ComposedMode::kComposed),
      is_event_type_scoped_in_v0_(IsEventTypeScopedInV0(event_type)),
      propagation_stopped_(false),
      immediate_propagation_stopped_(false),
      default_prevented_(false),
      default_handled_(false),
      was_initialized_(true),
      is_trusted_(false),
      prevent_default_called_on_uncancelable_event_(false),
      handling_passive_(PassiveMode::kNotPassiveDefault),
      event_phase_(0),
      current_target_(nullptr),
      platform_time_stamp_(platform_time_stamp) {}

Event::Event(const AtomicString& event_type,
             const EventInit& initializer,
             TimeTicks platform_time_stamp)
    : Event(event_type,
            initializer.bubbles(),
            initializer.cancelable(),
            initializer.composed() ? ComposedMode::kComposed
                                   : ComposedMode::kScoped,
            platform_time_stamp) {}

Event::~Event() {}

bool Event::IsScopedInV0() const {
  return isTrusted() && is_event_type_scoped_in_v0_;
}

void Event::initEvent(const AtomicString& event_type_arg,
                      bool can_bubble_arg,
                      bool cancelable_arg) {
  initEvent(event_type_arg, can_bubble_arg, cancelable_arg, nullptr);
}

void Event::initEvent(const AtomicString& event_type_arg,
                      bool can_bubble_arg,
                      bool cancelable_arg,
                      EventTarget* related_target) {
  if (IsBeingDispatched())
    return;

  was_initialized_ = true;
  propagation_stopped_ = false;
  immediate_propagation_stopped_ = false;
  default_prevented_ = false;
  is_trusted_ = false;
  prevent_default_called_on_uncancelable_event_ = false;

  type_ = event_type_arg;
  can_bubble_ = can_bubble_arg;
  cancelable_ = cancelable_arg;
}

bool Event::legacyReturnValue(ScriptState* script_state) const {
  bool return_value = !defaultPrevented();
  if (return_value) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      UseCounter::kEventGetReturnValueTrue);
  } else {
    UseCounter::Count(ExecutionContext::From(script_state),
                      UseCounter::kEventGetReturnValueFalse);
  }
  return return_value;
}

void Event::setLegacyReturnValue(ScriptState* script_state, bool return_value) {
  if (return_value) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      UseCounter::kEventSetReturnValueTrue);
  } else {
    UseCounter::Count(ExecutionContext::From(script_state),
                      UseCounter::kEventSetReturnValueFalse);
  }
  SetDefaultPrevented(!return_value);
}

const AtomicString& Event::InterfaceName() const {
  return EventNames::Event;
}

bool Event::HasInterface(const AtomicString& name) const {
  return InterfaceName() == name;
}

bool Event::IsUIEvent() const {
  return false;
}

bool Event::IsMouseEvent() const {
  return false;
}

bool Event::IsFocusEvent() const {
  return false;
}

bool Event::IsKeyboardEvent() const {
  return false;
}

bool Event::IsTouchEvent() const {
  return false;
}

bool Event::IsGestureEvent() const {
  return false;
}

bool Event::IsWheelEvent() const {
  return false;
}

bool Event::IsRelatedEvent() const {
  return false;
}

bool Event::IsPointerEvent() const {
  return false;
}

bool Event::IsInputEvent() const {
  return false;
}

bool Event::IsDragEvent() const {
  return false;
}

bool Event::IsClipboardEvent() const {
  return false;
}

bool Event::IsBeforeTextInsertedEvent() const {
  return false;
}

bool Event::IsBeforeUnloadEvent() const {
  return false;
}

void Event::preventDefault() {
  if (handling_passive_ != PassiveMode::kNotPassive &&
      handling_passive_ != PassiveMode::kNotPassiveDefault) {
    prevent_default_called_during_passive_ = true;

    const LocalDOMWindow* window =
        event_path_ ? event_path_->GetWindowEventContext().Window() : 0;
    if (window && handling_passive_ == PassiveMode::kPassive) {
      window->PrintErrorMessage(
          "Unable to preventDefault inside passive event listener invocation.");
    }
    return;
  }

  if (cancelable_)
    default_prevented_ = true;
  else
    prevent_default_called_on_uncancelable_event_ = true;
}

void Event::SetTarget(EventTarget* target) {
  if (target_ == target)
    return;

  target_ = target;
  if (target_)
    ReceivedTarget();
}

void Event::ReceivedTarget() {}

void Event::SetUnderlyingEvent(Event* ue) {
  // Prohibit creation of a cycle -- just do nothing in that case.
  for (Event* e = ue; e; e = e->UnderlyingEvent())
    if (e == this)
      return;
  underlying_event_ = ue;
}

void Event::InitEventPath(Node& node) {
  if (!event_path_) {
    event_path_ = new EventPath(node, this);
  } else {
    event_path_->InitializeWith(node, this);
  }
}

HeapVector<Member<EventTarget>> Event::path(ScriptState* script_state) const {
  return PathInternal(script_state, kNonEmptyAfterDispatch);
}

HeapVector<Member<EventTarget>> Event::composedPath(
    ScriptState* script_state) const {
  return PathInternal(script_state, kEmptyAfterDispatch);
}

void Event::SetHandlingPassive(PassiveMode mode) {
  handling_passive_ = mode;
  prevent_default_called_during_passive_ = false;
}

HeapVector<Member<EventTarget>> Event::PathInternal(ScriptState* script_state,
                                                    EventPathMode mode) const {
  if (target_)
    HostsUsingFeatures::CountHostOrIsolatedWorldHumanReadableName(
        script_state, *target_, HostsUsingFeatures::Feature::kEventPath);

  if (!current_target_) {
    DCHECK_EQ(Event::kNone, event_phase_);
    if (!event_path_) {
      // Before dispatching the event
      return HeapVector<Member<EventTarget>>();
    }
    DCHECK(!event_path_->IsEmpty());
    // After dispatching the event
    if (mode == kEmptyAfterDispatch)
      return HeapVector<Member<EventTarget>>();
    return event_path_->Last().GetTreeScopeEventContext().EnsureEventPath(
        *event_path_);
  }

  if (Node* node = current_target_->ToNode()) {
    DCHECK(event_path_);
    for (auto& context : event_path_->NodeEventContexts()) {
      if (node == context.GetNode())
        return context.GetTreeScopeEventContext().EnsureEventPath(*event_path_);
    }
    NOTREACHED();
  }

  if (LocalDOMWindow* window = current_target_->ToLocalDOMWindow()) {
    if (event_path_ && !event_path_->IsEmpty()) {
      return event_path_->TopNodeEventContext()
          .GetTreeScopeEventContext()
          .EnsureEventPath(*event_path_);
    }
    return HeapVector<Member<EventTarget>>(1, window);
  }

  return HeapVector<Member<EventTarget>>();
}

EventDispatchMediator* Event::CreateMediator() {
  return EventDispatchMediator::Create(this);
}

EventTarget* Event::currentTarget() const {
  if (!current_target_)
    return nullptr;
  Node* node = current_target_->ToNode();
  if (node && node->IsSVGElement()) {
    if (SVGElement* svg_element = ToSVGElement(node)->CorrespondingElement())
      return svg_element;
  }
  return current_target_.Get();
}

double Event::timeStamp(ScriptState* script_state) const {
  double time_stamp = 0;
  if (script_state && LocalDOMWindow::From(script_state)) {
    Performance* performance =
        DOMWindowPerformance::performance(*LocalDOMWindow::From(script_state));
    double timestamp_seconds =
        (platform_time_stamp_ - TimeTicks()).InSecondsF();
    time_stamp =
        performance->MonotonicTimeToDOMHighResTimeStamp(timestamp_seconds);
  }

  return time_stamp;
}

void Event::setCancelBubble(ScriptState* script_state, bool cancel) {
  if (cancel)
    propagation_stopped_ = true;
}

DEFINE_TRACE(Event) {
  visitor->Trace(current_target_);
  visitor->Trace(target_);
  visitor->Trace(underlying_event_);
  visitor->Trace(event_path_);
}

}  // namespace blink
