// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_report_info.h"

#include "third_party/blink/renderer/modules/hid/hid_report_item.h"

namespace blink {

HIDReportInfo::HIDReportInfo(
    const device::mojom::blink::HidReportDescription& report) {}

HIDReportInfo::~HIDReportInfo() {}

uint8_t HIDReportInfo::reportId() const {
  return 0;
}

const HeapVector<Member<HIDReportItem>>& HIDReportInfo::items() const {
  return items_;
}

void HIDReportInfo::Trace(blink::Visitor* visitor) {
  visitor->Trace(items_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
