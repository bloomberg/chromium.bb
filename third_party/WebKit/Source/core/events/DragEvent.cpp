// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/DragEvent.h"

#include "core/clipboard/DataTransfer.h"
#include "core/dom/Element.h"
#include "core/dom/events/EventDispatcher.h"

namespace blink {

DragEvent* DragEvent::Create(const AtomicString& type,
                             bool can_bubble,
                             bool cancelable,
                             AbstractView* view,
                             int detail,
                             double screen_x,
                             double screen_y,
                             double window_x,
                             double window_y,
                             double movement_x,
                             double movement_y,
                             WebInputEvent::Modifiers modifiers,
                             short button,
                             unsigned short buttons,
                             EventTarget* related_target,
                             TimeTicks platform_time_stamp,
                             DataTransfer* data_transfer,
                             SyntheticEventType synthetic_event_type) {
  return new DragEvent(
      type, can_bubble, cancelable, view, detail, screen_x, screen_y, window_x,
      window_y, movement_x, movement_y, modifiers, button, buttons,
      related_target, platform_time_stamp, data_transfer, synthetic_event_type);
}

DragEvent::DragEvent() : data_transfer_(nullptr) {}

DragEvent::DragEvent(DataTransfer* data_transfer)
    : data_transfer_(data_transfer) {}

DragEvent::DragEvent(const AtomicString& event_type,
                     bool can_bubble,
                     bool cancelable,
                     AbstractView* view,
                     int detail,
                     double screen_x,
                     double screen_y,
                     double window_x,
                     double window_y,
                     double movement_x,
                     double movement_y,
                     WebInputEvent::Modifiers modifiers,
                     short button,
                     unsigned short buttons,
                     EventTarget* related_target,
                     TimeTicks platform_time_stamp,
                     DataTransfer* data_transfer,
                     SyntheticEventType synthetic_event_type)
    : MouseEvent(
          event_type,
          can_bubble,
          cancelable,
          view,
          detail,
          screen_x,
          screen_y,
          window_x,
          window_y,
          movement_x,
          movement_y,
          modifiers,
          button,
          buttons,
          related_target,
          platform_time_stamp,
          synthetic_event_type,
          // TODO(zino): Should support canvas hit region because the drag event
          // is a kind of mouse event. Please see http://crbug.com/594073
          String()),
      data_transfer_(data_transfer)

{}

DragEvent::DragEvent(const AtomicString& type, const DragEventInit& initializer)
    : MouseEvent(type, initializer),
      data_transfer_(initializer.getDataTransfer()) {}

bool DragEvent::IsDragEvent() const {
  return true;
}

bool DragEvent::IsMouseEvent() const {
  return false;
}

void DragEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(data_transfer_);
  MouseEvent::Trace(visitor);
}

DispatchEventResult DragEvent::DispatchEvent(EventDispatcher& dispatcher) {
  GetEventPath().AdjustForRelatedTarget(dispatcher.GetNode(), relatedTarget());
  return dispatcher.Dispatch();
}

}  // namespace blink
