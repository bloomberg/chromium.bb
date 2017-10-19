// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DragEvent_h
#define DragEvent_h

#include "core/CoreExport.h"
#include "core/events/DragEventInit.h"
#include "core/events/MouseEvent.h"

namespace blink {

class DataTransfer;

class CORE_EXPORT DragEvent final : public MouseEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DragEvent* Create() { return new DragEvent; }

  static DragEvent* Create(DataTransfer* data_transfer) {
    return new DragEvent(data_transfer);
  }

  static DragEvent* Create(const AtomicString& type,
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
                           DataTransfer*,
                           SyntheticEventType = kRealOrIndistinguishable);

  static DragEvent* Create(const AtomicString& type,
                           const DragEventInit& initializer) {
    return new DragEvent(type, initializer);
  }

  DataTransfer* getDataTransfer() const override {
    return IsDragEvent() ? data_transfer_.Get() : nullptr;
  }

  bool IsDragEvent() const override;
  bool IsMouseEvent() const override;

  EventDispatchMediator* CreateMediator() override;

  virtual void Trace(blink::Visitor*);

 private:
  DragEvent();
  DragEvent(DataTransfer*);
  DragEvent(const AtomicString& type,
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
            DataTransfer*,
            SyntheticEventType);

  DragEvent(const AtomicString& type, const DragEventInit&);

  Member<DataTransfer> data_transfer_;
};

class DragEventDispatchMediator final : public EventDispatchMediator {
 public:
  static DragEventDispatchMediator* Create(DragEvent*);

 private:
  explicit DragEventDispatchMediator(DragEvent*);
  DragEvent& Event() const;
  DispatchEventResult DispatchEvent(EventDispatcher&) const override;
};

DEFINE_EVENT_TYPE_CASTS(DragEvent);

}  // namespace blink

#endif  // DragEvent_h
