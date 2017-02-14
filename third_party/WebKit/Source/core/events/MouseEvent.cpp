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

#include "core/events/MouseEvent.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Element.h"
#include "core/events/EventDispatcher.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/input/InputDeviceCapabilities.h"
#include "core/layout/LayoutObject.h"
#include "core/paint/PaintLayer.h"
#include "core/svg/SVGElement.h"
#include "public/platform/WebPointerProperties.h"

namespace blink {

namespace {

LayoutSize contentsScrollOffset(AbstractView* abstractView) {
  if (!abstractView || !abstractView->isLocalDOMWindow())
    return LayoutSize();
  LocalFrame* frame = toLocalDOMWindow(abstractView)->frame();
  if (!frame)
    return LayoutSize();
  FrameView* frameView = frame->view();
  if (!frameView)
    return LayoutSize();
  float scaleFactor = frame->pageZoomFactor();
  return LayoutSize(frameView->scrollX() / scaleFactor,
                    frameView->scrollY() / scaleFactor);
}

float pageZoomFactor(const UIEvent* event) {
  if (!event->view() || !event->view()->isLocalDOMWindow())
    return 1;
  LocalFrame* frame = toLocalDOMWindow(event->view())->frame();
  if (!frame)
    return 1;
  return frame->pageZoomFactor();
}

const LayoutObject* findTargetLayoutObject(Node*& targetNode) {
  LayoutObject* layoutObject = targetNode->layoutObject();
  if (!layoutObject || !layoutObject->isSVG())
    return layoutObject;
  // If this is an SVG node, compute the offset to the padding box of the
  // outermost SVG root (== the closest ancestor that has a CSS layout box.)
  while (!layoutObject->isSVGRoot())
    layoutObject = layoutObject->parent();
  // Update the target node to point to the SVG root.
  targetNode = layoutObject->node();
  DCHECK(!targetNode || (targetNode->isSVGElement() &&
                         toSVGElement(*targetNode).isOutermostSVGSVGElement()));
  return layoutObject;
}

}  // namespace

MouseEvent* MouseEvent::create(ScriptState* scriptState,
                               const AtomicString& type,
                               const MouseEventInit& initializer) {
  if (scriptState && scriptState->world().isIsolatedWorld())
    UIEventWithKeyState::didCreateEventInIsolatedWorld(
        initializer.ctrlKey(), initializer.altKey(), initializer.shiftKey(),
        initializer.metaKey());
  return new MouseEvent(type, initializer);
}

MouseEvent* MouseEvent::create(const AtomicString& eventType,
                               AbstractView* view,
                               const WebMouseEvent& event,
                               int detail,
                               const String& canvasRegionId,
                               Node* relatedTarget) {
  bool isMouseEnterOrLeave = eventType == EventTypeNames::mouseenter ||
                             eventType == EventTypeNames::mouseleave;
  bool isCancelable = !isMouseEnterOrLeave;
  bool isBubbling = !isMouseEnterOrLeave;
  return new MouseEvent(eventType, isBubbling, isCancelable, view, event,
                        detail, canvasRegionId, relatedTarget);
}

MouseEvent* MouseEvent::create(const AtomicString& eventType,
                               AbstractView* view,
                               Event* underlyingEvent,
                               SimulatedClickCreationScope creationScope) {
  WebInputEvent::Modifiers modifiers = WebInputEvent::NoModifiers;
  if (UIEventWithKeyState* keyStateEvent =
          findEventWithKeyState(underlyingEvent)) {
    modifiers = keyStateEvent->modifiers();
  }

  SyntheticEventType syntheticType = Positionless;
  int screenX = 0;
  int screenY = 0;
  if (underlyingEvent && underlyingEvent->isMouseEvent()) {
    syntheticType = RealOrIndistinguishable;
    MouseEvent* mouseEvent = toMouseEvent(underlyingEvent);
    screenX = mouseEvent->screenX();
    screenY = mouseEvent->screenY();
  }

  TimeTicks timestamp =
      underlyingEvent ? underlyingEvent->platformTimeStamp() : TimeTicks::Now();
  MouseEvent* createdEvent = new MouseEvent(
      eventType, true, true, view, 0, screenX, screenY, 0, 0, 0, 0, modifiers,
      0, 0, nullptr, timestamp, syntheticType, String());

  createdEvent->setTrusted(creationScope ==
                           SimulatedClickCreationScope::FromUserAgent);
  createdEvent->setUnderlyingEvent(underlyingEvent);
  if (syntheticType == RealOrIndistinguishable) {
    MouseEvent* mouseEvent = toMouseEvent(createdEvent->underlyingEvent());
    createdEvent->initCoordinates(mouseEvent->clientX(), mouseEvent->clientY());
  }

  return createdEvent;
}

MouseEvent::MouseEvent()
    : m_positionType(PositionType::Position),
      m_hasCachedRelativePosition(false),
      m_button(0),
      m_buttons(0),
      m_relatedTarget(nullptr),
      m_syntheticEventType(RealOrIndistinguishable) {}

MouseEvent::MouseEvent(const AtomicString& eventType,
                       bool canBubble,
                       bool cancelable,
                       AbstractView* abstractView,
                       const WebMouseEvent& event,
                       int detail,
                       const String& region,
                       EventTarget* relatedTarget)
    : UIEventWithKeyState(
          eventType,
          canBubble,
          cancelable,
          abstractView,
          detail,
          static_cast<WebInputEvent::Modifiers>(event.modifiers()),
          TimeTicks::FromSeconds(event.timeStampSeconds()),
          abstractView
              ? abstractView->getInputDeviceCapabilities()->firesTouchEvents(
                    event.fromTouch())
              : nullptr),
      m_screenLocation(event.globalX, event.globalY),
      m_movementDelta(flooredIntPoint(event.movementInRootFrame())),
      m_positionType(PositionType::Position),
      m_button(static_cast<short>(event.button)),
      m_buttons(webInputEventModifiersToButtons(event.modifiers())),
      m_relatedTarget(relatedTarget),
      m_syntheticEventType(event.fromTouch() ? FromTouch
                                             : RealOrIndistinguishable),
      m_region(region) {
  IntPoint rootFrameCoordinates = flooredIntPoint(event.positionInRootFrame());
  initCoordinatesFromRootFrame(rootFrameCoordinates.x(),
                               rootFrameCoordinates.y());
}

MouseEvent::MouseEvent(const AtomicString& eventType,
                       bool canBubble,
                       bool cancelable,
                       AbstractView* abstractView,
                       int detail,
                       int screenX,
                       int screenY,
                       int windowX,
                       int windowY,
                       int movementX,
                       int movementY,
                       WebInputEvent::Modifiers modifiers,
                       short button,
                       unsigned short buttons,
                       EventTarget* relatedTarget,
                       TimeTicks platformTimeStamp,
                       SyntheticEventType syntheticEventType,
                       const String& region)
    : UIEventWithKeyState(
          eventType,
          canBubble,
          cancelable,
          abstractView,
          detail,
          modifiers,
          platformTimeStamp,
          abstractView
              ? abstractView->getInputDeviceCapabilities()->firesTouchEvents(
                    syntheticEventType == FromTouch)
              : nullptr),
      m_screenLocation(screenX, screenY),
      m_movementDelta(movementX, movementY),
      m_positionType(syntheticEventType == Positionless
                         ? PositionType::Positionless
                         : PositionType::Position),
      m_button(button),
      m_buttons(buttons),
      m_relatedTarget(relatedTarget),
      m_syntheticEventType(syntheticEventType),
      m_region(region) {
  initCoordinatesFromRootFrame(windowX, windowY);
}

MouseEvent::MouseEvent(const AtomicString& eventType,
                       const MouseEventInit& initializer)
    : UIEventWithKeyState(eventType, initializer),
      m_screenLocation(
          DoublePoint(initializer.screenX(), initializer.screenY())),
      m_movementDelta(
          IntPoint(initializer.movementX(), initializer.movementY())),
      m_positionType(PositionType::Position),
      m_button(initializer.button()),
      m_buttons(initializer.buttons()),
      m_relatedTarget(initializer.relatedTarget()),
      m_syntheticEventType(RealOrIndistinguishable),
      m_region(initializer.region()) {
  initCoordinates(initializer.clientX(), initializer.clientY());
}

void MouseEvent::initCoordinates(const double clientX, const double clientY) {
  // Set up initial values for coordinates.
  // Correct values are computed lazily, see computeRelativePosition.
  m_clientLocation = DoublePoint(clientX, clientY);
  m_pageLocation = m_clientLocation + DoubleSize(contentsScrollOffset(view()));

  m_layerLocation = m_pageLocation;
  m_offsetLocation = m_pageLocation;

  computePageLocation();
  m_hasCachedRelativePosition = false;
}

void MouseEvent::initCoordinatesFromRootFrame(int windowX, int windowY) {
  DoublePoint adjustedPageLocation;
  DoubleSize scrollOffset;

  LocalFrame* frame = view() && view()->isLocalDOMWindow()
                          ? toLocalDOMWindow(view())->frame()
                          : nullptr;
  if (frame && hasPosition()) {
    if (FrameView* frameView = frame->view()) {
      adjustedPageLocation =
          frameView->rootFrameToContents(IntPoint(windowX, windowY));
      scrollOffset = frameView->scrollOffsetInt();
      float scaleFactor = 1 / frame->pageZoomFactor();
      if (scaleFactor != 1.0f) {
        adjustedPageLocation.scale(scaleFactor, scaleFactor);
        scrollOffset.scale(scaleFactor, scaleFactor);
      }
    }
  }

  m_clientLocation = adjustedPageLocation - scrollOffset;
  m_pageLocation = adjustedPageLocation;

  // Set up initial values for coordinates.
  // Correct values are computed lazily, see computeRelativePosition.
  m_layerLocation = m_pageLocation;
  m_offsetLocation = m_pageLocation;

  computePageLocation();
  m_hasCachedRelativePosition = false;
}

MouseEvent::~MouseEvent() {}

unsigned short MouseEvent::webInputEventModifiersToButtons(unsigned modifiers) {
  unsigned short buttons = 0;

  if (modifiers & WebInputEvent::LeftButtonDown)
    buttons |= static_cast<unsigned short>(WebPointerProperties::Buttons::Left);
  if (modifiers & WebInputEvent::RightButtonDown)
    buttons |=
        static_cast<unsigned short>(WebPointerProperties::Buttons::Right);
  if (modifiers & WebInputEvent::MiddleButtonDown)
    buttons |=
        static_cast<unsigned short>(WebPointerProperties::Buttons::Middle);

  return buttons;
}

void MouseEvent::initMouseEvent(ScriptState* scriptState,
                                const AtomicString& type,
                                bool canBubble,
                                bool cancelable,
                                AbstractView* view,
                                int detail,
                                int screenX,
                                int screenY,
                                int clientX,
                                int clientY,
                                bool ctrlKey,
                                bool altKey,
                                bool shiftKey,
                                bool metaKey,
                                short button,
                                EventTarget* relatedTarget,
                                unsigned short buttons) {
  if (isBeingDispatched())
    return;

  if (scriptState && scriptState->world().isIsolatedWorld())
    UIEventWithKeyState::didCreateEventInIsolatedWorld(ctrlKey, altKey,
                                                       shiftKey, metaKey);

  initModifiers(ctrlKey, altKey, shiftKey, metaKey);
  initMouseEventInternal(type, canBubble, cancelable, view, detail, screenX,
                         screenY, clientX, clientY, modifiers(), button,
                         relatedTarget, nullptr, buttons);
}

void MouseEvent::initMouseEventInternal(
    const AtomicString& type,
    bool canBubble,
    bool cancelable,
    AbstractView* view,
    int detail,
    int screenX,
    int screenY,
    int clientX,
    int clientY,
    WebInputEvent::Modifiers modifiers,
    short button,
    EventTarget* relatedTarget,
    InputDeviceCapabilities* sourceCapabilities,
    unsigned short buttons) {
  initUIEventInternal(type, canBubble, cancelable, relatedTarget, view, detail,
                      sourceCapabilities);

  m_screenLocation = IntPoint(screenX, screenY);
  m_button = button;
  m_buttons = buttons;
  m_relatedTarget = relatedTarget;
  m_modifiers = modifiers;

  initCoordinates(clientX, clientY);

  // FIXME: SyntheticEventType is not set to RealOrIndistinguishable here.
}

const AtomicString& MouseEvent::interfaceName() const {
  return EventNames::MouseEvent;
}

bool MouseEvent::isMouseEvent() const {
  return true;
}

int MouseEvent::which() const {
  // For the DOM, the return values for left, middle and right mouse buttons are
  // 0, 1, 2, respectively.
  // For the Netscape "which" property, the return values for left, middle and
  // right mouse buttons are 1, 2, 3, respectively.
  // So we must add 1.
  return m_button + 1;
}

Node* MouseEvent::toElement() const {
  // MSIE extension - "the object toward which the user is moving the mouse
  // pointer"
  if (type() == EventTypeNames::mouseout ||
      type() == EventTypeNames::mouseleave)
    return relatedTarget() ? relatedTarget()->toNode() : nullptr;

  return target() ? target()->toNode() : nullptr;
}

Node* MouseEvent::fromElement() const {
  // MSIE extension - "object from which activation or the mouse pointer is
  // exiting during the event" (huh?)
  if (type() != EventTypeNames::mouseout &&
      type() != EventTypeNames::mouseleave)
    return relatedTarget() ? relatedTarget()->toNode() : nullptr;

  return target() ? target()->toNode() : nullptr;
}

DEFINE_TRACE(MouseEvent) {
  visitor->trace(m_relatedTarget);
  UIEventWithKeyState::trace(visitor);
}

EventDispatchMediator* MouseEvent::createMediator() {
  return MouseEventDispatchMediator::create(this);
}

MouseEventDispatchMediator* MouseEventDispatchMediator::create(
    MouseEvent* mouseEvent) {
  return new MouseEventDispatchMediator(mouseEvent);
}

MouseEventDispatchMediator::MouseEventDispatchMediator(MouseEvent* mouseEvent)
    : EventDispatchMediator(mouseEvent) {}

MouseEvent& MouseEventDispatchMediator::event() const {
  return toMouseEvent(EventDispatchMediator::event());
}

DispatchEventResult MouseEventDispatchMediator::dispatchEvent(
    EventDispatcher& dispatcher) const {
  MouseEvent& mouseEvent = event();
  mouseEvent.eventPath().adjustForRelatedTarget(dispatcher.node(),
                                                mouseEvent.relatedTarget());

  if (!mouseEvent.isTrusted())
    return dispatcher.dispatch();

  if (isDisabledFormControl(&dispatcher.node()))
    return DispatchEventResult::CanceledBeforeDispatch;

  if (mouseEvent.type().isEmpty())
    return DispatchEventResult::NotCanceled;  // Shouldn't happen.

  DCHECK(!mouseEvent.target() ||
         mouseEvent.target() != mouseEvent.relatedTarget());

  EventTarget* relatedTarget = mouseEvent.relatedTarget();

  DispatchEventResult dispatchResult = dispatcher.dispatch();

  if (mouseEvent.type() != EventTypeNames::click || mouseEvent.detail() != 2)
    return dispatchResult;

  // Special case: If it's a double click event, we also send the dblclick
  // event. This is not part of the DOM specs, but is used for compatibility
  // with the ondblclick="" attribute. This is treated as a separate event in
  // other DOM-compliant browsers like Firefox, and so we do the same.
  MouseEvent* doubleClickEvent = MouseEvent::create();
  doubleClickEvent->initMouseEventInternal(
      EventTypeNames::dblclick, mouseEvent.bubbles(), mouseEvent.cancelable(),
      mouseEvent.view(), mouseEvent.detail(), mouseEvent.screenX(),
      mouseEvent.screenY(), mouseEvent.clientX(), mouseEvent.clientY(),
      mouseEvent.modifiers(), mouseEvent.button(), relatedTarget,
      mouseEvent.sourceCapabilities(), mouseEvent.buttons());
  doubleClickEvent->setComposed(mouseEvent.composed());

  // Inherit the trusted status from the original event.
  doubleClickEvent->setTrusted(mouseEvent.isTrusted());
  if (mouseEvent.defaultHandled())
    doubleClickEvent->setDefaultHandled();
  DispatchEventResult doubleClickDispatchResult =
      EventDispatcher::dispatchEvent(
          dispatcher.node(),
          MouseEventDispatchMediator::create(doubleClickEvent));
  if (doubleClickDispatchResult != DispatchEventResult::NotCanceled)
    return doubleClickDispatchResult;
  return dispatchResult;
}

void MouseEvent::computePageLocation() {
  float scaleFactor = pageZoomFactor(this);
  m_absoluteLocation = m_pageLocation.scaledBy(scaleFactor);
}

void MouseEvent::receivedTarget() {
  m_hasCachedRelativePosition = false;
}

void MouseEvent::computeRelativePosition() {
  Node* targetNode = target() ? target()->toNode() : nullptr;
  if (!targetNode)
    return;

  // Compute coordinates that are based on the target.
  m_layerLocation = m_pageLocation;
  m_offsetLocation = m_pageLocation;

  // Must have an updated layout tree for this math to work correctly.
  targetNode->document().updateStyleAndLayoutIgnorePendingStylesheets();

  // Adjust offsetLocation to be relative to the target's padding box.
  if (const LayoutObject* layoutObject = findTargetLayoutObject(targetNode)) {
    FloatPoint localPos = layoutObject->absoluteToLocal(
        FloatPoint(absoluteLocation()), UseTransforms);

    // Adding this here to address crbug.com/570666. Basically we'd like to
    // find the local coordinates relative to the padding box not the border
    // box.
    if (layoutObject->isBoxModelObject()) {
      const LayoutBoxModelObject* layoutBox =
          toLayoutBoxModelObject(layoutObject);
      localPos.move(-layoutBox->borderLeft(), -layoutBox->borderTop());
    }

    m_offsetLocation = DoublePoint(localPos);
    float scaleFactor = 1 / pageZoomFactor(this);
    if (scaleFactor != 1.0f)
      m_offsetLocation.scale(scaleFactor, scaleFactor);
  }

  // Adjust layerLocation to be relative to the layer.
  // FIXME: event.layerX and event.layerY are poorly defined,
  // and probably don't always correspond to PaintLayer offsets.
  // https://bugs.webkit.org/show_bug.cgi?id=21868
  Node* n = targetNode;
  while (n && !n->layoutObject())
    n = n->parentNode();

  if (n) {
    // FIXME: This logic is a wrong implementation of convertToLayerCoords.
    for (PaintLayer* layer = n->layoutObject()->enclosingLayer(); layer;
         layer = layer->parent()) {
      m_layerLocation -= DoubleSize(layer->location().x().toDouble(),
                                    layer->location().y().toDouble());
    }
  }

  m_hasCachedRelativePosition = true;
}

int MouseEvent::layerX() {
  if (!m_hasCachedRelativePosition)
    computeRelativePosition();

  // TODO(mustaq): Remove the PointerEvent specific code when mouse has
  // fractional coordinates. See crbug.com/655786.
  return isPointerEvent() ? m_layerLocation.x()
                          : static_cast<int>(m_layerLocation.x());
}

int MouseEvent::layerY() {
  if (!m_hasCachedRelativePosition)
    computeRelativePosition();

  // TODO(mustaq): Remove the PointerEvent specific code when mouse has
  // fractional coordinates. See crbug.com/655786.
  return isPointerEvent() ? m_layerLocation.y()
                          : static_cast<int>(m_layerLocation.y());
}

int MouseEvent::offsetX() {
  if (!hasPosition())
    return 0;
  if (!m_hasCachedRelativePosition)
    computeRelativePosition();
  return std::round(m_offsetLocation.x());
}

int MouseEvent::offsetY() {
  if (!hasPosition())
    return 0;
  if (!m_hasCachedRelativePosition)
    computeRelativePosition();
  return std::round(m_offsetLocation.y());
}

}  // namespace blink
