// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxPainterBase.h"

#include "core/dom/Document.h"
#include "core/frame/Settings.h"
#include "core/paint/BoxBorderPainter.h"
#include "core/paint/NinePieceImagePainter.h"
#include "core/paint/PaintInfo.h"
#include "core/style/BorderEdge.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ShadowList.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/LayoutRectOutsets.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

void BoxPainterBase::PaintNormalBoxShadow(const PaintInfo& info,
                                          const LayoutRect& paint_rect,
                                          const ComputedStyle& style,
                                          bool include_logical_left_edge,
                                          bool include_logical_right_edge) {
  if (!style.BoxShadow())
    return;
  GraphicsContext& context = info.context;

  FloatRoundedRect border = style.GetRoundedBorderFor(
      paint_rect, include_logical_left_edge, include_logical_right_edge);

  bool has_border_radius = style.HasBorderRadius();
  bool has_opaque_background =
      style.VisitedDependentColor(CSSPropertyBackgroundColor).Alpha() == 255;

  GraphicsContextStateSaver state_saver(context, false);

  const ShadowList* shadow_list = style.BoxShadow();
  for (size_t i = shadow_list->Shadows().size(); i--;) {
    const ShadowData& shadow = shadow_list->Shadows()[i];
    if (shadow.Style() != kNormal)
      continue;

    FloatSize shadow_offset(shadow.X(), shadow.Y());
    float shadow_blur = shadow.Blur();
    float shadow_spread = shadow.Spread();

    if (shadow_offset.IsZero() && !shadow_blur && !shadow_spread)
      continue;

    const Color& shadow_color = shadow.GetColor().Resolve(
        style.VisitedDependentColor(CSSPropertyColor));

    FloatRect fill_rect = border.Rect();
    fill_rect.Inflate(shadow_spread);
    if (fill_rect.IsEmpty())
      continue;

    FloatRect shadow_rect(border.Rect());
    shadow_rect.Inflate(shadow_blur + shadow_spread);
    shadow_rect.Move(shadow_offset);

    // Save the state and clip, if not already done.
    // The clip does not depend on any shadow-specific properties.
    if (!state_saver.Saved()) {
      state_saver.Save();
      if (has_border_radius) {
        FloatRoundedRect rect_to_clip_out = border;

        // If the box is opaque, it is unnecessary to clip it out. However,
        // doing so saves time when painting the shadow. On the other hand, it
        // introduces subpixel gaps along the corners. Those are avoided by
        // insetting the clipping path by one CSS pixel.
        if (has_opaque_background)
          rect_to_clip_out.InflateWithRadii(-1);

        if (!rect_to_clip_out.IsEmpty())
          context.ClipOutRoundedRect(rect_to_clip_out);
      } else {
        // This IntRect is correct even with fractional shadows, because it is
        // used for the rectangle of the box itself, which is always
        // pixel-aligned.
        FloatRect rect_to_clip_out = border.Rect();

        // If the box is opaque, it is unnecessary to clip it out. However,
        // doing so saves time when painting the shadow. On the other hand, it
        // introduces subpixel gaps along the edges if they are not
        // pixel-aligned. Those are avoided by insetting the clipping path by
        // one CSS pixel.
        if (has_opaque_background)
          rect_to_clip_out.Inflate(-1);

        if (!rect_to_clip_out.IsEmpty())
          context.ClipOut(rect_to_clip_out);
      }
    }

    // Draw only the shadow.
    context.SetShadow(shadow_offset, shadow_blur, shadow_color,
                      DrawLooperBuilder::kShadowRespectsTransforms,
                      DrawLooperBuilder::kShadowIgnoresAlpha, kDrawShadowOnly);

    if (has_border_radius) {
      FloatRoundedRect influence_rect(
          PixelSnappedIntRect(LayoutRect(shadow_rect)), border.GetRadii());
      float change_amount = 2 * shadow_blur + shadow_spread;
      if (change_amount >= 0)
        influence_rect.ExpandRadii(change_amount);
      else
        influence_rect.ShrinkRadii(-change_amount);

      FloatRoundedRect rounded_fill_rect = border;
      rounded_fill_rect.Inflate(shadow_spread);

      if (shadow_spread >= 0)
        rounded_fill_rect.ExpandRadii(shadow_spread);
      else
        rounded_fill_rect.ShrinkRadii(-shadow_spread);
      if (!rounded_fill_rect.IsRenderable())
        rounded_fill_rect.AdjustRadii();
      rounded_fill_rect.ConstrainRadii();
      context.FillRoundedRect(rounded_fill_rect, Color::kBlack);
    } else {
      context.FillRect(fill_rect, Color::kBlack);
    }
  }
}

void BoxPainterBase::PaintInsetBoxShadow(const PaintInfo& info,
                                         const LayoutRect& paint_rect,
                                         const ComputedStyle& style,
                                         bool include_logical_left_edge,
                                         bool include_logical_right_edge) {
  if (!style.BoxShadow())
    return;
  FloatRoundedRect bounds = style.GetRoundedInnerBorderFor(
      paint_rect, include_logical_left_edge, include_logical_right_edge);
  PaintInsetBoxShadowInBounds(info, bounds, style, include_logical_left_edge,
                              include_logical_right_edge);
}

void BoxPainterBase::PaintInsetBoxShadowInBounds(
    const PaintInfo& info,
    const FloatRoundedRect& bounds,
    const ComputedStyle& style,
    bool include_logical_left_edge,
    bool include_logical_right_edge) {
  // The caller should have checked style.boxShadow() when computing bounds.
  DCHECK(style.BoxShadow());
  GraphicsContext& context = info.context;

  bool is_horizontal = style.IsHorizontalWritingMode();
  GraphicsContextStateSaver state_saver(context, false);

  const ShadowList* shadow_list = style.BoxShadow();
  for (size_t i = shadow_list->Shadows().size(); i--;) {
    const ShadowData& shadow = shadow_list->Shadows()[i];
    if (shadow.Style() != kInset)
      continue;

    FloatSize shadow_offset(shadow.X(), shadow.Y());
    float shadow_blur = shadow.Blur();
    float shadow_spread = shadow.Spread();

    if (shadow_offset.IsZero() && !shadow_blur && !shadow_spread)
      continue;

    const Color& shadow_color = shadow.GetColor().Resolve(
        style.VisitedDependentColor(CSSPropertyColor));

    // The inset shadow case.
    GraphicsContext::Edges clipped_edges = GraphicsContext::kNoEdge;
    if (!include_logical_left_edge) {
      if (is_horizontal)
        clipped_edges |= GraphicsContext::kLeftEdge;
      else
        clipped_edges |= GraphicsContext::kTopEdge;
    }
    if (!include_logical_right_edge) {
      if (is_horizontal)
        clipped_edges |= GraphicsContext::kRightEdge;
      else
        clipped_edges |= GraphicsContext::kBottomEdge;
    }
    context.DrawInnerShadow(bounds, shadow_color, shadow_offset, shadow_blur,
                            shadow_spread, clipped_edges);
  }
}

bool BoxPainterBase::ShouldForceWhiteBackgroundForPrintEconomy(
    const Document& document,
    const ComputedStyle& style) {
  return document.Printing() &&
         style.PrintColorAdjust() == EPrintColorAdjust::kEconomy &&
         (!document.GetSettings() ||
          !document.GetSettings()->GetShouldPrintBackgrounds());
}

bool BoxPainterBase::CalculateFillLayerOcclusionCulling(
    FillLayerOcclusionOutputList& reversed_paint_list,
    const FillLayer& fill_layer,
    const Document& document,
    const ComputedStyle& style) {
  bool is_non_associative = false;
  for (auto current_layer = &fill_layer; current_layer;
       current_layer = current_layer->Next()) {
    reversed_paint_list.push_back(current_layer);
    // Stop traversal when an opaque layer is encountered.
    // FIXME : It would be possible for the following occlusion culling test to
    // be more aggressive on layers with no repeat by testing whether the image
    // covers the layout rect.  Testing that here would imply duplicating a lot
    // of calculations that are currently done in
    // LayoutBoxModelObject::paintFillLayer. A more efficient solution might be
    // to move the layer recursion into paintFillLayer, or to compute the layer
    // geometry here and pass it down.

    // TODO(trchen): Need to check compositing mode as well.
    if (current_layer->BlendMode() != WebBlendMode::kNormal)
      is_non_associative = true;

    // TODO(trchen): A fill layer cannot paint if the calculated tile size is
    // empty.  This occlusion check can be wrong.
    if (current_layer->ClipOccludesNextLayers() &&
        current_layer->ImageOccludesNextLayers(document, style)) {
      if (current_layer->Clip() == kBorderFillBox)
        is_non_associative = false;
      break;
    }
  }
  return is_non_associative;
}

FloatRoundedRect BoxPainterBase::GetBackgroundRoundedRect(
    const ComputedStyle& style,
    const LayoutRect& border_rect,
    bool has_line_box_sibling,
    const LayoutSize& inline_box_size,
    bool include_logical_left_edge,
    bool include_logical_right_edge) {
  FloatRoundedRect border = style.GetRoundedBorderFor(
      border_rect, include_logical_left_edge, include_logical_right_edge);
  if (has_line_box_sibling) {
    FloatRoundedRect segment_border = style.GetRoundedBorderFor(
        LayoutRect(LayoutPoint(), LayoutSize(FlooredIntSize(inline_box_size))),
        include_logical_left_edge, include_logical_right_edge);
    border.SetRadii(segment_border.GetRadii());
  }
  return border;
}

FloatRoundedRect BoxPainterBase::BackgroundRoundedRectAdjustedForBleedAvoidance(
    const ComputedStyle& style,
    const LayoutRect& border_rect,
    BackgroundBleedAvoidance bleed_avoidance,
    bool has_line_box_sibling,
    const LayoutSize& box_size,
    bool include_logical_left_edge,
    bool include_logical_right_edge) {
  if (bleed_avoidance == kBackgroundBleedShrinkBackground) {
    // Inset the background rect by a "safe" amount: 1/2 border-width for opaque
    // border styles, 1/6 border-width for double borders.

    // TODO(fmalita): we should be able to fold these parameters into
    // BoxBorderInfo or BoxDecorationData and avoid calling getBorderEdgeInfo
    // redundantly here.
    BorderEdge edges[4];
    style.GetBorderEdgeInfo(edges, include_logical_left_edge,
                            include_logical_right_edge);

    // Use the most conservative inset to avoid mixed-style corner issues.
    float fractional_inset = 1.0f / 2;
    for (auto& edge : edges) {
      if (edge.BorderStyle() == EBorderStyle::kDouble) {
        fractional_inset = 1.0f / 6;
        break;
      }
    }

    FloatRectOutsets insets(-fractional_inset * edges[kBSTop].Width(),
                            -fractional_inset * edges[kBSRight].Width(),
                            -fractional_inset * edges[kBSBottom].Width(),
                            -fractional_inset * edges[kBSLeft].Width());

    FloatRoundedRect background_rounded_rect = GetBackgroundRoundedRect(
        style, border_rect, has_line_box_sibling, box_size,
        include_logical_left_edge, include_logical_right_edge);
    FloatRect inset_rect(background_rounded_rect.Rect());
    inset_rect.Expand(insets);
    FloatRoundedRect::Radii inset_radii(background_rounded_rect.GetRadii());
    inset_radii.Shrink(-insets.Top(), -insets.Bottom(), -insets.Left(),
                       -insets.Right());
    return FloatRoundedRect(inset_rect, inset_radii);
  }

  return GetBackgroundRoundedRect(style, border_rect, has_line_box_sibling,
                                  box_size, include_logical_left_edge,
                                  include_logical_right_edge);
}

BoxPainterBase::FillLayerInfo::FillLayerInfo(
    const Document& doc,
    const ComputedStyle& style,
    bool has_overflow_clip,
    Color bg_color,
    const FillLayer& layer,
    BackgroundBleedAvoidance bleed_avoidance,
    bool include_left,
    bool include_right)
    : image(layer.GetImage()),
      color(bg_color),
      include_left_edge(include_left),
      include_right_edge(include_right),
      is_bottom_layer(!layer.Next()),
      is_border_fill(layer.Clip() == kBorderFillBox),
      is_clipped_with_local_scrolling(has_overflow_clip &&
                                      layer.Attachment() ==
                                          kLocalBackgroundAttachment) {
  // When printing backgrounds is disabled or using economy mode,
  // change existing background colors and images to a solid white background.
  // If there's no bg color or image, leave it untouched to avoid affecting
  // transparency.  We don't try to avoid loading the background images,
  // because this style flag is only set when printing, and at that point
  // we've already loaded the background images anyway. (To avoid loading the
  // background images we'd have to do this check when applying styles rather
  // than while layout.)
  if (BoxPainterBase::ShouldForceWhiteBackgroundForPrintEconomy(doc, style)) {
    // Note that we can't reuse this variable below because the bgColor might
    // be changed.
    bool should_paint_background_color = is_bottom_layer && color.Alpha();
    if (image || should_paint_background_color) {
      color = Color::kWhite;
      image = nullptr;
    }
  }

  const bool has_rounded_border =
      style.HasBorderRadius() && (include_left_edge || include_right_edge);
  // BorderFillBox radius clipping is taken care of by
  // BackgroundBleedClip{Only,Layer}
  is_rounded_fill =
      has_rounded_border &&
      !(is_border_fill && BleedAvoidanceIsClipping(bleed_avoidance));

  should_paint_image = image && image->CanRender();
  should_paint_color =
      is_bottom_layer && color.Alpha() &&
      (!should_paint_image || !layer.ImageOccludesNextLayers(doc, style));
}

FloatRoundedRect BoxPainterBase::RoundedBorderRectForClip(
    const ComputedStyle& style,
    const BoxPainterBase::FillLayerInfo& info,
    const FillLayer& bg_layer,
    const LayoutRect& rect,
    BackgroundBleedAvoidance bleed_avoidance,
    bool has_line_box_sibling,
    const LayoutSize& box_size,
    LayoutRectOutsets border_padding_insets) {
  FloatRoundedRect border =
      info.is_border_fill
          ? BackgroundRoundedRectAdjustedForBleedAvoidance(
                style, rect, bleed_avoidance, has_line_box_sibling, box_size,
                info.include_left_edge, info.include_right_edge)
          : GetBackgroundRoundedRect(style, rect, has_line_box_sibling,
                                     box_size, info.include_left_edge,
                                     info.include_right_edge);

  // Clip to the padding or content boxes as necessary.
  if (bg_layer.Clip() == kContentFillBox) {
    border = style.GetRoundedInnerBorderFor(
        LayoutRect(border.Rect()), border_padding_insets,
        info.include_left_edge, info.include_right_edge);
  } else if (bg_layer.Clip() == kPaddingFillBox) {
    border = style.GetRoundedInnerBorderFor(LayoutRect(border.Rect()),
                                            info.include_left_edge,
                                            info.include_right_edge);
  }
  return border;
}

void BoxPainterBase::PaintBorder(const ImageResourceObserver& obj,
                                 const Document& document,
                                 Node* node,
                                 const PaintInfo& info,
                                 const LayoutRect& rect,
                                 const ComputedStyle& style,
                                 BackgroundBleedAvoidance bleed_avoidance,
                                 bool include_logical_left_edge,
                                 bool include_logical_right_edge) {
  // border-image is not affected by border-radius.
  if (NinePieceImagePainter::Paint(info.context, obj, document, node, rect,
                                   style, style.BorderImage())) {
    return;
  }

  const BoxBorderPainter border_painter(rect, style, bleed_avoidance,
                                        include_logical_left_edge,
                                        include_logical_right_edge);
  border_painter.PaintBorder(info, rect);
}

}  // namespace blink
