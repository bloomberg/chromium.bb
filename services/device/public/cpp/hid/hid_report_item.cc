// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/hid_report_item.h"

#include "services/device/public/mojom/hid.mojom.h"

namespace device {

HidReportItem::HidReportItem(HidReportDescriptorItem::Tag tag,
                             uint32_t short_data,
                             const HidItemStateTable& state)
    : tag_(tag),
      report_info_(
          *reinterpret_cast<HidReportDescriptorItem::ReportInfo*>(&short_data)),
      report_id_(state.report_id),
      local_(state.local),
      global_(state.global_stack.empty()
                  ? HidItemStateTable::HidGlobalItemState()
                  : state.global_stack.back()),
      is_range_(state.local.usage_minimum != state.local.usage_maximum),
      has_strings_(state.local.string_index ||
                   (state.local.string_minimum != state.local.string_maximum)),
      has_designators_(
          state.local.designator_index ||
          (state.local.designator_minimum != state.local.designator_maximum)) {
  global_.usage_page = mojom::kPageUndefined;
  if (state.local.string_index) {
    local_.string_minimum = state.local.string_index;
    local_.string_maximum = state.local.string_index;
  }
  if (state.local.designator_index) {
    local_.designator_minimum = state.local.designator_index;
    local_.designator_maximum = state.local.designator_index;
  }
}

HidReportItem::~HidReportItem() = default;

}  // namespace device
