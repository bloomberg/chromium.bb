// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_FRAGMENT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_FRAGMENT_PAINTER_H_

#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/object_painter_base.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

struct PaintInfo;
struct PhysicalOffset;

// Generic fragment painter for paint logic shared between all types of
// fragments. LayoutNG version of ObjectPainter, based on ObjectPainterBase.
class NGFragmentPainter : public ObjectPainterBase {
  STACK_ALLOCATED();

 public:
  NGFragmentPainter(const NGPhysicalBoxFragment& box,
                    const DisplayItemClient& display_item_client)
      : box_fragment_(box), display_item_client_(display_item_client) {}

  void PaintOutline(const PaintInfo&, const PhysicalOffset& paint_offset);

  void AddURLRectIfNeeded(const PaintInfo&, const PhysicalOffset& paint_offset);

 private:
  const NGPhysicalBoxFragment& PhysicalFragment() const {
    return box_fragment_;
  }
  const DisplayItemClient& GetDisplayItemClient() const {
    return display_item_client_;
  }

  const NGPhysicalBoxFragment& box_fragment_;
  const DisplayItemClient& display_item_client_;
};

}  // namespace blink

#endif
