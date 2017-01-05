/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#ifndef WheelEvent_h
#define WheelEvent_h

#include "core/CoreExport.h"
#include "core/events/MouseEvent.h"
#include "core/events/WheelEventInit.h"
#include "platform/geometry/FloatPoint.h"
#include "public/platform/WebMouseWheelEvent.h"

namespace blink {

class CORE_EXPORT WheelEvent final : public MouseEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum { TickMultiplier = 120 };

  enum DeltaMode { kDomDeltaPixel = 0, kDomDeltaLine, kDomDeltaPage };

  static WheelEvent* create() { return new WheelEvent; }

  static WheelEvent* create(const WebMouseWheelEvent& nativeEvent,
                            AbstractView*);

  static WheelEvent* create(const AtomicString& type,
                            const WheelEventInit& initializer) {
    return new WheelEvent(type, initializer);
  }

  double deltaX() const { return m_deltaX; }  // Positive when scrolling right.
  double deltaY() const { return m_deltaY; }  // Positive when scrolling down.
  double deltaZ() const { return m_deltaZ; }
  int wheelDelta() const {
    return wheelDeltaY() ? wheelDeltaY() : wheelDeltaX();
  }  // Deprecated.
  int wheelDeltaX() const {
    return m_wheelDelta.x();
  }  // Deprecated, negative when scrolling right.
  int wheelDeltaY() const {
    return m_wheelDelta.y();
  }  // Deprecated, negative when scrolling down.
  unsigned deltaMode() const { return m_deltaMode; }

  const AtomicString& interfaceName() const override;
  bool isMouseEvent() const override;
  bool isWheelEvent() const override;

  EventDispatchMediator* createMediator() override;

  const WebMouseWheelEvent& nativeEvent() const { return m_nativeEvent; }

  DECLARE_VIRTUAL_TRACE();

 private:
  WheelEvent();
  WheelEvent(const AtomicString&, const WheelEventInit&);
  WheelEvent(const WebMouseWheelEvent&, AbstractView*);

  IntPoint m_wheelDelta;
  double m_deltaX;
  double m_deltaY;
  double m_deltaZ;
  unsigned m_deltaMode;
  WebMouseWheelEvent m_nativeEvent;
};

DEFINE_EVENT_TYPE_CASTS(WheelEvent);

}  // namespace blink

#endif  // WheelEvent_h
