// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PointerEvent_h
#define PointerEvent_h

#include "core/events/MouseEvent.h"
#include "core/events/PointerEventInit.h"

namespace blink {

class CORE_EXPORT PointerEvent final : public MouseEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PointerEvent* Create(const AtomicString& type,
                              const PointerEventInit& initializer,
                              TimeTicks platform_time_stamp) {
    return new PointerEvent(type, initializer, platform_time_stamp);
  }
  static PointerEvent* Create(const AtomicString& type,
                              const PointerEventInit& initializer) {
    return PointerEvent::Create(type, initializer, TimeTicks::Now());
  }

  int pointerId() const { return pointer_id_; }
  double width() const { return width_; }
  double height() const { return height_; }
  float pressure() const { return pressure_; }
  long tiltX() const { return tilt_x_; }
  long tiltY() const { return tilt_y_; }
  float tangentialPressure() const { return tangential_pressure_; }
  long twist() const { return twist_; }
  const String& pointerType() const { return pointer_type_; }
  bool isPrimary() const { return is_primary_; }

  short button() const override { return RawButton(); }
  bool IsMouseEvent() const override;
  bool IsPointerEvent() const override;

  // TODO(eirage): crbug.com/773813 Make PointerEvents of type mouse have
  // fractional coordiantes
  double screenX() const override {
    return (pointer_type_ == "mouse") ? static_cast<int>(screen_location_.X())
                                      : screen_location_.X();
  }
  double screenY() const override {
    return (pointer_type_ == "mouse") ? static_cast<int>(screen_location_.Y())
                                      : screen_location_.Y();
  }

  double clientX() const override {
    return (pointer_type_ == "mouse") ? static_cast<int>(client_location_.X())
                                      : client_location_.X();
  }
  double clientY() const override {
    return (pointer_type_ == "mouse") ? static_cast<int>(client_location_.Y())
                                      : client_location_.Y();
  }

  double pageX() const override {
    return (pointer_type_ == "mouse") ? static_cast<int>(page_location_.X())
                                      : page_location_.X();
  }
  double pageY() const override {
    return (pointer_type_ == "mouse") ? static_cast<int>(page_location_.Y())
                                      : page_location_.Y();
  }

  EventDispatchMediator* CreateMediator() override;
  void ReceivedTarget() override;

  HeapVector<Member<PointerEvent>> getCoalescedEvents();

  virtual void Trace(blink::Visitor*);

 private:
  PointerEvent(const AtomicString&,
               const PointerEventInit&,
               TimeTicks platform_time_stamp);

  int pointer_id_;
  double width_;
  double height_;
  float pressure_;
  long tilt_x_;
  long tilt_y_;
  float tangential_pressure_;
  long twist_;
  String pointer_type_;
  bool is_primary_;

  bool coalesced_events_targets_dirty_;

  HeapVector<Member<PointerEvent>> coalesced_events_;
};

class PointerEventDispatchMediator final : public EventDispatchMediator {
 public:
  static PointerEventDispatchMediator* Create(PointerEvent*);

 private:
  explicit PointerEventDispatchMediator(PointerEvent*);
  PointerEvent& Event() const;
  DispatchEventResult DispatchEvent(EventDispatcher&) const override;
};

DEFINE_EVENT_TYPE_CASTS(PointerEvent);

}  // namespace blink

#endif  // PointerEvent_h
