/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/events/EventTarget.h"

#include <memory>
#include "bindings/core/v8/AddEventListenerOptionsOrBoolean.h"
#include "bindings/core/v8/EventListenerOptionsOrBoolean.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "core/dom/ExceptionCode.h"
#include "core/editing/Editor.h"
#include "core/events/Event.h"
#include "core/events/EventUtil.h"
#include "core/events/PointerEvent.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/PerformanceMonitor.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/probe/CoreProbes.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/Histogram.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/Vector.h"
#include "public/web/WebSettings.h"

namespace blink {
namespace {

enum PassiveForcedListenerResultType {
  kPreventDefaultNotCalled,
  kDocumentLevelTouchPreventDefaultCalled,
  kPassiveForcedListenerResultTypeMax
};

Event::PassiveMode EventPassiveMode(
    const RegisteredEventListener& event_listener) {
  if (!event_listener.Passive()) {
    if (event_listener.PassiveSpecified())
      return Event::PassiveMode::kNotPassive;
    return Event::PassiveMode::kNotPassiveDefault;
  }
  if (event_listener.PassiveForcedForDocumentTarget())
    return Event::PassiveMode::kPassiveForcedDocumentLevel;
  if (event_listener.PassiveSpecified())
    return Event::PassiveMode::kPassive;
  return Event::PassiveMode::kPassiveDefault;
}

Settings* WindowSettings(LocalDOMWindow* executing_window) {
  if (executing_window) {
    if (LocalFrame* frame = executing_window->GetFrame()) {
      return frame->GetSettings();
    }
  }
  return nullptr;
}

bool IsTouchScrollBlockingEvent(const AtomicString& event_type) {
  return event_type == EventTypeNames::touchstart ||
         event_type == EventTypeNames::touchmove;
}

bool IsScrollBlockingEvent(const AtomicString& event_type) {
  return IsTouchScrollBlockingEvent(event_type) ||
         event_type == EventTypeNames::mousewheel ||
         event_type == EventTypeNames::wheel;
}

double BlockedEventsWarningThreshold(ExecutionContext* context,
                                     const Event* event) {
  if (!event->cancelable())
    return 0.0;
  if (!IsScrollBlockingEvent(event->type()))
    return 0.0;
  return PerformanceMonitor::Threshold(context,
                                       PerformanceMonitor::kBlockedEvent);
}

void ReportBlockedEvent(ExecutionContext* context,
                        const Event* event,
                        RegisteredEventListener* registered_listener,
                        double delayed_seconds) {
  if (registered_listener->Listener()->GetType() !=
      EventListener::kJSEventListenerType)
    return;

  String message_text = String::Format(
      "Handling of '%s' input event was delayed for %ld ms due to main thread "
      "being busy. "
      "Consider marking event handler as 'passive' to make the page more "
      "responsive.",
      event->type().GetString().Utf8().data(), lround(delayed_seconds * 1000));

  PerformanceMonitor::ReportGenericViolation(
      context, PerformanceMonitor::kBlockedEvent, message_text, delayed_seconds,
      GetFunctionLocation(context, registered_listener->Listener()));
  registered_listener->SetBlockedEventWarningEmitted();
}

// UseCounts the event if it has the specified type. Returns true iff the event
// type matches.
bool CheckTypeThenUseCount(const Event* event,
                           const AtomicString& event_type_to_count,
                           const WebFeature feature,
                           const Document* document) {
  if (event->type() != event_type_to_count)
    return false;
  UseCounter::Count(*document, feature);
  return true;
}

}  // namespace

EventTargetData::EventTargetData() {}

EventTargetData::~EventTargetData() {}

DEFINE_TRACE(EventTargetData) {
  visitor->Trace(event_listener_map);
}

DEFINE_TRACE_WRAPPERS(EventTarget) {
  EventListenerIterator iterator(const_cast<EventTarget*>(this));
  while (EventListener* listener = iterator.NextListener()) {
    if (listener->GetType() != EventListener::kJSEventListenerType)
      continue;
    visitor->TraceWrappersWithManualWriteBarrier(
        static_cast<V8AbstractEventListener*>(listener));
  }
}

EventTarget::EventTarget() {}

EventTarget::~EventTarget() {}

Node* EventTarget::ToNode() {
  return nullptr;
}

const DOMWindow* EventTarget::ToDOMWindow() const {
  return nullptr;
}

const LocalDOMWindow* EventTarget::ToLocalDOMWindow() const {
  return nullptr;
}

LocalDOMWindow* EventTarget::ToLocalDOMWindow() {
  return nullptr;
}

MessagePort* EventTarget::ToMessagePort() {
  return nullptr;
}

ServiceWorker* EventTarget::ToServiceWorker() {
  return nullptr;
}

inline LocalDOMWindow* EventTarget::ExecutingWindow() {
  if (ExecutionContext* context = GetExecutionContext())
    return context->ExecutingWindow();
  return nullptr;
}

void EventTarget::SetDefaultAddEventListenerOptions(
    const AtomicString& event_type,
    EventListener* event_listener,
    AddEventListenerOptionsResolved& options) {
  options.SetPassiveSpecified(options.hasPassive());

  if (!IsScrollBlockingEvent(event_type)) {
    if (!options.hasPassive())
      options.setPassive(false);
    return;
  }

  LocalDOMWindow* executing_window = this->ExecutingWindow();
  if (executing_window) {
    if (options.hasPassive()) {
      UseCounter::Count(executing_window->document(),
                        options.passive()
                            ? WebFeature::kAddEventListenerPassiveTrue
                            : WebFeature::kAddEventListenerPassiveFalse);
    }
  }

  if (RuntimeEnabledFeatures::PassiveDocumentEventListenersEnabled() &&
      IsTouchScrollBlockingEvent(event_type)) {
    if (!options.hasPassive()) {
      if (Node* node = ToNode()) {
        if (node->IsDocumentNode() ||
            node->GetDocument().documentElement() == node ||
            node->GetDocument().body() == node) {
          options.setPassive(true);
          options.SetPassiveForcedForDocumentTarget(true);
          return;
        }
      } else if (ToLocalDOMWindow()) {
        options.setPassive(true);
        options.SetPassiveForcedForDocumentTarget(true);
        return;
      }
    }
  }

  // For mousewheel event listeners that have the target as the window and
  // a bound function name of "ssc_wheel" treat and no passive value default
  // passive to true. See crbug.com/501568.
  if (RuntimeEnabledFeatures::SmoothScrollJSInterventionEnabled() &&
      event_type == EventTypeNames::mousewheel && ToLocalDOMWindow() &&
      event_listener && !options.hasPassive()) {
    if (V8AbstractEventListener* v8_listener =
            V8AbstractEventListener::Cast(event_listener)) {
      v8::Local<v8::Object> function = v8_listener->GetExistingListenerObject();
      if (function->IsFunction() &&
          strcmp("ssc_wheel",
                 *v8::String::Utf8Value(
                     v8::Local<v8::Function>::Cast(function)->GetName())) ==
              0) {
        options.setPassive(true);
        if (executing_window) {
          UseCounter::Count(executing_window->document(),
                            WebFeature::kSmoothScrollJSInterventionActivated);

          executing_window->GetFrame()->Console().AddMessage(
              ConsoleMessage::Create(
                  kInterventionMessageSource, kWarningMessageLevel,
                  "Registering mousewheel event as passive due to "
                  "smoothscroll.js usage. The smoothscroll.js library is "
                  "buggy, no longer necessary and degrades performance. See "
                  "https://www.chromestatus.com/feature/5749447073988608"));
        }

        return;
      }
    }
  }

  if (Settings* settings = WindowSettings(ExecutingWindow())) {
    switch (settings->GetPassiveListenerDefault()) {
      case PassiveListenerDefault::kFalse:
        if (!options.hasPassive())
          options.setPassive(false);
        break;
      case PassiveListenerDefault::kTrue:
        if (!options.hasPassive())
          options.setPassive(true);
        break;
      case PassiveListenerDefault::kForceAllTrue:
        options.setPassive(true);
        break;
    }
  } else {
    if (!options.hasPassive())
      options.setPassive(false);
  }

  if (!options.passive()) {
    String message_text = String::Format(
        "Added non-passive event listener to a scroll-blocking '%s' event. "
        "Consider marking event handler as 'passive' to make the page more "
        "responsive. See "
        "https://www.chromestatus.com/feature/5745543795965952",
        event_type.GetString().Utf8().data());

    PerformanceMonitor::ReportGenericViolation(
        GetExecutionContext(), PerformanceMonitor::kDiscouragedAPIUse,
        message_text, 0, nullptr);
  }
}

bool EventTarget::addEventListener(const AtomicString& event_type,
                                   EventListener* listener,
                                   bool use_capture) {
  AddEventListenerOptionsResolved options;
  options.setCapture(use_capture);
  SetDefaultAddEventListenerOptions(event_type, listener, options);
  return AddEventListenerInternal(event_type, listener, options);
}

bool EventTarget::addEventListener(
    const AtomicString& event_type,
    EventListener* listener,
    const AddEventListenerOptionsOrBoolean& options_union) {
  if (options_union.isBoolean())
    return addEventListener(event_type, listener, options_union.getAsBoolean());
  if (options_union.isAddEventListenerOptions()) {
    AddEventListenerOptionsResolved options =
        options_union.getAsAddEventListenerOptions();
    return addEventListener(event_type, listener, options);
  }
  return addEventListener(event_type, listener);
}

bool EventTarget::addEventListener(const AtomicString& event_type,
                                   EventListener* listener,
                                   AddEventListenerOptionsResolved& options) {
  SetDefaultAddEventListenerOptions(event_type, listener, options);
  return AddEventListenerInternal(event_type, listener, options);
}

bool EventTarget::AddEventListenerInternal(
    const AtomicString& event_type,
    EventListener* listener,
    const AddEventListenerOptionsResolved& options) {
  if (!listener)
    return false;

  V8DOMActivityLogger* activity_logger =
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld();
  if (activity_logger) {
    Vector<String> argv;
    argv.push_back(ToNode() ? ToNode()->nodeName() : InterfaceName());
    argv.push_back(event_type);
    activity_logger->LogEvent("blinkAddEventListener", argv.size(),
                              argv.data());
  }

  RegisteredEventListener registered_listener;
  bool added = EnsureEventTargetData().event_listener_map.Add(
      event_type, listener, options, &registered_listener);
  if (added) {
    if (listener->GetType() == EventListener::kJSEventListenerType) {
      ScriptWrappableVisitor::WriteBarrier(
          static_cast<V8AbstractEventListener*>(listener));
    }
    AddedEventListener(event_type, registered_listener);
  }
  return added;
}

void EventTarget::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  if (const LocalDOMWindow* executing_window = this->ExecutingWindow()) {
    if (const Document* document = executing_window->document()) {
      if (event_type == EventTypeNames::auxclick)
        UseCounter::Count(*document, WebFeature::kAuxclickAddListenerCount);
      else if (event_type == EventTypeNames::appinstalled)
        UseCounter::Count(*document, WebFeature::kAppInstalledEventAddListener);
      else if (EventUtil::IsPointerEventType(event_type))
        UseCounter::Count(*document, WebFeature::kPointerEventAddListenerCount);
      else if (event_type == EventTypeNames::slotchange)
        UseCounter::Count(*document, WebFeature::kSlotChangeEventAddListener);
    }
  }
  if (EventUtil::IsDOMMutationEventType(event_type)) {
    if (ExecutionContext* context = GetExecutionContext()) {
      String message_text = String::Format(
          "Added synchronous DOM mutation listener to a '%s' event. "
          "Consider using MutationObserver to make the page more responsive.",
          event_type.GetString().Utf8().data());
      PerformanceMonitor::ReportGenericViolation(
          context, PerformanceMonitor::kDiscouragedAPIUse, message_text, 0,
          nullptr);
    }
  }
}

bool EventTarget::removeEventListener(const AtomicString& event_type,
                                      const EventListener* listener,
                                      bool use_capture) {
  EventListenerOptions options;
  options.setCapture(use_capture);
  return RemoveEventListenerInternal(event_type, listener, options);
}

bool EventTarget::removeEventListener(
    const AtomicString& event_type,
    const EventListener* listener,
    const EventListenerOptionsOrBoolean& options_union) {
  if (options_union.isBoolean())
    return removeEventListener(event_type, listener,
                               options_union.getAsBoolean());
  if (options_union.isEventListenerOptions()) {
    EventListenerOptions options = options_union.getAsEventListenerOptions();
    return removeEventListener(event_type, listener, options);
  }
  return removeEventListener(event_type, listener);
}

bool EventTarget::removeEventListener(const AtomicString& event_type,
                                      const EventListener* listener,
                                      EventListenerOptions& options) {
  return RemoveEventListenerInternal(event_type, listener, options);
}

bool EventTarget::RemoveEventListenerInternal(
    const AtomicString& event_type,
    const EventListener* listener,
    const EventListenerOptions& options) {
  if (!listener)
    return false;

  EventTargetData* d = GetEventTargetData();
  if (!d)
    return false;

  size_t index_of_removed_listener;
  RegisteredEventListener registered_listener;

  if (!d->event_listener_map.Remove(event_type, listener, options,
                                    &index_of_removed_listener,
                                    &registered_listener))
    return false;

  // Notify firing events planning to invoke the listener at 'index' that
  // they have one less listener to invoke.
  if (d->firing_event_iterators) {
    for (const auto& firing_iterator : *d->firing_event_iterators) {
      if (event_type != firing_iterator.event_type)
        continue;

      if (index_of_removed_listener >= firing_iterator.end)
        continue;

      --firing_iterator.end;
      // Note that when firing an event listener,
      // firingIterator.iterator indicates the next event listener
      // that would fire, not the currently firing event
      // listener. See EventTarget::fireEventListeners.
      if (index_of_removed_listener < firing_iterator.iterator)
        --firing_iterator.iterator;
    }
  }
  RemovedEventListener(event_type, registered_listener);
  return true;
}

void EventTarget::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {}

bool EventTarget::SetAttributeEventListener(const AtomicString& event_type,
                                            EventListener* listener) {
  ClearAttributeEventListener(event_type);
  if (!listener)
    return false;
  return addEventListener(event_type, listener, false);
}

EventListener* EventTarget::GetAttributeEventListener(
    const AtomicString& event_type) {
  EventListenerVector* listener_vector = GetEventListeners(event_type);
  if (!listener_vector)
    return nullptr;

  for (auto& event_listener : *listener_vector) {
    EventListener* listener = event_listener.Listener();
    if (listener->IsAttribute() &&
        listener->BelongsToTheCurrentWorld(GetExecutionContext()))
      return listener;
  }
  return nullptr;
}

bool EventTarget::ClearAttributeEventListener(const AtomicString& event_type) {
  EventListener* listener = GetAttributeEventListener(event_type);
  if (!listener)
    return false;
  return removeEventListener(event_type, listener, false);
}

bool EventTarget::dispatchEventForBindings(Event* event,
                                           ExceptionState& exception_state) {
  if (!event->WasInitialized()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The event provided is uninitialized.");
    return false;
  }
  if (event->IsBeingDispatched()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The event is already being dispatched.");
    return false;
  }

  if (!GetExecutionContext())
    return false;

  event->SetTrusted(false);

  // Return whether the event was cancelled or not to JS not that it
  // might have actually been default handled; so check only against
  // CanceledByEventHandler.
  return DispatchEventInternal(event) !=
         DispatchEventResult::kCanceledByEventHandler;
}

DispatchEventResult EventTarget::DispatchEvent(Event* event) {
  event->SetTrusted(true);
  return DispatchEventInternal(event);
}

DispatchEventResult EventTarget::DispatchEventInternal(Event* event) {
  event->SetTarget(this);
  event->SetCurrentTarget(this);
  event->SetEventPhase(Event::kAtTarget);
  DispatchEventResult dispatch_result = FireEventListeners(event);
  event->SetEventPhase(0);
  return dispatch_result;
}

void EventTarget::UncaughtExceptionInEventHandler() {}

static const AtomicString& LegacyType(const Event* event) {
  if (event->type() == EventTypeNames::transitionend)
    return EventTypeNames::webkitTransitionEnd;

  if (event->type() == EventTypeNames::animationstart)
    return EventTypeNames::webkitAnimationStart;

  if (event->type() == EventTypeNames::animationend)
    return EventTypeNames::webkitAnimationEnd;

  if (event->type() == EventTypeNames::animationiteration)
    return EventTypeNames::webkitAnimationIteration;

  if (event->type() == EventTypeNames::wheel)
    return EventTypeNames::mousewheel;

  return g_empty_atom;
}

void EventTarget::CountLegacyEvents(
    const AtomicString& legacy_type_name,
    EventListenerVector* listeners_vector,
    EventListenerVector* legacy_listeners_vector) {
  WebFeature unprefixed_feature;
  WebFeature prefixed_feature;
  WebFeature prefixed_and_unprefixed_feature;
  if (legacy_type_name == EventTypeNames::webkitTransitionEnd) {
    prefixed_feature = WebFeature::kPrefixedTransitionEndEvent;
    unprefixed_feature = WebFeature::kUnprefixedTransitionEndEvent;
    prefixed_and_unprefixed_feature =
        WebFeature::kPrefixedAndUnprefixedTransitionEndEvent;
  } else if (legacy_type_name == EventTypeNames::webkitAnimationEnd) {
    prefixed_feature = WebFeature::kPrefixedAnimationEndEvent;
    unprefixed_feature = WebFeature::kUnprefixedAnimationEndEvent;
    prefixed_and_unprefixed_feature =
        WebFeature::kPrefixedAndUnprefixedAnimationEndEvent;
  } else if (legacy_type_name == EventTypeNames::webkitAnimationStart) {
    prefixed_feature = WebFeature::kPrefixedAnimationStartEvent;
    unprefixed_feature = WebFeature::kUnprefixedAnimationStartEvent;
    prefixed_and_unprefixed_feature =
        WebFeature::kPrefixedAndUnprefixedAnimationStartEvent;
  } else if (legacy_type_name == EventTypeNames::webkitAnimationIteration) {
    prefixed_feature = WebFeature::kPrefixedAnimationIterationEvent;
    unprefixed_feature = WebFeature::kUnprefixedAnimationIterationEvent;
    prefixed_and_unprefixed_feature =
        WebFeature::kPrefixedAndUnprefixedAnimationIterationEvent;
  } else if (legacy_type_name == EventTypeNames::mousewheel) {
    prefixed_feature = WebFeature::kMouseWheelEvent;
    unprefixed_feature = WebFeature::kWheelEvent;
    prefixed_and_unprefixed_feature = WebFeature::kMouseWheelAndWheelEvent;
  } else {
    return;
  }

  if (const LocalDOMWindow* executing_window = this->ExecutingWindow()) {
    if (const Document* document = executing_window->document()) {
      if (legacy_listeners_vector) {
        if (listeners_vector)
          UseCounter::Count(*document, prefixed_and_unprefixed_feature);
        else
          UseCounter::Count(*document, prefixed_feature);
      } else if (listeners_vector) {
        UseCounter::Count(*document, unprefixed_feature);
      }
    }
  }
}

DispatchEventResult EventTarget::FireEventListeners(Event* event) {
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  DCHECK(event);
  DCHECK(event->WasInitialized());

  EventTargetData* d = GetEventTargetData();
  if (!d)
    return DispatchEventResult::kNotCanceled;

  EventListenerVector* legacy_listeners_vector = nullptr;
  AtomicString legacy_type_name = LegacyType(event);
  if (!legacy_type_name.IsEmpty())
    legacy_listeners_vector = d->event_listener_map.Find(legacy_type_name);

  EventListenerVector* listeners_vector =
      d->event_listener_map.Find(event->type());

  bool fired_event_listeners = false;
  if (listeners_vector) {
    fired_event_listeners = FireEventListeners(event, d, *listeners_vector);
  } else if (event->isTrusted() && legacy_listeners_vector) {
    AtomicString unprefixed_type_name = event->type();
    event->SetType(legacy_type_name);
    fired_event_listeners =
        FireEventListeners(event, d, *legacy_listeners_vector);
    event->SetType(unprefixed_type_name);
  }

  // Only invoke the callback if event listeners were fired for this phase.
  if (fired_event_listeners) {
    event->DoneDispatchingEventAtCurrentTarget();

    // Only count uma metrics if we really fired an event listener.
    Editor::CountEvent(GetExecutionContext(), event);
    CountLegacyEvents(legacy_type_name, listeners_vector,
                      legacy_listeners_vector);
  }
  return GetDispatchEventResult(*event);
}

bool EventTarget::FireEventListeners(Event* event,
                                     EventTargetData* d,
                                     EventListenerVector& entry) {
  // Fire all listeners registered for this event. Don't fire listeners removed
  // during event dispatch. Also, don't fire event listeners added during event
  // dispatch. Conveniently, all new event listeners will be added after or at
  // index |size|, so iterating up to (but not including) |size| naturally
  // excludes new event listeners.
  if (const LocalDOMWindow* executing_window = this->ExecutingWindow()) {
    if (const Document* document = executing_window->document()) {
      if (CheckTypeThenUseCount(event, EventTypeNames::beforeunload,
                                WebFeature::kDocumentBeforeUnloadFired,
                                document)) {
        if (executing_window != executing_window->top())
          UseCounter::Count(*document, WebFeature::kSubFrameBeforeUnloadFired);
      } else if (CheckTypeThenUseCount(event, EventTypeNames::unload,
                                       WebFeature::kDocumentUnloadFired,
                                       document)) {
      } else if (CheckTypeThenUseCount(event, EventTypeNames::DOMFocusIn,
                                       WebFeature::kDOMFocusInOutEvent,
                                       document)) {
      } else if (CheckTypeThenUseCount(event, EventTypeNames::DOMFocusOut,
                                       WebFeature::kDOMFocusInOutEvent,
                                       document)) {
      } else if (CheckTypeThenUseCount(event, EventTypeNames::focusin,
                                       WebFeature::kFocusInOutEvent,
                                       document)) {
      } else if (CheckTypeThenUseCount(event, EventTypeNames::focusout,
                                       WebFeature::kFocusInOutEvent,
                                       document)) {
      } else if (CheckTypeThenUseCount(event, EventTypeNames::textInput,
                                       WebFeature::kTextInputFired, document)) {
      } else if (CheckTypeThenUseCount(event, EventTypeNames::touchstart,
                                       WebFeature::kTouchStartFired,
                                       document)) {
      } else if (CheckTypeThenUseCount(event, EventTypeNames::mousedown,
                                       WebFeature::kMouseDownFired, document)) {
      } else if (CheckTypeThenUseCount(event, EventTypeNames::pointerdown,
                                       WebFeature::kPointerDownFired,
                                       document)) {
        if (event->IsPointerEvent() &&
            static_cast<PointerEvent*>(event)->pointerType() == "touch") {
          UseCounter::Count(*document, WebFeature::kPointerDownFiredForTouch);
        }
      } else if (CheckTypeThenUseCount(event, EventTypeNames::pointerenter,
                                       WebFeature::kPointerEnterLeaveFired,
                                       document)) {
      } else if (CheckTypeThenUseCount(event, EventTypeNames::pointerleave,
                                       WebFeature::kPointerEnterLeaveFired,
                                       document)) {
      } else if (CheckTypeThenUseCount(event, EventTypeNames::pointerover,
                                       WebFeature::kPointerOverOutFired,
                                       document)) {
      } else if (CheckTypeThenUseCount(event, EventTypeNames::pointerout,
                                       WebFeature::kPointerOverOutFired,
                                       document)) {
      }
    }
  }

  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return false;

  size_t i = 0;
  size_t size = entry.size();
  if (!d->firing_event_iterators)
    d->firing_event_iterators = WTF::WrapUnique(new FiringEventIteratorVector);
  d->firing_event_iterators->push_back(
      FiringEventIterator(event->type(), i, size));

  double blocked_event_threshold =
      BlockedEventsWarningThreshold(context, event);
  TimeTicks now;
  bool should_report_blocked_event = false;
  if (blocked_event_threshold) {
    now = TimeTicks::Now();
    should_report_blocked_event =
        (now - event->PlatformTimeStamp()).InSecondsF() >
        blocked_event_threshold;
  }
  bool fired_listener = false;

  while (i < size) {
    RegisteredEventListener registered_listener = entry[i];

    // Move the iterator past this event listener. This must match
    // the handling of the FiringEventIterator::iterator in
    // EventTarget::removeEventListener.
    ++i;

    if (event->eventPhase() == Event::kCapturingPhase &&
        !registered_listener.Capture())
      continue;
    if (event->eventPhase() == Event::kBubblingPhase &&
        registered_listener.Capture())
      continue;

    EventListener* listener = registered_listener.Listener();
    // The listener will be retained by Member<EventListener> in the
    // registeredListener, i and size are updated with the firing event iterator
    // in case the listener is removed from the listener vector below.
    if (registered_listener.Once())
      removeEventListener(event->type(), listener,
                          registered_listener.Capture());

    // If stopImmediatePropagation has been called, we just break out
    // immediately, without handling any more events on this target.
    if (event->ImmediatePropagationStopped())
      break;

    event->SetHandlingPassive(EventPassiveMode(registered_listener));
    bool passive_forced = registered_listener.PassiveForcedForDocumentTarget();

    probe::UserCallback probe(context, nullptr, event->type(), false, this);

    // To match Mozilla, the AT_TARGET phase fires both capturing and bubbling
    // event listeners, even though that violates some versions of the DOM spec.
    listener->handleEvent(context, event);
    fired_listener = true;

    // If we're about to report this event listener as blocking, make sure it
    // wasn't removed while handling the event.
    if (should_report_blocked_event && i > 0 &&
        entry[i - 1].Listener() == listener && !entry[i - 1].Passive() &&
        !entry[i - 1].BlockedEventWarningEmitted() &&
        !event->defaultPrevented()) {
      ReportBlockedEvent(context, event, &entry[i - 1],
                         (now - event->PlatformTimeStamp()).InSecondsF());
    }

    if (passive_forced) {
      DEFINE_STATIC_LOCAL(EnumerationHistogram, passive_forced_histogram,
                          ("Event.PassiveForcedEventDispatchCancelled",
                           kPassiveForcedListenerResultTypeMax));
      PassiveForcedListenerResultType breakage_type = kPreventDefaultNotCalled;
      if (event->PreventDefaultCalledDuringPassive())
        breakage_type = kDocumentLevelTouchPreventDefaultCalled;

      passive_forced_histogram.Count(breakage_type);
    }

    event->SetHandlingPassive(Event::PassiveMode::kNotPassive);

    CHECK_LE(i, size);
  }
  d->firing_event_iterators->pop_back();
  return fired_listener;
}

DispatchEventResult EventTarget::GetDispatchEventResult(const Event& event) {
  if (event.defaultPrevented())
    return DispatchEventResult::kCanceledByEventHandler;
  if (event.DefaultHandled())
    return DispatchEventResult::kCanceledByDefaultEventHandler;
  return DispatchEventResult::kNotCanceled;
}

EventListenerVector* EventTarget::GetEventListeners(
    const AtomicString& event_type) {
  EventTargetData* data = GetEventTargetData();
  if (!data)
    return nullptr;
  return data->event_listener_map.Find(event_type);
}

Vector<AtomicString> EventTarget::EventTypes() {
  EventTargetData* d = GetEventTargetData();
  return d ? d->event_listener_map.EventTypes() : Vector<AtomicString>();
}

void EventTarget::RemoveAllEventListeners() {
  EventTargetData* d = GetEventTargetData();
  if (!d)
    return;
  d->event_listener_map.Clear();

  // Notify firing events planning to invoke the listener at 'index' that
  // they have one less listener to invoke.
  if (d->firing_event_iterators) {
    for (const auto& iterator : *d->firing_event_iterators) {
      iterator.iterator = 0;
      iterator.end = 0;
    }
  }
}

STATIC_ASSERT_ENUM(WebSettings::PassiveEventListenerDefault::kFalse,
                   PassiveListenerDefault::kFalse);
STATIC_ASSERT_ENUM(WebSettings::PassiveEventListenerDefault::kTrue,
                   PassiveListenerDefault::kTrue);
STATIC_ASSERT_ENUM(WebSettings::PassiveEventListenerDefault::kForceAllTrue,
                   PassiveListenerDefault::kForceAllTrue);

}  // namespace blink
