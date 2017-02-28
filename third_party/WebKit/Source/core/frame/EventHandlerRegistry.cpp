// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/EventHandlerRegistry.h"

#include "core/events/EventListenerOptions.h"
#include "core/events/EventUtil.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"

namespace blink {

namespace {

WebEventListenerProperties webEventListenerProperties(bool hasBlocking,
                                                      bool hasPassive) {
  if (hasBlocking && hasPassive)
    return WebEventListenerProperties::BlockingAndPassive;
  if (hasBlocking)
    return WebEventListenerProperties::Blocking;
  if (hasPassive)
    return WebEventListenerProperties::Passive;
  return WebEventListenerProperties::Nothing;
}

}  // namespace

EventHandlerRegistry::EventHandlerRegistry(FrameHost& frameHost)
    : m_frameHost(&frameHost) {}

EventHandlerRegistry::~EventHandlerRegistry() {
  for (size_t i = 0; i < EventHandlerClassCount; ++i) {
    EventHandlerClass handlerClass = static_cast<EventHandlerClass>(i);
    checkConsistency(handlerClass);
  }
}

bool EventHandlerRegistry::eventTypeToClass(
    const AtomicString& eventType,
    const AddEventListenerOptions& options,
    EventHandlerClass* result) {
  if (eventType == EventTypeNames::scroll) {
    *result = ScrollEvent;
  } else if (eventType == EventTypeNames::wheel ||
             eventType == EventTypeNames::mousewheel) {
    *result = options.passive() ? WheelEventPassive : WheelEventBlocking;
  } else if (eventType == EventTypeNames::touchend ||
             eventType == EventTypeNames::touchcancel) {
    *result = options.passive() ? TouchEndOrCancelEventPassive
                                : TouchEndOrCancelEventBlocking;
  } else if (eventType == EventTypeNames::touchstart ||
             eventType == EventTypeNames::touchmove) {
    *result = options.passive() ? TouchStartOrMoveEventPassive
                                : TouchStartOrMoveEventBlocking;
  } else if (EventUtil::isPointerEventType(eventType)) {
    // The EventHandlerClass is TouchStartOrMoveEventPassive since
    // the pointer events never block scrolling and the compositor
    // only needs to know about the touch listeners.
    *result = TouchStartOrMoveEventPassive;
#if DCHECK_IS_ON()
  } else if (eventType == EventTypeNames::load ||
             eventType == EventTypeNames::mousemove ||
             eventType == EventTypeNames::touchstart) {
    *result = EventsForTesting;
#endif
  } else {
    return false;
  }
  return true;
}

const EventTargetSet* EventHandlerRegistry::eventHandlerTargets(
    EventHandlerClass handlerClass) const {
  checkConsistency(handlerClass);
  return &m_targets[handlerClass];
}

bool EventHandlerRegistry::hasEventHandlers(
    EventHandlerClass handlerClass) const {
  checkConsistency(handlerClass);
  return m_targets[handlerClass].size();
}

bool EventHandlerRegistry::updateEventHandlerTargets(
    ChangeOperation op,
    EventHandlerClass handlerClass,
    EventTarget* target) {
  EventTargetSet* targets = &m_targets[handlerClass];
  if (op == Add) {
    if (!targets->add(target).isNewEntry) {
      // Just incremented refcount, no real change.
      return false;
    }
  } else {
    ASSERT(op == Remove || op == RemoveAll);
    ASSERT(op == RemoveAll || targets->contains(target));

    if (op == RemoveAll) {
      if (!targets->contains(target))
        return false;
      targets->removeAll(target);
    } else {
      if (!targets->remove(target)) {
        // Just decremented refcount, no real update.
        return false;
      }
    }
  }
  return true;
}

void EventHandlerRegistry::updateEventHandlerInternal(
    ChangeOperation op,
    EventHandlerClass handlerClass,
    EventTarget* target) {
  bool hadHandlers = m_targets[handlerClass].size();
  bool targetSetChanged = updateEventHandlerTargets(op, handlerClass, target);
  bool hasHandlers = m_targets[handlerClass].size();

  if (hadHandlers != hasHandlers) {
    LocalFrame* frame = nullptr;
    if (Node* node = target->toNode()) {
      frame = node->document().frame();
    } else if (LocalDOMWindow* domWindow = target->toLocalDOMWindow()) {
      frame = domWindow->frame();
    } else {
      NOTREACHED() << "Unexpected target type for event handler.";
    }

    notifyHasHandlersChanged(frame, handlerClass, hasHandlers);
  }

  if (targetSetChanged)
    notifyDidAddOrRemoveEventHandlerTarget(handlerClass);
}

void EventHandlerRegistry::updateEventHandlerOfType(
    ChangeOperation op,
    const AtomicString& eventType,
    const AddEventListenerOptions& options,
    EventTarget* target) {
  EventHandlerClass handlerClass;
  if (!eventTypeToClass(eventType, options, &handlerClass))
    return;
  updateEventHandlerInternal(op, handlerClass, target);
}

void EventHandlerRegistry::didAddEventHandler(
    EventTarget& target,
    const AtomicString& eventType,
    const AddEventListenerOptions& options) {
  updateEventHandlerOfType(Add, eventType, options, &target);
}

void EventHandlerRegistry::didRemoveEventHandler(
    EventTarget& target,
    const AtomicString& eventType,
    const AddEventListenerOptions& options) {
  updateEventHandlerOfType(Remove, eventType, options, &target);
}

void EventHandlerRegistry::didAddEventHandler(EventTarget& target,
                                              EventHandlerClass handlerClass) {
  updateEventHandlerInternal(Add, handlerClass, &target);
}

void EventHandlerRegistry::didRemoveEventHandler(
    EventTarget& target,
    EventHandlerClass handlerClass) {
  updateEventHandlerInternal(Remove, handlerClass, &target);
}

void EventHandlerRegistry::didMoveIntoFrameHost(EventTarget& target) {
  if (!target.hasEventListeners())
    return;

  // This code is not efficient at all.
  Vector<AtomicString> eventTypes = target.eventTypes();
  for (size_t i = 0; i < eventTypes.size(); ++i) {
    EventListenerVector* listeners = target.getEventListeners(eventTypes[i]);
    if (!listeners)
      continue;
    for (unsigned count = listeners->size(); count > 0; --count) {
      EventHandlerClass handlerClass;
      if (!eventTypeToClass(eventTypes[i], (*listeners)[count - 1].options(),
                            &handlerClass))
        continue;

      didAddEventHandler(target, handlerClass);
    }
  }
}

void EventHandlerRegistry::didMoveOutOfFrameHost(EventTarget& target) {
  didRemoveAllEventHandlers(target);
}

void EventHandlerRegistry::didRemoveAllEventHandlers(EventTarget& target) {
  for (size_t i = 0; i < EventHandlerClassCount; ++i) {
    EventHandlerClass handlerClass = static_cast<EventHandlerClass>(i);
    updateEventHandlerInternal(RemoveAll, handlerClass, &target);
  }
}

void EventHandlerRegistry::notifyHasHandlersChanged(
    LocalFrame* frame,
    EventHandlerClass handlerClass,
    bool hasActiveHandlers) {
  switch (handlerClass) {
    case ScrollEvent:
      m_frameHost->page().chromeClient().setHasScrollEventHandlers(
          frame, hasActiveHandlers);
      break;
    case WheelEventBlocking:
    case WheelEventPassive:
      m_frameHost->page().chromeClient().setEventListenerProperties(
          frame, WebEventListenerClass::MouseWheel,
          webEventListenerProperties(hasEventHandlers(WheelEventBlocking),
                                     hasEventHandlers(WheelEventPassive)));
      break;
    case TouchStartOrMoveEventBlocking:
    case TouchStartOrMoveEventPassive:
      m_frameHost->page().chromeClient().setEventListenerProperties(
          frame, WebEventListenerClass::TouchStartOrMove,
          webEventListenerProperties(
              hasEventHandlers(TouchStartOrMoveEventBlocking),
              hasEventHandlers(TouchStartOrMoveEventPassive)));
      break;
    case TouchEndOrCancelEventBlocking:
    case TouchEndOrCancelEventPassive:
      m_frameHost->page().chromeClient().setEventListenerProperties(
          frame, WebEventListenerClass::TouchEndOrCancel,
          webEventListenerProperties(
              hasEventHandlers(TouchEndOrCancelEventBlocking),
              hasEventHandlers(TouchEndOrCancelEventPassive)));
      break;
#if DCHECK_IS_ON()
    case EventsForTesting:
      break;
#endif
    default:
      ASSERT_NOT_REACHED();
      break;
  }
}

void EventHandlerRegistry::notifyDidAddOrRemoveEventHandlerTarget(
    EventHandlerClass handlerClass) {
  ScrollingCoordinator* scrollingCoordinator =
      m_frameHost->page().scrollingCoordinator();
  if (scrollingCoordinator && handlerClass == TouchStartOrMoveEventBlocking)
    scrollingCoordinator->touchEventTargetRectsDidChange();
}

DEFINE_TRACE(EventHandlerRegistry) {
  visitor->trace(m_frameHost);
  visitor->template registerWeakMembers<
      EventHandlerRegistry, &EventHandlerRegistry::clearWeakMembers>(this);
}

void EventHandlerRegistry::clearWeakMembers(Visitor* visitor) {
  Vector<UntracedMember<EventTarget>> deadTargets;
  for (size_t i = 0; i < EventHandlerClassCount; ++i) {
    EventHandlerClass handlerClass = static_cast<EventHandlerClass>(i);
    const EventTargetSet* targets = &m_targets[handlerClass];
    for (const auto& eventTarget : *targets) {
      Node* node = eventTarget.key->toNode();
      LocalDOMWindow* window = eventTarget.key->toLocalDOMWindow();
      if (node && !ThreadHeap::isHeapObjectAlive(node)) {
        deadTargets.push_back(node);
      } else if (window && !ThreadHeap::isHeapObjectAlive(window)) {
        deadTargets.push_back(window);
      }
    }
  }
  for (size_t i = 0; i < deadTargets.size(); ++i)
    didRemoveAllEventHandlers(*deadTargets[i]);
}

void EventHandlerRegistry::documentDetached(Document& document) {
  // Remove all event targets under the detached document.
  for (size_t handlerClassIndex = 0; handlerClassIndex < EventHandlerClassCount;
       ++handlerClassIndex) {
    EventHandlerClass handlerClass =
        static_cast<EventHandlerClass>(handlerClassIndex);
    Vector<UntracedMember<EventTarget>> targetsToRemove;
    const EventTargetSet* targets = &m_targets[handlerClass];
    for (const auto& eventTarget : *targets) {
      if (Node* node = eventTarget.key->toNode()) {
        for (Document* doc = &node->document(); doc;
             doc = doc->localOwner() ? &doc->localOwner()->document() : 0) {
          if (doc == &document) {
            targetsToRemove.push_back(eventTarget.key);
            break;
          }
        }
      } else if (eventTarget.key->toLocalDOMWindow()) {
        // DOMWindows may outlive their documents, so we shouldn't remove their
        // handlers here.
      } else {
        ASSERT_NOT_REACHED();
      }
    }
    for (size_t i = 0; i < targetsToRemove.size(); ++i)
      updateEventHandlerInternal(RemoveAll, handlerClass, targetsToRemove[i]);
  }
}

void EventHandlerRegistry::checkConsistency(
    EventHandlerClass handlerClass) const {
#if DCHECK_IS_ON()
  const EventTargetSet* targets = &m_targets[handlerClass];
  for (const auto& eventTarget : *targets) {
    if (Node* node = eventTarget.key->toNode()) {
      // See the comment for |documentDetached| if either of these assertions
      // fails.
      DCHECK(node->document().frameHost());
      DCHECK(node->document().frameHost() == m_frameHost);
    } else if (LocalDOMWindow* window = eventTarget.key->toLocalDOMWindow()) {
      // If any of these assertions fail, LocalDOMWindow failed to unregister
      // its handlers properly.
      DCHECK(window->frame());
      DCHECK(window->frame()->host());
      DCHECK(window->frame()->host() == m_frameHost);
    }
  }
#endif  // DCHECK_IS_ON()
}

}  // namespace blink
