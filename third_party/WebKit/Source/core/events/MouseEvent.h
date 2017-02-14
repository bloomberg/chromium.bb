/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 *
 */

#ifndef MouseEvent_h
#define MouseEvent_h

#include "core/CoreExport.h"
#include "core/events/EventDispatchMediator.h"
#include "core/events/MouseEventInit.h"
#include "core/events/UIEventWithKeyState.h"
#include "public/platform/WebMouseEvent.h"

namespace blink {
class DataTransfer;
class EventDispatcher;

class CORE_EXPORT MouseEvent : public UIEventWithKeyState {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum SyntheticEventType {
    // Real mouse input events or synthetic events that behave just like real
    // events
    RealOrIndistinguishable,
    // Synthetic mouse events derived from touch input
    FromTouch,
    // Synthetic mouse events generated without a position, for example those
    // generated from keyboard input.
    Positionless,
  };

  static MouseEvent* create() { return new MouseEvent; }

  static MouseEvent* create(const AtomicString& eventType,
                            AbstractView*,
                            const WebMouseEvent&,
                            int detail,
                            const String& canvasRegionId,
                            Node* relatedTarget);

  static MouseEvent* create(ScriptState*,
                            const AtomicString& eventType,
                            const MouseEventInit&);

  static MouseEvent* create(const AtomicString& eventType,
                            AbstractView*,
                            Event* underlyingEvent,
                            SimulatedClickCreationScope);

  ~MouseEvent() override;

  static unsigned short webInputEventModifiersToButtons(unsigned modifiers);

  void initMouseEvent(ScriptState*,
                      const AtomicString& type,
                      bool canBubble,
                      bool cancelable,
                      AbstractView*,
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
                      unsigned short buttons = 0);

  // WinIE uses 1,4,2 for left/middle/right but not for click (just for
  // mousedown/up, maybe others), but we will match the standard DOM.
  virtual short button() const { return m_button == -1 ? 0 : m_button; }
  unsigned short buttons() const { return m_buttons; }
  bool buttonDown() const { return m_button != -1; }
  EventTarget* relatedTarget() const { return m_relatedTarget.get(); }
  void setRelatedTarget(EventTarget* relatedTarget) {
    m_relatedTarget = relatedTarget;
  }
  SyntheticEventType getSyntheticEventType() const {
    return m_syntheticEventType;
  }
  const String& region() const { return m_region; }

  Node* toElement() const;
  Node* fromElement() const;

  virtual DataTransfer* getDataTransfer() const { return nullptr; }

  bool fromTouch() const { return m_syntheticEventType == FromTouch; }

  const AtomicString& interfaceName() const override;

  bool isMouseEvent() const override;
  int which() const final;

  EventDispatchMediator* createMediator() override;

  int clickCount() { return detail(); }

  const WebMouseEvent* nativeEvent() const { return m_nativeEvent.get(); }

  enum class PositionType {
    Position,
    // Positionless mouse events are used, for example, for 'click' events from
    // keyboard input.  It's kind of surprising for a mouse event not to have a
    // position.
    Positionless
  };

  // Note that these values are adjusted to counter the effects of zoom, so that
  // values exposed via DOM APIs are invariant under zooming.
  // TODO(mustaq): Remove the PointerEvent specific code when mouse has
  // fractional coordinates. See crbug.com/655786.
  double screenX() const {
    return isPointerEvent() ? m_screenLocation.x()
                            : static_cast<int>(m_screenLocation.x());
  }
  double screenY() const {
    return isPointerEvent() ? m_screenLocation.y()
                            : static_cast<int>(m_screenLocation.y());
  }

  double clientX() const {
    return isPointerEvent() ? m_clientLocation.x()
                            : static_cast<int>(m_clientLocation.x());
  }
  double clientY() const {
    return isPointerEvent() ? m_clientLocation.y()
                            : static_cast<int>(m_clientLocation.y());
  }

  int movementX() const { return m_movementDelta.x(); }
  int movementY() const { return m_movementDelta.y(); }

  int layerX();
  int layerY();

  int offsetX();
  int offsetY();

  double pageX() const {
    return isPointerEvent() ? m_pageLocation.x()
                            : static_cast<int>(m_pageLocation.x());
  }
  double pageY() const {
    return isPointerEvent() ? m_pageLocation.y()
                            : static_cast<int>(m_pageLocation.y());
  }

  double x() const { return clientX(); }
  double y() const { return clientY(); }

  bool hasPosition() const { return m_positionType == PositionType::Position; }

  // Page point in "absolute" coordinates (i.e. post-zoomed, page-relative
  // coords, usable with LayoutObject::absoluteToLocal) relative to view(), i.e.
  // the local frame.
  const DoublePoint& absoluteLocation() const { return m_absoluteLocation; }

  DECLARE_VIRTUAL_TRACE();

 protected:
  MouseEvent(const AtomicString& type,
             bool canBubble,
             bool cancelable,
             AbstractView*,
             const WebMouseEvent&,
             int detail,
             const String& region,
             EventTarget* relatedTarget);

  MouseEvent(const AtomicString& type,
             bool canBubble,
             bool cancelable,
             AbstractView*,
             int detail,
             int screenX,
             int screenY,
             int windowX,
             int windowY,
             int movementX,
             int movementY,
             WebInputEvent::Modifiers,
             short button,
             unsigned short buttons,
             EventTarget* relatedTarget,
             TimeTicks platformTimeStamp,
             SyntheticEventType,
             const String& region);

  MouseEvent(const AtomicString& type, const MouseEventInit&);

  MouseEvent();

  short rawButton() const { return m_button; }

 private:
  friend class MouseEventDispatchMediator;
  void initMouseEventInternal(const AtomicString& type,
                              bool canBubble,
                              bool cancelable,
                              AbstractView*,
                              int detail,
                              int screenX,
                              int screenY,
                              int clientX,
                              int clientY,
                              WebInputEvent::Modifiers,
                              short button,
                              EventTarget* relatedTarget,
                              InputDeviceCapabilities* sourceCapabilities,
                              unsigned short buttons = 0);

  void initCoordinates(const double clientX, const double clientY);
  void initCoordinatesFromRootFrame(int windowX, int windowY);
  void receivedTarget() final;

  void computePageLocation();
  void computeRelativePosition();

  DoublePoint m_screenLocation;
  DoublePoint m_clientLocation;
  DoublePoint m_movementDelta;

  DoublePoint m_pageLocation;
  DoublePoint m_layerLocation;
  DoublePoint m_offsetLocation;
  DoublePoint m_absoluteLocation;
  PositionType m_positionType;
  bool m_hasCachedRelativePosition;
  short m_button;
  unsigned short m_buttons;
  Member<EventTarget> m_relatedTarget;
  SyntheticEventType m_syntheticEventType;
  String m_region;
  std::unique_ptr<WebMouseEvent> m_nativeEvent;
};

class MouseEventDispatchMediator final : public EventDispatchMediator {
 public:
  static MouseEventDispatchMediator* create(MouseEvent*);

 private:
  explicit MouseEventDispatchMediator(MouseEvent*);
  MouseEvent& event() const;

  DispatchEventResult dispatchEvent(EventDispatcher&) const override;
};

DEFINE_EVENT_TYPE_CASTS(MouseEvent);

}  // namespace blink

#endif  // MouseEvent_h
