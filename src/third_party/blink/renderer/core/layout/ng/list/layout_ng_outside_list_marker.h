// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_OUTSIDE_LIST_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_OUTSIDE_LIST_MARKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"
#include "third_party/blink/renderer/core/layout/ng/list/list_marker.h"

namespace blink {

// A LayoutObject subclass for outside-positioned list markers in LayoutNG.
class CORE_EXPORT LayoutNGOutsideListMarker final
    : public LayoutNGBlockFlowMixin<LayoutBlockFlow> {
 public:
  explicit LayoutNGOutsideListMarker(Element*);

  void WillCollectInlines() override;

  const char* GetName() const override { return "LayoutNGOutsideListMarker"; }

  bool NeedsOccupyWholeLine() const;

  const ListMarker& Marker() const { return list_marker_; }
  ListMarker& Marker() { return list_marker_; }

 private:
  bool IsOfType(LayoutObjectType) const override;
  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  ListMarker list_marker_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGOutsideListMarker,
                                IsLayoutNGOutsideListMarker());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_OUTSIDE_LIST_MARKER_H_
