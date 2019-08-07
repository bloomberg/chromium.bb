// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_REPORT_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_REPORT_ITEM_H_

#include "services/device/public/mojom/hid.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

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
  const Vector<String>& strings() const { return strings_; }
  uint16_t reportSize() const { return 0; }
  uint16_t reportCount() const { return 0; }
  uint32_t unitExponent() const { return 0; }
  String unitSystem() const { return String(); }
  int8_t unitFactorLengthExponent() const { return 0; }
  int8_t unitFactorMassExponent() const { return 0; }
  int8_t unitFactorTimeExponent() const { return 0; }
  int8_t unitFactorTemperatureExponent() const { return 0; }
  int8_t unitFactorCurrentExponent() const { return 0; }
  int8_t unitFactorLuminousIntensityExponent() const { return 0; }
  int32_t logicalMinimum() const { return 0; }
  int32_t logicalMaximum() const { return 0; }
  int32_t physicalMinimum() const { return 0; }
  int32_t physicalMaximum() const { return 0; }

 private:
  Vector<uint32_t> usages_;
  Vector<String> strings_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_REPORT_ITEM_H_
