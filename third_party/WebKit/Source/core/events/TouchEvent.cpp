/*
 * Copyright 2008, The Android Open Source Project
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
 */

#include "core/events/TouchEvent.h"

#include "core/dom/events/EventDispatcher.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLElement.h"
#include "core/input/InputDeviceCapabilities.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/Histogram.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/ScriptState.h"
#include "public/platform/WebCoalescedInputEvent.h"

namespace blink {

namespace {

// These offsets change indicies into the ListenerHistogram
// enumeration. The addition of a series of offsets then
// produces the resulting ListenerHistogram value.
// TODO(dtapuska): Remove all of these histogram counts once
// https://crbug.com/599609 is fixed.
const size_t kTouchTargetHistogramRootScrollerOffset = 6;
const size_t kTouchTargetHistogramScrollableDocumentOffset = 3;
const size_t kTouchTargetHistogramAlreadyHandledOffset = 0;
const size_t kTouchTargetHistogramNotHandledOffset = 1;
const size_t kTouchTargetHistogramHandledOffset = 2;
const size_t kCapturingOffset = 0;
const size_t kAtTargetOffset = 12;
const size_t kBubblingOffset = 24;

enum TouchTargetAndDispatchResultType {
  // The following enums represent state captured during the CAPTURING_PHASE.

  // Non-root-scroller, non-scrollable document, already handled.
  kCapturingNonRootScrollerNonScrollableAlreadyHandled,
  // Non-root-scroller, non-scrollable document, not handled.
  kCapturingNonRootScrollerNonScrollableNotHandled,
  // Non-root-scroller, non-scrollable document, handled application.
  kCapturingNonRootScrollerNonScrollableHandled,
  // Non-root-scroller, scrollable document, already handled.
  kCapturingNonRootScrollerScrollableDocumentAlreadyHandled,
  // Non-root-scroller, scrollable document, not handled.
  kCapturingNonRootScrollerScrollableDocumentNotHandled,
  // Non-root-scroller, scrollable document, handled application.
  kCapturingNonRootScrollerScrollableDocumentHandled,
  // Root-scroller, non-scrollable document, already handled.
  kCapturingRootScrollerNonScrollableAlreadyHandled,
  // Root-scroller, non-scrollable document, not handled.
  kCapturingRootScrollerNonScrollableNotHandled,
  // Root-scroller, non-scrollable document, handled.
  kCapturingRootScrollerNonScrollableHandled,
  // Root-scroller, scrollable document, already handled.
  kCapturingRootScrollerScrollableDocumentAlreadyHandled,
  // Root-scroller, scrollable document, not handled.
  kCapturingRootScrollerScrollableDocumentNotHandled,
  // Root-scroller, scrollable document, handled.
  kCapturingRootScrollerScrollableDocumentHandled,

  // The following enums represent state captured during the AT_TARGET phase.

  // Non-root-scroller, non-scrollable document, already handled.
  kNonRootScrollerNonScrollableAlreadyHandled,
  // Non-root-scroller, non-scrollable document, not handled.
  kNonRootScrollerNonScrollableNotHandled,
  // Non-root-scroller, non-scrollable document, handled application.
  kNonRootScrollerNonScrollableHandled,
  // Non-root-scroller, scrollable document, already handled.
  kNonRootScrollerScrollableDocumentAlreadyHandled,
  // Non-root-scroller, scrollable document, not handled.
  kNonRootScrollerScrollableDocumentNotHandled,
  // Non-root-scroller, scrollable document, handled application.
  kNonRootScrollerScrollableDocumentHandled,
  // Root-scroller, non-scrollable document, already handled.
  kRootScrollerNonScrollableAlreadyHandled,
  // Root-scroller, non-scrollable document, not handled.
  kRootScrollerNonScrollableNotHandled,
  // Root-scroller, non-scrollable document, handled.
  kRootScrollerNonScrollableHandled,
  // Root-scroller, scrollable document, already handled.
  kRootScrollerScrollableDocumentAlreadyHandled,
  // Root-scroller, scrollable document, not handled.
  kRootScrollerScrollableDocumentNotHandled,
  // Root-scroller, scrollable document, handled.
  kRootScrollerScrollableDocumentHandled,

  // The following enums represent state captured during the BUBBLING_PHASE.

  // Non-root-scroller, non-scrollable document, already handled.
  kBubblingNonRootScrollerNonScrollableAlreadyHandled,
  // Non-root-scroller, non-scrollable document, not handled.
  kBubblingNonRootScrollerNonScrollableNotHandled,
  // Non-root-scroller, non-scrollable document, handled application.
  kBubblingNonRootScrollerNonScrollableHandled,
  // Non-root-scroller, scrollable document, already handled.
  kBubblingNonRootScrollerScrollableDocumentAlreadyHandled,
  // Non-root-scroller, scrollable document, not handled.
  kBubblingNonRootScrollerScrollableDocumentNotHandled,
  // Non-root-scroller, scrollable document, handled application.
  kBubblingNonRootScrollerScrollableDocumentHandled,
  // Root-scroller, non-scrollable document, already handled.
  kBubblingRootScrollerNonScrollableAlreadyHandled,
  // Root-scroller, non-scrollable document, not handled.
  kBubblingRootScrollerNonScrollableNotHandled,
  // Root-scroller, non-scrollable document, handled.
  kBubblingRootScrollerNonScrollableHandled,
  // Root-scroller, scrollable document, already handled.
  kBubblingRootScrollerScrollableDocumentAlreadyHandled,
  // Root-scroller, scrollable document, not handled.
  kBubblingRootScrollerScrollableDocumentNotHandled,
  // Root-scroller, scrollable document, handled.
  kBubblingRootScrollerScrollableDocumentHandled,

  kTouchTargetAndDispatchResultTypeMax,
};

void LogTouchTargetHistogram(EventTarget* event_target,
                             unsigned short phase,
                             bool default_prevented_before_current_target,
                             bool default_prevented) {
  int result = 0;
  Document* document = nullptr;

  switch (phase) {
    default:
    case Event::kNone:
      return;
    case Event::kCapturingPhase:
      result += kCapturingOffset;
      break;
    case Event::kAtTarget:
      result += kAtTargetOffset;
      break;
    case Event::kBubblingPhase:
      result += kBubblingOffset;
      break;
  }

  if (const LocalDOMWindow* dom_window = event_target->ToLocalDOMWindow()) {
    // Treat the window as a root scroller as well.
    document = dom_window->document();
    result += kTouchTargetHistogramRootScrollerOffset;
  } else if (Node* node = event_target->ToNode()) {
    // Report if the target node is the document or body.
    if (node->IsDocumentNode() ||
        node->GetDocument().documentElement() == node ||
        node->GetDocument().body() == node) {
      result += kTouchTargetHistogramRootScrollerOffset;
    }
    document = &node->GetDocument();
  }

  if (document) {
    LocalFrameView* view = document->View();
    if (view && view->IsScrollable())
      result += kTouchTargetHistogramScrollableDocumentOffset;
  }

  if (default_prevented_before_current_target)
    result += kTouchTargetHistogramAlreadyHandledOffset;
  else if (default_prevented)
    result += kTouchTargetHistogramHandledOffset;
  else
    result += kTouchTargetHistogramNotHandledOffset;

  DEFINE_STATIC_LOCAL(EnumerationHistogram, root_document_listener_histogram,
                      ("Event.Touch.TargetAndDispatchResult2",
                       kTouchTargetAndDispatchResultTypeMax));
  root_document_listener_histogram.Count(
      static_cast<TouchTargetAndDispatchResultType>(result));
}

// Helper function to get WebTouchEvent from WebCoalescedInputEvent.
const WebTouchEvent* GetWebTouchEvent(const WebCoalescedInputEvent& event) {
  return static_cast<const WebTouchEvent*>(&event.Event());
}
}  // namespace

TouchEvent::TouchEvent()
    : default_prevented_before_current_target_(false),
      current_touch_action_(TouchAction::kTouchActionAuto) {}

TouchEvent::TouchEvent(const WebCoalescedInputEvent& event,
                       TouchList* touches,
                       TouchList* target_touches,
                       TouchList* changed_touches,
                       const AtomicString& type,
                       AbstractView* view,
                       TouchAction current_touch_action)
    // Pass a sourceCapabilities including the ability to fire touchevents when
    // creating this touchevent, which is always created from input device
    // capabilities from EventHandler.
    : UIEventWithKeyState(
          type,
          true,
          GetWebTouchEvent(event)->IsCancelable(),
          view,
          0,
          static_cast<WebInputEvent::Modifiers>(event.Event().GetModifiers()),
          TimeTicks::FromSeconds(event.Event().TimeStampSeconds()),
          view ? view->GetInputDeviceCapabilities()->FiresTouchEvents(true)
               : nullptr),
      touches_(touches),
      target_touches_(target_touches),
      changed_touches_(changed_touches),
      default_prevented_before_current_target_(false),
      current_touch_action_(current_touch_action) {
  DCHECK(WebInputEvent::IsTouchEventType(event.Event().GetType()));
  native_event_.reset(new WebCoalescedInputEvent(event));
}

TouchEvent::TouchEvent(const AtomicString& type,
                       const TouchEventInit& initializer)
    : UIEventWithKeyState(type, initializer),
      touches_(TouchList::Create(initializer.touches())),
      target_touches_(TouchList::Create(initializer.targetTouches())),
      changed_touches_(TouchList::Create(initializer.changedTouches())),
      current_touch_action_(TouchAction::kTouchActionAuto) {}

TouchEvent::~TouchEvent() {}

const AtomicString& TouchEvent::InterfaceName() const {
  return EventNames::TouchEvent;
}

bool TouchEvent::IsTouchEvent() const {
  return true;
}

void TouchEvent::preventDefault() {
  UIEventWithKeyState::preventDefault();

  // A common developer error is to wait too long before attempting to stop
  // scrolling by consuming a touchmove event. Generate a warning if this
  // event is uncancelable.
  MessageSource message_source = kJSMessageSource;
  String warning_message;
  switch (HandlingPassive()) {
    case PassiveMode::kNotPassive:
    case PassiveMode::kNotPassiveDefault:
      if (!cancelable()) {
        if (view() && view()->IsLocalDOMWindow() && view()->GetFrame()) {
          UseCounter::Count(
              ToLocalFrame(view()->GetFrame()),
              WebFeature::kUncancelableTouchEventPreventDefaulted);
        }

        if (native_event_ &&
            GetWebTouchEvent(*native_event_)->dispatch_type ==
                WebInputEvent::
                    kListenersForcedNonBlockingDueToMainThreadResponsiveness) {
          // Non blocking due to main thread responsiveness.
          if (view() && view()->IsLocalDOMWindow() && view()->GetFrame()) {
            UseCounter::Count(
                ToLocalFrame(view()->GetFrame()),
                WebFeature::
                    kUncancelableTouchEventDueToMainThreadResponsivenessPreventDefaulted);
          }
          message_source = kInterventionMessageSource;
          warning_message =
              "Ignored attempt to cancel a " + type() +
              " event with cancelable=false. This event was forced to be "
              "non-cancellable because the page was too busy to handle the "
              "event promptly.";
        } else {
          // Non blocking for any other reason.
          warning_message = "Ignored attempt to cancel a " + type() +
                            " event with cancelable=false, for example "
                            "because scrolling is in progress and "
                            "cannot be interrupted.";
        }
      }
      break;
    case PassiveMode::kPassiveForcedDocumentLevel:
      // Only enable the warning when the current touch action is auto because
      // an author may use touch action but call preventDefault for interop with
      // browsers that don't support touch-action.
      if (current_touch_action_ == TouchAction::kTouchActionAuto) {
        message_source = kInterventionMessageSource;
        warning_message =
            "Unable to preventDefault inside passive event listener due to "
            "target being treated as passive. See "
            "https://www.chromestatus.com/features/5093566007214080";
      }
      break;
    default:
      break;
  }

  if (!warning_message.IsEmpty() && view() && view()->IsLocalDOMWindow() &&
      view()->GetFrame()) {
    ToLocalDOMWindow(view())->GetFrame()->Console().AddMessage(
        ConsoleMessage::Create(message_source, kWarningMessageLevel,
                               warning_message));
  }

  if ((type() == EventTypeNames::touchstart ||
       type() == EventTypeNames::touchmove) &&
      view() && view()->IsLocalDOMWindow() && view()->GetFrame() &&
      current_touch_action_ == TouchAction::kTouchActionAuto) {
    switch (HandlingPassive()) {
      case PassiveMode::kNotPassiveDefault:
        UseCounter::Count(ToLocalFrame(view()->GetFrame()),
                          WebFeature::kTouchEventPreventedNoTouchAction);
        break;
      case PassiveMode::kPassiveForcedDocumentLevel:
        UseCounter::Count(
            ToLocalFrame(view()->GetFrame()),
            WebFeature::kTouchEventPreventedForcedDocumentPassiveNoTouchAction);
        break;
      default:
        break;
    }
  }
}

bool TouchEvent::IsTouchStartOrFirstTouchMove() const {
  if (!native_event_)
    return false;
  return GetWebTouchEvent(*native_event_)->touch_start_or_first_touch_move;
}

void TouchEvent::DoneDispatchingEventAtCurrentTarget() {
  // Do not log for non-cancelable events, events that don't block
  // scrolling, have more than one touch point or aren't on the main frame.
  if (!cancelable() || !IsTouchStartOrFirstTouchMove() ||
      !(touches_ && touches_->length() == 1) ||
      !(view() && view()->GetFrame() && view()->GetFrame()->IsMainFrame()))
    return;

  bool canceled = defaultPrevented();
  LogTouchTargetHistogram(currentTarget(), eventPhase(),
                          default_prevented_before_current_target_, canceled);
  default_prevented_before_current_target_ = canceled;
}

EventDispatchMediator* TouchEvent::CreateMediator() {
  return TouchEventDispatchMediator::Create(this);
}

DEFINE_TRACE(TouchEvent) {
  visitor->Trace(touches_);
  visitor->Trace(target_touches_);
  visitor->Trace(changed_touches_);
  UIEventWithKeyState::Trace(visitor);
}

TouchEventDispatchMediator* TouchEventDispatchMediator::Create(
    TouchEvent* touch_event) {
  return new TouchEventDispatchMediator(touch_event);
}

TouchEventDispatchMediator::TouchEventDispatchMediator(TouchEvent* touch_event)
    : EventDispatchMediator(touch_event) {}

TouchEvent& TouchEventDispatchMediator::Event() const {
  return ToTouchEvent(EventDispatchMediator::GetEvent());
}

DispatchEventResult TouchEventDispatchMediator::DispatchEvent(
    EventDispatcher& dispatcher) const {
  Event().GetEventPath().AdjustForTouchEvent(Event());
  return dispatcher.Dispatch();
}

}  // namespace blink
