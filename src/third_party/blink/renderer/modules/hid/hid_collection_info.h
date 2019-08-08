// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_COLLECTION_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_COLLECTION_INFO_H_

#include "services/device/public/mojom/hid.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class HIDReportInfo;

class HIDCollectionInfo : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HIDCollectionInfo(
      const device::mojom::blink::HidCollectionInfo& collection);
  ~HIDCollectionInfo() override;

  uint16_t usagePage() const;
  uint16_t usage() const;
  const HeapVector<Member<HIDCollectionInfo>>& children() const;
  const HeapVector<Member<HIDReportInfo>>& inputReports() const;
  const HeapVector<Member<HIDReportInfo>>& outputReports() const;
  const HeapVector<Member<HIDReportInfo>>& featureReports() const;
  const Vector<uint8_t>& reportIds() const;
  uint32_t collectionType() const;

  void Trace(blink::Visitor* visitor) override;

 private:
  HeapVector<Member<HIDCollectionInfo>> children_;
  HeapVector<Member<HIDReportInfo>> input_reports_;
  HeapVector<Member<HIDReportInfo>> output_reports_;
  HeapVector<Member<HIDReportInfo>> feature_reports_;
  Vector<uint8_t> report_ids_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_COLLECTION_INFO_H_
