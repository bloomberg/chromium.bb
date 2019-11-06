// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_input_report_event.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/hid/hid_input_report_event_init.h"

namespace blink {

HIDInputReportEvent* HIDInputReportEvent::Create(
    const AtomicString& type,
    const HIDInputReportEventInit* initializer) {
  return MakeGarbageCollected<HIDInputReportEvent>(type, initializer);
}

HIDInputReportEvent* HIDInputReportEvent::Create(const AtomicString& type,
                                                 HIDDevice* device,
                                                 uint8_t report_id,
                                                 const Vector<uint8_t>& data) {
  return MakeGarbageCollected<HIDInputReportEvent>(type, device, report_id,
                                                   data);
}

HIDInputReportEvent::HIDInputReportEvent(
    const AtomicString& type,
    const HIDInputReportEventInit* initializer)
    : Event(type, initializer) {}

HIDInputReportEvent::HIDInputReportEvent(const AtomicString& type,
                                         HIDDevice* device,
                                         uint8_t report_id,
                                         const Vector<uint8_t>& data)
    : Event(type, Bubbles::kNo, Cancelable::kNo) {}

void HIDInputReportEvent::Trace(blink::Visitor* visitor) {
  Event::Trace(visitor);
}

}  // namespace blink
