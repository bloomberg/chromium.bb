/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/events/EventDispatcher.h"

#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/events/EventDispatchMediator.h"
#include "core/events/MouseEvent.h"
#include "core/events/ScopedEventQueue.h"
#include "core/events/WindowEventContext.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLElement.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

DispatchEventResult EventDispatcher::DispatchEvent(
    Node& node,
    EventDispatchMediator* mediator) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
               "EventDispatcher::dispatchEvent");
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  EventDispatcher dispatcher(node, &mediator->GetEvent());
  return mediator->DispatchEvent(dispatcher);
}

EventDispatcher::EventDispatcher(Node& node, Event* event)
    : node_(node), event_(event) {
  DCHECK(event_.Get());
  view_ = node.GetDocument().View();
  event_->InitEventPath(*node_);
}

void EventDispatcher::DispatchScopedEvent(Node& node,
                                          EventDispatchMediator* mediator) {
  // We need to set the target here because it can go away by the time we
  // actually fire the event.
  mediator->GetEvent().SetTarget(
      EventPath::EventTargetRespectingTargetRules(node));
  ScopedEventQueue::Instance()->EnqueueEventDispatchMediator(mediator);
}

void EventDispatcher::DispatchSimulatedClick(
    Node& node,
    Event* underlying_event,
    SimulatedClickMouseEventOptions mouse_event_options,
    SimulatedClickCreationScope creation_scope) {
  // This persistent vector doesn't cause leaks, because added Nodes are removed
  // before dispatchSimulatedClick() returns. This vector is here just to
  // prevent the code from running into an infinite recursion of
  // dispatchSimulatedClick().
  DEFINE_STATIC_LOCAL(HeapHashSet<Member<Node>>,
                      nodes_dispatching_simulated_clicks,
                      (new HeapHashSet<Member<Node>>));

  if (IsDisabledFormControl(&node))
    return;

  if (nodes_dispatching_simulated_clicks.Contains(&node))
    return;

  nodes_dispatching_simulated_clicks.insert(&node);

  if (mouse_event_options == kSendMouseOverUpDownEvents)
    EventDispatcher(node, MouseEvent::Create(EventTypeNames::mouseover,
                                             node.GetDocument().domWindow(),
                                             underlying_event, creation_scope))
        .Dispatch();

  if (mouse_event_options != kSendNoEvents) {
    EventDispatcher(node, MouseEvent::Create(EventTypeNames::mousedown,
                                             node.GetDocument().domWindow(),
                                             underlying_event, creation_scope))
        .Dispatch();
    node.SetActive(true);
    EventDispatcher(node, MouseEvent::Create(EventTypeNames::mouseup,
                                             node.GetDocument().domWindow(),
                                             underlying_event, creation_scope))
        .Dispatch();
  }
  // Some elements (e.g. the color picker) may set active state to true before
  // calling this method and expect the state to be reset during the call.
  node.SetActive(false);

  // always send click
  EventDispatcher(node, MouseEvent::Create(EventTypeNames::click,
                                           node.GetDocument().domWindow(),
                                           underlying_event, creation_scope))
      .Dispatch();

  nodes_dispatching_simulated_clicks.erase(&node);
}

// https://dom.spec.whatwg.org/#dispatching-events
DispatchEventResult EventDispatcher::Dispatch() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
               "EventDispatcher::dispatch");

#if DCHECK_IS_ON()
  DCHECK(!event_dispatched_);
  event_dispatched_ = true;
#endif
  if (GetEvent().GetEventPath().IsEmpty()) {
    // eventPath() can be empty if event path is shrinked by relataedTarget
    // retargeting.
    return DispatchEventResult::kNotCanceled;
  }
  event_->GetEventPath().EnsureWindowEventContext();

  // 6. Let isActivationEvent be true, if event is a MouseEvent object and
  // event's type attribute is "click", and false otherwise.
  //
  // We need to include non-standard textInput event for HTMLInputElement.
  const bool is_activation_event =
      (event_->IsMouseEvent() && event_->type() == EventTypeNames::click) ||
      event_->type() == EventTypeNames::textInput;

  // 7. Let activationTarget be target, if isActivationEvent is true and target
  // has activation behavior, and null otherwise.
  Node* activation_target =
      is_activation_event && node_->HasActivationBehavior() ? node_ : nullptr;

  // A part of step 9 loop.
  if (is_activation_event && !activation_target && event_->bubbles()) {
    size_t size = event_->GetEventPath().size();
    for (size_t i = 1; i < size; ++i) {
      Node* target = event_->GetEventPath()[i].GetNode();
      if (target->HasActivationBehavior()) {
        activation_target = target;
        break;
      }
    }
  }

  event_->SetTarget(EventPath::EventTargetRespectingTargetRules(*node_));
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  DCHECK(event_->target());
  TRACE_EVENT1("devtools.timeline", "EventDispatch", "data",
               InspectorEventDispatchEvent::Data(*event_));
  EventDispatchHandlingState* pre_dispatch_event_handler_result = nullptr;
  if (DispatchEventPreProcess(activation_target,
                              pre_dispatch_event_handler_result) ==
      kContinueDispatching) {
    if (DispatchEventAtCapturing() == kContinueDispatching) {
      if (DispatchEventAtTarget() == kContinueDispatching)
        DispatchEventAtBubbling();
    }
  }
  DispatchEventPostProcess(activation_target,
                           pre_dispatch_event_handler_result);

  // Ensure that after event dispatch, the event's target object is the
  // outermost shadow DOM boundary.
  event_->SetTarget(event_->GetEventPath().GetWindowEventContext().Target());
  event_->SetCurrentTarget(nullptr);

  return EventTarget::GetDispatchEventResult(*event_);
}

inline EventDispatchContinuation EventDispatcher::DispatchEventPreProcess(
    Node* activation_target,
    EventDispatchHandlingState*& pre_dispatch_event_handler_result) {
  // 11. If activationTarget is non-null and activationTarget has
  // legacy-pre-activation behavior, then run activationTarget's
  // legacy-pre-activation behavior.
  if (activation_target) {
    pre_dispatch_event_handler_result =
        activation_target->PreDispatchEventHandler(event_.Get());
  }
  return (event_->GetEventPath().IsEmpty() || event_->PropagationStopped())
             ? kDoneDispatching
             : kContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::DispatchEventAtCapturing() {
  // Trigger capturing event handlers, starting at the top and working our way
  // down.
  event_->SetEventPhase(Event::kCapturingPhase);

  if (event_->GetEventPath().GetWindowEventContext().HandleLocalEvents(
          *event_) &&
      event_->PropagationStopped())
    return kDoneDispatching;

  for (size_t i = event_->GetEventPath().size() - 1; i > 0; --i) {
    const NodeEventContext& event_context = event_->GetEventPath()[i];
    if (event_context.CurrentTargetSameAsTarget())
      continue;
    event_context.HandleLocalEvents(*event_);
    if (event_->PropagationStopped())
      return kDoneDispatching;
  }

  return kContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::DispatchEventAtTarget() {
  event_->SetEventPhase(Event::kAtTarget);
  event_->GetEventPath()[0].HandleLocalEvents(*event_);
  return event_->PropagationStopped() ? kDoneDispatching : kContinueDispatching;
}

inline void EventDispatcher::DispatchEventAtBubbling() {
  // Trigger bubbling event handlers, starting at the bottom and working our way
  // up.
  size_t size = event_->GetEventPath().size();
  for (size_t i = 1; i < size; ++i) {
    const NodeEventContext& event_context = event_->GetEventPath()[i];
    if (event_context.CurrentTargetSameAsTarget()) {
      event_->SetEventPhase(Event::kAtTarget);
    } else if (event_->bubbles() && !event_->cancelBubble()) {
      event_->SetEventPhase(Event::kBubblingPhase);
    } else {
      continue;
    }
    event_context.HandleLocalEvents(*event_);
    if (event_->PropagationStopped())
      return;
  }
  if (event_->bubbles() && !event_->cancelBubble()) {
    event_->SetEventPhase(Event::kBubblingPhase);
    event_->GetEventPath().GetWindowEventContext().HandleLocalEvents(*event_);
  }
}

inline void EventDispatcher::DispatchEventPostProcess(
    Node* activation_target,
    EventDispatchHandlingState* pre_dispatch_event_handler_result) {
  event_->SetTarget(EventPath::EventTargetRespectingTargetRules(*node_));
  // https://dom.spec.whatwg.org/#concept-event-dispatch
  // 14. Unset event’s dispatch flag, stop propagation flag, and stop immediate
  // propagation flag.
  event_->SetStopPropagation(false);
  event_->SetStopImmediatePropagation(false);
  // 15. Set event’s eventPhase attribute to NONE.
  event_->SetEventPhase(0);
  // 16. Set event’s currentTarget attribute to null.
  event_->SetCurrentTarget(nullptr);

  bool is_click = event_->IsMouseEvent() &&
                  ToMouseEvent(*event_).type() == EventTypeNames::click;
  if (is_click) {
    // Fire an accessibility event indicating a node was clicked on.  This is
    // safe if m_event->target()->toNode() returns null.
    if (AXObjectCache* cache = node_->GetDocument().ExistingAXObjectCache())
      cache->HandleClicked(event_->target()->ToNode());

    // Pass the data from the PreDispatchEventHandler to the
    // PostDispatchEventHandler.
    // This may dispatch an event, and node_ and event_ might be altered.
    if (activation_target) {
      activation_target->PostDispatchEventHandler(
          event_.Get(), pre_dispatch_event_handler_result);
    }
    // TODO(tkent): Is it safe to kick DefaultEventHandler() with such altered
    // event_?
  }

  // The DOM Events spec says that events dispatched by JS (other than "click")
  // should not have their default handlers invoked.
  bool is_trusted_or_click =
      !RuntimeEnabledFeatures::TrustedEventsDefaultActionEnabled() ||
      event_->isTrusted() || is_click;

  // For Android WebView (distinguished by wideViewportQuirkEnabled)
  // enable untrusted events for mouse down on select elements because
  // fastclick.js seems to generate these. crbug.com/642698
  // TODO(dtapuska): Change this to a target SDK quirk crbug.com/643705
  if (!is_trusted_or_click && event_->IsMouseEvent() &&
      event_->type() == EventTypeNames::mousedown &&
      isHTMLSelectElement(*node_)) {
    if (Settings* settings = node_->GetDocument().GetSettings()) {
      is_trusted_or_click = settings->GetWideViewportQuirkEnabled();
    }
  }

  // Call default event handlers. While the DOM does have a concept of
  // preventing default handling, the detail of which handlers are called is an
  // internal implementation detail and not part of the DOM.
  if (!event_->defaultPrevented() && !event_->DefaultHandled() &&
      is_trusted_or_click) {
    // Non-bubbling events call only one default event handler, the one for the
    // target.
    node_->WillCallDefaultEventHandler(*event_);
    node_->DefaultEventHandler(event_.Get());
    DCHECK(!event_->defaultPrevented());
    // For bubbling events, call default event handlers on the same targets in
    // the same order as the bubbling phase.
    if (!event_->DefaultHandled() && event_->bubbles()) {
      size_t size = event_->GetEventPath().size();
      for (size_t i = 1; i < size; ++i) {
        event_->GetEventPath()[i].GetNode()->WillCallDefaultEventHandler(
            *event_);
        event_->GetEventPath()[i].GetNode()->DefaultEventHandler(event_.Get());
        DCHECK(!event_->defaultPrevented());
        if (event_->DefaultHandled())
          break;
      }
    }
  }

  // Track the usage of sending a mousedown event to a select element to force
  // it to open. This measures a possible breakage of not allowing untrusted
  // events to open select boxes.
  if (!event_->isTrusted() && event_->IsMouseEvent() &&
      event_->type() == EventTypeNames::mousedown &&
      isHTMLSelectElement(*node_)) {
    UseCounter::Count(node_->GetDocument(),
                      WebFeature::kUntrustedMouseDownEventDispatchedToSelect);
  }
}

}  // namespace blink
