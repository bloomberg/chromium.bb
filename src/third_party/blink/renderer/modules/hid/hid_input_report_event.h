// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_INPUT_REPORT_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_INPUT_REPORT_EVENT_H_

#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class DOMDataView;
class HIDInputReportEventInit;
class HIDDevice;

class HIDInputReportEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static HIDInputReportEvent* Create(const AtomicString& type,
                                     const HIDInputReportEventInit*);
  static HIDInputReportEvent* Create(const AtomicString& type,
                                     HIDDevice*,
                                     uint8_t report_id,
                                     const Vector<uint8_t>& data);

  HIDInputReportEvent(const AtomicString& type, const HIDInputReportEventInit*);
  HIDInputReportEvent(const AtomicString& type,
                      HIDDevice* device,
                      uint8_t report_id,
                      const Vector<uint8_t>& data);

  HIDDevice* device() const { return nullptr; }
  uint8_t reportId() const { return 0; }
  DOMDataView* data() const { return nullptr; }

  void Trace(blink::Visitor*) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_INPUT_REPORT_EVENT_H_
