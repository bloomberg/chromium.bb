// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_collection_info.h"

#include "third_party/blink/renderer/modules/hid/hid_report_info.h"

namespace blink {

HIDCollectionInfo::HIDCollectionInfo(
    const device::mojom::blink::HidCollectionInfo& info) {}

HIDCollectionInfo::~HIDCollectionInfo() = default;

uint16_t HIDCollectionInfo::usagePage() const {
  return 0;
}

uint16_t HIDCollectionInfo::usage() const {
  return 0;
}

const HeapVector<Member<HIDCollectionInfo>>& HIDCollectionInfo::children()
    const {
  return children_;
}

const HeapVector<Member<HIDReportInfo>>& HIDCollectionInfo::inputReports()
    const {
  return input_reports_;
}

const HeapVector<Member<HIDReportInfo>>& HIDCollectionInfo::outputReports()
    const {
  return output_reports_;
}

const HeapVector<Member<HIDReportInfo>>& HIDCollectionInfo::featureReports()
    const {
  return feature_reports_;
}

uint32_t HIDCollectionInfo::collectionType() const {
  return 0;
}

void HIDCollectionInfo::Trace(blink::Visitor* visitor) {
  visitor->Trace(children_);
  visitor->Trace(input_reports_);
  visitor->Trace(output_reports_);
  visitor->Trace(feature_reports_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
