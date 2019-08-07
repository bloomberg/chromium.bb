// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_REPORT_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_REPORT_ITEM_H_

#include "services/device/public/mojom/hid.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class HIDReportItem : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HIDReportItem(const device::mojom::blink::HidReportItem& item);
  ~HIDReportItem() override;

  bool isAbsolute() const { return false; }
  bool isArray() const { return false; }
  bool isRange() const { return false; }
  bool hasNull() const { return false; }
  const Vector<uint32_t>& usages() const { return usages_; }
  uint32_t usageMinimum() const { return 0; }
  uint32_t usageMaximum() const { return 0; }
  uint32_t designatorMinimum() const { return 0; }
  uint32_t designatorMaximum() const { return 0; }
  uint32_t stringMinimum() const { return 0; }
  uint32_t stringMaximum() const { return 0; }
  uint16_t reportSize() const { return 0; }
  uint16_t reportCount() const { return 0; }
  uint32_t unitExponent() const { return 0; }
  uint32_t unit() const { return 0; }
  int32_t logicalMinimum() const { return 0; }
  int32_t logicalMaximum() const { return 0; }
  int32_t physicalMinimum() const { return 0; }
  int32_t physicalMaximum() const { return 0; }

  void Trace(blink::Visitor* visitor) override;

 private:
  Vector<uint32_t> usages_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_REPORT_ITEM_H_
