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
#include "core/dom/events/EventDispatchMediator.h"
#include "core/events/MouseEventInit.h"
#include "core/events/UIEventWithKeyState.h"
#include "public/platform/WebMenuSourceType.h"
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
    kRealOrIndistinguishable,
    // Synthetic mouse events derived from touch input
    kFromTouch,
    // Synthetic mouse events generated without a position, for example those
    // generated from keyboard input.
    kPositionless,
  };

  static MouseEvent* Create() { return new MouseEvent; }

  static MouseEvent* Create(const AtomicString& event_type,
                            AbstractView*,
                            const WebMouseEvent&,
                            int detail,
                            const String& canvas_region_id,
                            Node* related_target);

  static MouseEvent* Create(ScriptState*,
                            const AtomicString& event_type,
                            const MouseEventInit&);

  static MouseEvent* Create(const AtomicString& event_type,
                            AbstractView*,
                            Event* underlying_event,
                            SimulatedClickCreationScope);

  ~MouseEvent() override;

  static unsigned short WebInputEventModifiersToButtons(unsigned modifiers);

  void initMouseEvent(ScriptState*,
                      const AtomicString& type,
                      bool can_bubble,
                      bool cancelable,
                      AbstractView*,
                      int detail,
                      int screen_x,
                      int screen_y,
                      int client_x,
                      int client_y,
                      bool ctrl_key,
                      bool alt_key,
                      bool shift_key,
                      bool meta_key,
                      short button,
                      EventTarget* related_target,
                      unsigned short buttons = 0);

  // WinIE uses 1,4,2 for left/middle/right but not for click (just for
  // mousedown/up, maybe others), but we will match the standard DOM.
  virtual short button() const;
  unsigned short buttons() const { return buttons_; }
  bool ButtonDown() const { return button_ != -1; }
  EventTarget* relatedTarget() const { return related_target_.Get(); }
  void SetRelatedTarget(EventTarget* related_target) {
    related_target_ = related_target;
  }
  SyntheticEventType GetSyntheticEventType() const {
    return synthetic_event_type_;
  }
  const String& region() const { return region_; }

  Node* toElement() const;
  Node* fromElement() const;

  virtual DataTransfer* getDataTransfer() const { return nullptr; }

  bool FromTouch() const { return synthetic_event_type_ == kFromTouch; }

  const AtomicString& InterfaceName() const override;

  bool IsMouseEvent() const override;
  unsigned which() const override;

  EventDispatchMediator* CreateMediator() override;

  int ClickCount() { return detail(); }

  const WebMouseEvent* NativeEvent() const { return native_event_.get(); }

  enum class PositionType {
    kPosition,
    // Positionless mouse events are used, for example, for 'click' events from
    // keyboard input.  It's kind of surprising for a mouse event not to have a
    // position.
    kPositionless
  };

  // Note that these values are adjusted to counter the effects of zoom, so that
  // values exposed via DOM APIs are invariant under zooming.
  // TODO(mustaq): Remove the PointerEvent specific code when mouse has
  // fractional coordinates. See crbug.com/655786.
  double screenX() const {
    return IsPointerEvent() ? screen_location_.X()
                            : static_cast<int>(screen_location_.X());
  }
  double screenY() const {
    return IsPointerEvent() ? screen_location_.Y()
                            : static_cast<int>(screen_location_.Y());
  }

  double clientX() const {
    return IsPointerEvent() ? client_location_.X()
                            : static_cast<int>(client_location_.X());
  }
  double clientY() const {
    return IsPointerEvent() ? client_location_.Y()
                            : static_cast<int>(client_location_.Y());
  }

  int movementX() const { return movement_delta_.X(); }
  int movementY() const { return movement_delta_.Y(); }

  int layerX();
  int layerY();

  int offsetX();
  int offsetY();

  double pageX() const {
    return IsPointerEvent() ? page_location_.X()
                            : static_cast<int>(page_location_.X());
  }
  double pageY() const {
    return IsPointerEvent() ? page_location_.Y()
                            : static_cast<int>(page_location_.Y());
  }

  double x() const { return clientX(); }
  double y() const { return clientY(); }

  bool HasPosition() const { return position_type_ == PositionType::kPosition; }

  WebMenuSourceType GetMenuSourceType() const { return menu_source_type_; }

  // Page point in "absolute" coordinates (i.e. post-zoomed, page-relative
  // coords, usable with LayoutObject::absoluteToLocal) relative to view(), i.e.
  // the local frame.
  const DoublePoint& AbsoluteLocation() const { return absolute_location_; }

  virtual void Trace(blink::Visitor*);

 protected:
  MouseEvent(const AtomicString& type,
             bool can_bubble,
             bool cancelable,
             AbstractView*,
             const WebMouseEvent&,
             int detail,
             const String& region,
             EventTarget* related_target);

  MouseEvent(const AtomicString& type,
             bool can_bubble,
             bool cancelable,
             AbstractView*,
             int detail,
             int screen_x,
             int screen_y,
             int window_x,
             int window_y,
             int movement_x,
             int movement_y,
             WebInputEvent::Modifiers,
             short button,
             unsigned short buttons,
             EventTarget* related_target,
             TimeTicks platform_time_stamp,
             SyntheticEventType,
             const String& region);

  MouseEvent(const AtomicString& type,
             const MouseEventInit&,
             TimeTicks platform_time_stamp);
  MouseEvent(const AtomicString& type, const MouseEventInit& init)
      : MouseEvent(type, init, TimeTicks::Now()) {}

  MouseEvent();

  short RawButton() const { return button_; }

  void ReceivedTarget() override;

 private:
  friend class MouseEventDispatchMediator;
  void InitMouseEventInternal(const AtomicString& type,
                              bool can_bubble,
                              bool cancelable,
                              AbstractView*,
                              int detail,
                              int screen_x,
                              int screen_y,
                              int client_x,
                              int client_y,
                              WebInputEvent::Modifiers,
                              short button,
                              EventTarget* related_target,
                              InputDeviceCapabilities* source_capabilities,
                              unsigned short buttons = 0);

  void InitCoordinates(const double client_x, const double client_y);
  void InitCoordinatesFromRootFrame(int window_x, int window_y);

  void ComputePageLocation();
  void ComputeRelativePosition();

  DoublePoint screen_location_;
  DoublePoint client_location_;
  DoublePoint movement_delta_;

  DoublePoint page_location_;
  DoublePoint layer_location_;
  DoublePoint offset_location_;
  DoublePoint absolute_location_;
  PositionType position_type_;
  bool has_cached_relative_position_;
  short button_;
  unsigned short buttons_;
  Member<EventTarget> related_target_;
  SyntheticEventType synthetic_event_type_;
  String region_;

  // Only used for contextmenu events.
  WebMenuSourceType menu_source_type_;

  std::unique_ptr<WebMouseEvent> native_event_;
};

class MouseEventDispatchMediator final : public EventDispatchMediator {
 public:
  static MouseEventDispatchMediator* Create(MouseEvent*);

 private:
  explicit MouseEventDispatchMediator(MouseEvent*);
  MouseEvent& Event() const;

  DispatchEventResult DispatchEvent(EventDispatcher&) const override;
};

DEFINE_EVENT_TYPE_CASTS(MouseEvent);

}  // namespace blink

#endif  // MouseEvent_h
