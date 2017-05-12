// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxDecorationData.h"

#include "core/layout/LayoutBox.h"
#include "core/paint/BoxPainter.h"
#include "core/style/BorderEdge.h"
#include "core/style/ComputedStyle.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

BoxDecorationData::BoxDecorationData(const LayoutBox& layout_box) {
  background_color =
      layout_box.Style()->VisitedDependentColor(CSSPropertyBackgroundColor);
  has_background =
      background_color.Alpha() || layout_box.Style()->HasBackgroundImage();
  DCHECK(has_background == layout_box.Style()->HasBackground());
  has_border_decoration = layout_box.Style()->HasBorderDecoration();
  has_appearance = layout_box.Style()->HasAppearance();
  bleed_avoidance = DetermineBackgroundBleedAvoidance(layout_box);
}

namespace {

bool BorderObscuresBackgroundEdge(const ComputedStyle& style) {
  BorderEdge edges[4];
  style.GetBorderEdgeInfo(edges);

  for (auto& edge : edges) {
    if (!edge.ObscuresBackgroundEdge())
      return false;
  }

  return true;
}

}  // anonymous namespace

BackgroundBleedAvoidance BoxDecorationData::DetermineBackgroundBleedAvoidance(
    const LayoutBox& layout_box) {
  if (layout_box.IsDocumentElement())
    return kBackgroundBleedNone;

  if (!has_background)
    return kBackgroundBleedNone;

  const ComputedStyle& box_style = layout_box.StyleRef();
  const bool has_border_radius = box_style.HasBorderRadius();
  if (!has_border_decoration || !has_border_radius ||
      layout_box.CanRenderBorderImage()) {
    if (layout_box.BackgroundShouldAlwaysBeClipped())
      return kBackgroundBleedClipOnly;
    // Border radius clipping may require layer bleed avoidance if we are going
    // to draw an image over something else, because we do not want the
    // antialiasing to lead to bleeding
    if (box_style.HasBackgroundImage() && has_border_radius) {
      // But if the top layer is opaque for the purposes of background painting,
      // we do not need the bleed avoidance because we will not paint anything
      // behind the top layer.  But only if we need to draw something
      // underneath.
      const FillLayer& fill_layer = layout_box.Style()->BackgroundLayers();
      if ((background_color.Alpha() || fill_layer.Next()) &&
          !fill_layer.ImageOccludesNextLayers(layout_box.GetDocument(),
                                              box_style))
        return kBackgroundBleedClipLayer;
    }
    return kBackgroundBleedNone;
  }

  if (BorderObscuresBackgroundEdge(box_style))
    return kBackgroundBleedShrinkBackground;

  return kBackgroundBleedClipLayer;
}

}  // namespace blink
