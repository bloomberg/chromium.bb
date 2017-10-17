// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGForeignObjectPainter.h"

#include "core/layout/svg/LayoutSVGForeignObject.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/FloatClipRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGPaintContext.h"
#include "platform/wtf/Optional.h"

namespace blink {

namespace {

class BlockPainterDelegate : public LayoutBlock {
 public:
  BlockPainterDelegate(const LayoutSVGForeignObject& layout_svg_foreign_object)
      : LayoutBlock(nullptr),
        layout_svg_foreign_object_(layout_svg_foreign_object) {}

 private:
  void Paint(const PaintInfo& paint_info,
             const LayoutPoint& paint_offset) const final {
    BlockPainter(layout_svg_foreign_object_).Paint(paint_info, paint_offset);
  }
  const LayoutSVGForeignObject& layout_svg_foreign_object_;
};

}  // namespace

void SVGForeignObjectPainter::Paint(const PaintInfo& paint_info) {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection)
    return;

  PaintInfo paint_info_before_filtering(paint_info);
  paint_info_before_filtering.UpdateCullRect(
      layout_svg_foreign_object_.LocalSVGTransform());
  SVGTransformContext transform_context(
      paint_info_before_filtering.context, layout_svg_foreign_object_,
      layout_svg_foreign_object_.LocalSVGTransform());

  // In theory we should just let BlockPainter::paint() handle the clip, but for
  // now we don't allow normal overflow clip for LayoutSVGBlock, so we have to
  // apply clip manually. See LayoutSVGBlock::allowsOverflowClip() for details.
  Optional<FloatClipRecorder> clip_recorder;
  if (SVGLayoutSupport::IsOverflowHidden(&layout_svg_foreign_object_)) {
    clip_recorder.emplace(paint_info_before_filtering.context,
                          layout_svg_foreign_object_,
                          paint_info_before_filtering.phase,
                          FloatRect(layout_svg_foreign_object_.FrameRect()));
  }

  SVGPaintContext paint_context(layout_svg_foreign_object_,
                                paint_info_before_filtering);
  bool continue_rendering = true;
  if (paint_context.GetPaintInfo().phase == PaintPhase::kForeground)
    continue_rendering = paint_context.ApplyClipMaskAndFilterIfNecessary();

  if (continue_rendering) {
    // Paint all phases of FO elements atomically as though the FO element
    // established its own stacking context.  The delegate forwards calls to
    // paint() in LayoutObject::paintAllPhasesAtomically() to
    // BlockPainter::paint(), instead of m_layoutSVGForeignObject.paint() (which
    // would call this method again).
    BlockPainterDelegate delegate(layout_svg_foreign_object_);
    ObjectPainter(delegate).PaintAllPhasesAtomically(
        paint_context.GetPaintInfo(), LayoutPoint());
  }
}

}  // namespace blink
