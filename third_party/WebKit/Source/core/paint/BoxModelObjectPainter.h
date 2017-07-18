// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxModelObjectPainter_h
#define BoxModelObjectPainter_h

#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/paint/BoxPainterBase.h"
#include "core/paint/RoundedInnerRectClipper.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class FillLayer;
class InlineFlowBox;
class LayoutRect;
struct PaintInfo;
class LayoutBoxModelObject;
class BackgroundImageGeometry;

// BoxModelObjectPainter is a class that can paint either a LayoutBox or a
// LayoutInline and allows code sharing between block and inline block painting.
class BoxModelObjectPainter : public BoxPainterBase {
  STACK_ALLOCATED();

 public:
  BoxModelObjectPainter(const LayoutBoxModelObject& box_model)
      : box_model_(box_model) {}

  void PaintFillLayer(const PaintInfo&,
                      const Color&,
                      const FillLayer&,
                      const LayoutRect&,
                      BackgroundBleedAvoidance,
                      BackgroundImageGeometry&,
                      const InlineFlowBox* = nullptr,
                      const LayoutSize& = LayoutSize(),
                      SkBlendMode = SkBlendMode::kSrcOver);

  static bool IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
      const LayoutBoxModelObject*,
      const PaintInfo&);

 private:
  Node* GetNode() const;
  LayoutRectOutsets BorderOutsets(const BoxPainterBase::FillLayerInfo&) const;
  LayoutRectOutsets PaddingOutsets(const BoxPainterBase::FillLayerInfo&) const;

  const LayoutBoxModelObject& box_model_;
};

}  // namespace blink

#endif  // BoxModelObjectPainter_h
