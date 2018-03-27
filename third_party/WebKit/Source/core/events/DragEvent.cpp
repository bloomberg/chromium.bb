// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/DragEvent.h"

#include "core/clipboard/DataTransfer.h"
#include "core/dom/Element.h"
#include "core/dom/events/EventDispatcher.h"

namespace blink {

DragEvent::DragEvent() : data_transfer_(nullptr) {}

DragEvent::DragEvent(const AtomicString& type,
                     const DragEventInit& initializer,
                     TimeTicks platform_time_stamp,
                     SyntheticEventType synthetic_event_type)
    : MouseEvent(type, initializer, platform_time_stamp, synthetic_event_type),
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
