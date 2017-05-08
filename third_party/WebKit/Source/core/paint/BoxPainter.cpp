// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxPainter.h"

#include "core/HTMLNames.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/BoxBorderPainter.h"
#include "core/paint/BoxDecorationData.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/NinePieceImagePainter.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/RoundedInnerRectClipper.h"
#include "core/paint/ScrollRecorder.h"
#include "core/paint/ThemePainter.h"
#include "core/style/BorderEdge.h"
#include "core/style/ShadowList.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRectOutsets.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "platform/wtf/Optional.h"

namespace blink {

bool BoxPainter::IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
    const LayoutBoxModelObject* obj,
    const PaintInfo& paint_info) {
  return paint_info.PaintFlags() & kPaintLayerPaintingOverflowContents &&
         !(paint_info.PaintFlags() &
           kPaintLayerPaintingCompositingBackgroundPhase) &&
         obj == paint_info.PaintContainer();
}

void BoxPainter::Paint(const PaintInfo& paint_info,
                       const LayoutPoint& paint_offset) {
  ObjectPainter(layout_box_).CheckPaintOffset(paint_info, paint_offset);
  // Default implementation. Just pass paint through to the children.
  PaintChildren(paint_info, paint_offset + layout_box_.Location());
}

void BoxPainter::PaintChildren(const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset) {
  PaintInfo child_info(paint_info);
  for (LayoutObject* child = layout_box_.SlowFirstChild(); child;
       child = child->NextSibling())
    child->Paint(child_info, paint_offset);
}

void BoxPainter::PaintBoxDecorationBackground(const PaintInfo& paint_info,
                                              const LayoutPoint& paint_offset) {
  LayoutRect paint_rect;
  Optional<ScrollRecorder> scroll_recorder;
  if (IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          &layout_box_, paint_info)) {
    // For the case where we are painting the background into the scrolling
    // contents layer of a composited scroller we need to include the entire
    // overflow rect.
    paint_rect = layout_box_.LayoutOverflowRect();
    scroll_recorder.emplace(paint_info.context, layout_box_, paint_info.phase,
                            layout_box_.ScrolledContentOffset());

    // The background painting code assumes that the borders are part of the
    // paintRect so we expand the paintRect by the border size when painting the
    // background into the scrolling contents layer.
    paint_rect.ExpandEdges(layout_box_.BorderTop(), layout_box_.BorderRight(),
                           layout_box_.BorderBottom(),
                           layout_box_.BorderLeft());
  } else {
    paint_rect = layout_box_.BorderBoxRect();
  }

  paint_rect.MoveBy(paint_offset);
  PaintBoxDecorationBackgroundWithRect(paint_info, paint_offset, paint_rect);
}

LayoutRect BoxPainter::BoundsForDrawingRecorder(
    const PaintInfo& paint_info,
    const LayoutPoint& adjusted_paint_offset) {
  LayoutRect bounds =
      IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          &layout_box_, paint_info)
          ? layout_box_.LayoutOverflowRect()
          : layout_box_.SelfVisualOverflowRect();
  bounds.MoveBy(adjusted_paint_offset);
  return bounds;
}

void BoxPainter::PaintBoxDecorationBackgroundWithRect(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const LayoutRect& paint_rect) {
  bool painting_overflow_contents =
      IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          &layout_box_, paint_info);
  const ComputedStyle& style = layout_box_.StyleRef();

  Optional<DisplayItemCacheSkipper> cache_skipper;
  // Disable cache in under-invalidation checking mode for MediaSliderPart
  // because we always paint using the latest data (buffered ranges, current
  // time and duration) which may be different from the cached data.
  if ((RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled() &&
       style.Appearance() == kMediaSliderPart)
      // We may paint a delayed-invalidation object before it's actually
      // invalidated. Note this would be handled for us by
      // LayoutObjectDrawingRecorder but we have to use DrawingRecorder as we
      // may use the scrolling contents layer as DisplayItemClient below.
      || layout_box_.FullPaintInvalidationReason() ==
             kPaintInvalidationDelayedFull) {
    cache_skipper.emplace(paint_info.context);
  }

  const DisplayItemClient& display_item_client =
      painting_overflow_contents ? static_cast<const DisplayItemClient&>(
                                       *layout_box_.Layer()
                                            ->GetCompositedLayerMapping()
                                            ->ScrollingContentsLayer())
                                 : layout_box_;
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(
      paint_info.context, display_item_client,
      DisplayItem::kBoxDecorationBackground,
      FloatRect(BoundsForDrawingRecorder(paint_info, paint_offset)));
  BoxDecorationData box_decoration_data(layout_box_);
  GraphicsContextStateSaver state_saver(paint_info.context, false);

  if (!painting_overflow_contents) {
    // FIXME: Should eventually give the theme control over whether the box
    // shadow should paint, since controls could have custom shadows of their
    // own.
    PaintNormalBoxShadow(paint_info, paint_rect, style);

    if (BleedAvoidanceIsClipping(box_decoration_data.bleed_avoidance)) {
      state_saver.Save();
      FloatRoundedRect border = style.GetRoundedBorderFor(paint_rect);
      paint_info.context.ClipRoundedRect(border);

      if (box_decoration_data.bleed_avoidance == kBackgroundBleedClipLayer)
        paint_info.context.BeginLayer();
    }
  }

  // If we have a native theme appearance, paint that before painting our
  // background.  The theme will tell us whether or not we should also paint the
  // CSS background.
  IntRect snapped_paint_rect(PixelSnappedIntRect(paint_rect));
  ThemePainter& theme_painter = LayoutTheme::GetTheme().Painter();
  bool theme_painted =
      box_decoration_data.has_appearance &&
      !theme_painter.Paint(layout_box_, paint_info, snapped_paint_rect);
  bool should_paint_background =
      !theme_painted && (!paint_info.SkipRootBackground() ||
                         paint_info.PaintContainer() != &layout_box_);
  if (should_paint_background) {
    PaintBackground(paint_info, paint_rect,
                    box_decoration_data.background_color,
                    box_decoration_data.bleed_avoidance);

    if (box_decoration_data.has_appearance)
      theme_painter.PaintDecorations(layout_box_, paint_info,
                                     snapped_paint_rect);
  }

  if (!painting_overflow_contents) {
    PaintInsetBoxShadow(paint_info, paint_rect, style);

    // The theme will tell us whether or not we should also paint the CSS
    // border.
    if (box_decoration_data.has_border_decoration &&
        (!box_decoration_data.has_appearance ||
         (!theme_painted &&
          LayoutTheme::GetTheme().Painter().PaintBorderOnly(
              layout_box_, paint_info, snapped_paint_rect))) &&
        !(layout_box_.IsTable() &&
          ToLayoutTable(&layout_box_)->ShouldCollapseBorders())) {
      PaintBorder(layout_box_, paint_info, paint_rect, style,
                  box_decoration_data.bleed_avoidance);
    }
  }

  if (box_decoration_data.bleed_avoidance == kBackgroundBleedClipLayer)
    paint_info.context.EndLayer();
}

void BoxPainter::PaintBackground(const PaintInfo& paint_info,
                                 const LayoutRect& paint_rect,
                                 const Color& background_color,
                                 BackgroundBleedAvoidance bleed_avoidance) {
  if (layout_box_.IsDocumentElement())
    return;
  if (layout_box_.BackgroundStolenForBeingBody())
    return;
  if (layout_box_.BackgroundIsKnownToBeObscured())
    return;
  PaintFillLayers(paint_info, background_color,
                  layout_box_.Style()->BackgroundLayers(), paint_rect,
                  bleed_avoidance);
}

bool BoxPainter::CalculateFillLayerOcclusionCulling(
    FillLayerOcclusionOutputList& reversed_paint_list,
    const FillLayer& fill_layer) {
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
    if (current_layer->BlendMode() != kWebBlendModeNormal)
      is_non_associative = true;

    // TODO(trchen): A fill layer cannot paint if the calculated tile size is
    // empty.  This occlusion check can be wrong.
    if (current_layer->ClipOccludesNextLayers() &&
        current_layer->ImageOccludesNextLayers(layout_box_)) {
      if (current_layer->Clip() == kBorderFillBox)
        is_non_associative = false;
      break;
    }
  }
  return is_non_associative;
}

void BoxPainter::PaintFillLayers(const PaintInfo& paint_info,
                                 const Color& c,
                                 const FillLayer& fill_layer,
                                 const LayoutRect& rect,
                                 BackgroundBleedAvoidance bleed_avoidance,
                                 SkBlendMode op,
                                 const LayoutObject* background_object) {
  FillLayerOcclusionOutputList reversed_paint_list;
  bool should_draw_background_in_separate_buffer =
      CalculateFillLayerOcclusionCulling(reversed_paint_list, fill_layer);

  // TODO(trchen): We can optimize out isolation group if we have a
  // non-transparent background color and the bottom layer encloses all other
  // layers.

  GraphicsContext& context = paint_info.context;

  if (should_draw_background_in_separate_buffer)
    context.BeginLayer();

  for (auto it = reversed_paint_list.rbegin(); it != reversed_paint_list.rend();
       ++it)
    PaintFillLayer(layout_box_, paint_info, c, **it, rect, bleed_avoidance, 0,
                   LayoutSize(), op, background_object);

  if (should_draw_background_in_separate_buffer)
    context.EndLayer();
}

namespace {

FloatRoundedRect GetBackgroundRoundedRect(const LayoutObject& obj,
                                          const LayoutRect& border_rect,
                                          const InlineFlowBox* box,
                                          LayoutUnit inline_box_width,
                                          LayoutUnit inline_box_height,
                                          bool include_logical_left_edge,
                                          bool include_logical_right_edge) {
  FloatRoundedRect border = obj.Style()->GetRoundedBorderFor(
      border_rect, include_logical_left_edge, include_logical_right_edge);
  if (box && (box->NextLineBox() || box->PrevLineBox())) {
    FloatRoundedRect segment_border = obj.Style()->GetRoundedBorderFor(
        LayoutRect(0, 0, inline_box_width.ToInt(), inline_box_height.ToInt()),
        include_logical_left_edge, include_logical_right_edge);
    border.SetRadii(segment_border.GetRadii());
  }
  return border;
}

FloatRoundedRect BackgroundRoundedRectAdjustedForBleedAvoidance(
    const LayoutObject& obj,
    const LayoutRect& border_rect,
    BackgroundBleedAvoidance bleed_avoidance,
    const InlineFlowBox* box,
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
    obj.Style()->GetBorderEdgeInfo(edges, include_logical_left_edge,
                                   include_logical_right_edge);

    // Use the most conservative inset to avoid mixed-style corner issues.
    float fractional_inset = 1.0f / 2;
    for (auto& edge : edges) {
      if (edge.BorderStyle() == kBorderStyleDouble) {
        fractional_inset = 1.0f / 6;
        break;
      }
    }

    FloatRectOutsets insets(-fractional_inset * edges[kBSTop].Width(),
                            -fractional_inset * edges[kBSRight].Width(),
                            -fractional_inset * edges[kBSBottom].Width(),
                            -fractional_inset * edges[kBSLeft].Width());

    FloatRoundedRect background_rounded_rect = GetBackgroundRoundedRect(
        obj, border_rect, box, box_size.Width(), box_size.Height(),
        include_logical_left_edge, include_logical_right_edge);
    FloatRect inset_rect(background_rounded_rect.Rect());
    inset_rect.Expand(insets);
    FloatRoundedRect::Radii inset_radii(background_rounded_rect.GetRadii());
    inset_radii.Shrink(-insets.Top(), -insets.Bottom(), -insets.Left(),
                       -insets.Right());
    return FloatRoundedRect(inset_rect, inset_radii);
  }

  return GetBackgroundRoundedRect(obj, border_rect, box, box_size.Width(),
                                  box_size.Height(), include_logical_left_edge,
                                  include_logical_right_edge);
}

struct FillLayerInfo {
  STACK_ALLOCATED();

  FillLayerInfo(const LayoutBoxModelObject& obj,
                Color bg_color,
                const FillLayer& layer,
                BackgroundBleedAvoidance bleed_avoidance,
                const InlineFlowBox* box)
      : image(layer.GetImage()),
        color(bg_color),
        include_left_edge(box ? box->IncludeLogicalLeftEdge() : true),
        include_right_edge(box ? box->IncludeLogicalRightEdge() : true),
        is_bottom_layer(!layer.Next()),
        is_border_fill(layer.Clip() == kBorderFillBox),
        is_clipped_with_local_scrolling(obj.HasOverflowClip() &&
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
    if (BoxPainter::ShouldForceWhiteBackgroundForPrintEconomy(
            obj.StyleRef(), obj.GetDocument())) {
      // Note that we can't reuse this variable below because the bgColor might
      // be changed.
      bool should_paint_background_color = is_bottom_layer && color.Alpha();
      if (image || should_paint_background_color) {
        color = Color::kWhite;
        image = nullptr;
      }
    }

    const bool has_rounded_border = obj.Style()->HasBorderRadius() &&
                                    (include_left_edge || include_right_edge);
    // BorderFillBox radius clipping is taken care of by
    // BackgroundBleedClip{Only,Layer}
    is_rounded_fill =
        has_rounded_border &&
        !(is_border_fill && BleedAvoidanceIsClipping(bleed_avoidance));

    should_paint_image = image && image->CanRender();
    should_paint_color =
        is_bottom_layer && color.Alpha() &&
        (!should_paint_image || !layer.ImageOccludesNextLayers(obj));
  }

  // FillLayerInfo is a temporary, stack-allocated container which cannot
  // outlive the StyleImage.  This would normally be a raw pointer, if not for
  // the Oilpan tooling complaints.
  Member<StyleImage> image;
  Color color;

  bool include_left_edge;
  bool include_right_edge;
  bool is_bottom_layer;
  bool is_border_fill;
  bool is_clipped_with_local_scrolling;
  bool is_rounded_fill;

  bool should_paint_image;
  bool should_paint_color;
};

// RAII image paint helper.
class ImagePaintContext {
  STACK_ALLOCATED();

 public:
  ImagePaintContext(const LayoutBoxModelObject& obj,
                    GraphicsContext& context,
                    const FillLayer& layer,
                    const StyleImage& style_image,
                    SkBlendMode op,
                    const LayoutObject* background_object,
                    const LayoutSize& container_size)
      : context_(context),
        previous_interpolation_quality_(context.ImageInterpolationQuality()) {
    SkBlendMode bg_op =
        WebCoreCompositeToSkiaComposite(layer.Composite(), layer.BlendMode());
    // if op != SkBlendMode::kSrcOver, a mask is being painted.
    composite_op_ = (op == SkBlendMode::kSrcOver) ? bg_op : op;

    const LayoutObject& image_client =
        background_object ? *background_object : obj;
    image_ = style_image.GetImage(image_client, FlooredIntSize(container_size));

    interpolation_quality_ = BoxPainter::ChooseInterpolationQuality(
        image_client, image_.Get(), &layer, container_size);
    if (interpolation_quality_ != previous_interpolation_quality_)
      context.SetImageInterpolationQuality(interpolation_quality_);

    if (layer.MaskSourceType() == kMaskLuminance)
      context.SetColorFilter(kColorFilterLuminanceToAlpha);
  }

  ~ImagePaintContext() {
    if (interpolation_quality_ != previous_interpolation_quality_)
      context_.SetImageInterpolationQuality(previous_interpolation_quality_);
  }

  Image* GetImage() const { return image_.Get(); }

  SkBlendMode CompositeOp() const { return composite_op_; }

 private:
  RefPtr<Image> image_;
  GraphicsContext& context_;
  SkBlendMode composite_op_;
  InterpolationQuality interpolation_quality_;
  InterpolationQuality previous_interpolation_quality_;
};

inline bool PaintFastBottomLayer(const LayoutBoxModelObject& obj,
                                 const PaintInfo& paint_info,
                                 const FillLayerInfo& info,
                                 const FillLayer& layer,
                                 const LayoutRect& rect,
                                 BackgroundBleedAvoidance bleed_avoidance,
                                 const InlineFlowBox* box,
                                 const LayoutSize& box_size,
                                 SkBlendMode op,
                                 const LayoutObject* background_object,
                                 Optional<BackgroundImageGeometry>& geometry) {
  // Painting a background image from an ancestor onto a cell is a complex case.
  if (obj.IsTableCell() && background_object &&
      !background_object->IsTableCell())
    return false;
  // Complex cases not handled on the fast path.
  if (!info.is_bottom_layer || !info.is_border_fill ||
      info.is_clipped_with_local_scrolling)
    return false;

  // Transparent layer, nothing to paint.
  if (!info.should_paint_color && !info.should_paint_image)
    return true;

  // When the layer has an image, figure out whether it is covered by a single
  // tile.
  FloatRect image_tile;
  if (info.should_paint_image) {
    // Avoid image shaders when printing (poorly supported in PDF).
    if (info.is_rounded_fill && paint_info.IsPrinting())
      return false;

    DCHECK(!geometry);
    geometry.emplace();
    geometry->Calculate(obj, background_object, paint_info.PaintContainer(),
                        paint_info.GetGlobalPaintFlags(), layer, rect);

    if (!geometry->DestRect().IsEmpty()) {
      // The tile is too small.
      if (geometry->TileSize().Width() < rect.Width() ||
          geometry->TileSize().Height() < rect.Height())
        return false;

      image_tile = Image::ComputeTileContaining(
          FloatPoint(geometry->DestRect().Location()),
          FloatSize(geometry->TileSize()), FloatPoint(geometry->Phase()),
          FloatSize(geometry->SpaceSize()));

      // The tile is misaligned.
      if (!image_tile.Contains(FloatRect(rect)))
        return false;
    }
  }

  // At this point we're committed to the fast path: the destination (r)rect
  // fits within a single tile, and we can paint it using direct draw(R)Rect()
  // calls.
  GraphicsContext& context = paint_info.context;
  FloatRoundedRect border =
      info.is_rounded_fill
          ? BackgroundRoundedRectAdjustedForBleedAvoidance(
                obj, rect, bleed_avoidance, box, box_size,
                info.include_left_edge, info.include_right_edge)
          : FloatRoundedRect(PixelSnappedIntRect(rect));

  Optional<RoundedInnerRectClipper> clipper;
  if (info.is_rounded_fill && !border.IsRenderable()) {
    // When the rrect is not renderable, we resort to clipping.
    // RoundedInnerRectClipper handles this case via discrete, corner-wise
    // clipping.
    clipper.emplace(obj, paint_info, rect, border, kApplyToContext);
    border.SetRadii(FloatRoundedRect::Radii());
  }

  // Paint the color if needed.
  if (info.should_paint_color)
    context.FillRoundedRect(border, info.color);

  // Paint the image if needed.
  if (!info.should_paint_image || image_tile.IsEmpty())
    return true;

  const ImagePaintContext image_context(obj, context, layer, *info.image, op,
                                        background_object,
                                        geometry->TileSize());
  if (!image_context.GetImage())
    return true;

  const FloatSize intrinsic_tile_size =
      image_context.GetImage()->HasRelativeSize()
          ? image_tile.Size()
          : FloatSize(image_context.GetImage()->Size());
  const FloatRect src_rect = Image::ComputeSubsetForTile(
      image_tile, border.Rect(), intrinsic_tile_size);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
               "data", InspectorPaintImageEvent::Data(obj, *info.image));
  context.DrawImageRRect(image_context.GetImage(), border, src_rect,
                         image_context.CompositeOp());

  return true;
}

}  // anonymous namespace

void BoxPainter::PaintFillLayer(const LayoutBoxModelObject& obj,
                                const PaintInfo& paint_info,
                                const Color& color,
                                const FillLayer& bg_layer,
                                const LayoutRect& rect,
                                BackgroundBleedAvoidance bleed_avoidance,
                                const InlineFlowBox* box,
                                const LayoutSize& box_size,
                                SkBlendMode op,
                                const LayoutObject* background_object) {
  GraphicsContext& context = paint_info.context;
  if (rect.IsEmpty())
    return;

  const FillLayerInfo info(obj, color, bg_layer, bleed_avoidance, box);
  Optional<BackgroundImageGeometry> geometry;

  // Fast path for drawing simple color backgrounds.
  if (PaintFastBottomLayer(obj, paint_info, info, bg_layer, rect,
                           bleed_avoidance, box, box_size, op,
                           background_object, geometry)) {
    return;
  }

  Optional<RoundedInnerRectClipper> clip_to_border;
  if (info.is_rounded_fill) {
    FloatRoundedRect border =
        info.is_border_fill
            ? BackgroundRoundedRectAdjustedForBleedAvoidance(
                  obj, rect, bleed_avoidance, box, box_size,
                  info.include_left_edge, info.include_right_edge)
            : GetBackgroundRoundedRect(
                  obj, rect, box, box_size.Width(), box_size.Height(),
                  info.include_left_edge, info.include_right_edge);

    // Clip to the padding or content boxes as necessary.
    if (bg_layer.Clip() == kContentFillBox) {
      border = obj.Style()->GetRoundedInnerBorderFor(
          LayoutRect(border.Rect()),
          LayoutRectOutsets(-(obj.PaddingTop() + obj.BorderTop()),
                            -(obj.PaddingRight() + obj.BorderRight()),
                            -(obj.PaddingBottom() + obj.BorderBottom()),
                            -(obj.PaddingLeft() + obj.BorderLeft())),
          info.include_left_edge, info.include_right_edge);
    } else if (bg_layer.Clip() == kPaddingFillBox) {
      border = obj.Style()->GetRoundedInnerBorderFor(LayoutRect(border.Rect()),
                                                     info.include_left_edge,
                                                     info.include_right_edge);
    }

    clip_to_border.emplace(obj, paint_info, rect, border, kApplyToContext);
  }

  LayoutUnit b_left = info.include_left_edge ? obj.BorderLeft() : LayoutUnit();
  LayoutUnit b_right =
      info.include_right_edge ? obj.BorderRight() : LayoutUnit();
  LayoutUnit p_left = info.include_left_edge ? obj.PaddingLeft() : LayoutUnit();
  LayoutUnit p_right =
      info.include_right_edge ? obj.PaddingRight() : LayoutUnit();

  GraphicsContextStateSaver clip_with_scrolling_state_saver(
      context, info.is_clipped_with_local_scrolling);
  LayoutRect scrolled_paint_rect = rect;
  if (info.is_clipped_with_local_scrolling &&
      !IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          &obj, paint_info)) {
    // Clip to the overflow area.
    const LayoutBox& this_box = ToLayoutBox(obj);
    // TODO(chrishtr): this should be pixel-snapped.
    context.Clip(FloatRect(this_box.OverflowClipRect(rect.Location())));

    // Adjust the paint rect to reflect a scrolled content box with borders at
    // the ends.
    IntSize offset = this_box.ScrolledContentOffset();
    scrolled_paint_rect.Move(-offset);
    scrolled_paint_rect.SetWidth(b_left + this_box.ScrollWidth() + b_right);
    scrolled_paint_rect.SetHeight(this_box.BorderTop() +
                                  this_box.ScrollHeight() +
                                  this_box.BorderBottom());
  }

  GraphicsContextStateSaver background_clip_state_saver(context, false);
  IntRect mask_rect;

  switch (bg_layer.Clip()) {
    case kPaddingFillBox:
    case kContentFillBox: {
      if (info.is_rounded_fill)
        break;

      // Clip to the padding or content boxes as necessary.
      bool include_padding = bg_layer.Clip() == kContentFillBox;
      LayoutRect clip_rect(
          scrolled_paint_rect.X() + b_left +
              (include_padding ? p_left : LayoutUnit()),
          scrolled_paint_rect.Y() + obj.BorderTop() +
              (include_padding ? obj.PaddingTop() : LayoutUnit()),
          scrolled_paint_rect.Width() - b_left - b_right -
              (include_padding ? p_left + p_right : LayoutUnit()),
          scrolled_paint_rect.Height() - obj.BorderTop() - obj.BorderBottom() -
              (include_padding ? obj.PaddingTop() + obj.PaddingBottom()
                               : LayoutUnit()));
      background_clip_state_saver.Save();
      // TODO(chrishtr): this should be pixel-snapped.
      context.Clip(FloatRect(clip_rect));

      break;
    }
    case kTextFillBox: {
      // First figure out how big the mask has to be. It should be no bigger
      // than what we need to actually render, so we should intersect the dirty
      // rect with the border box of the background.
      mask_rect = PixelSnappedIntRect(rect);

      // We draw the background into a separate layer, to be later masked with
      // yet another layer holding the text content.
      background_clip_state_saver.Save();
      context.Clip(mask_rect);
      context.BeginLayer();

      break;
    }
    case kBorderFillBox:
      break;
    default:
      NOTREACHED();
      break;
  }

  // Paint the color first underneath all images, culled if background image
  // occludes it.
  // TODO(trchen): In the !bgLayer.hasRepeatXY() case, we could improve the
  // culling test by verifying whether the background image covers the entire
  // painting area.
  if (info.is_bottom_layer && info.color.Alpha() && info.should_paint_color) {
    IntRect background_rect(PixelSnappedIntRect(scrolled_paint_rect));
    context.FillRect(background_rect, info.color);
  }

  // no progressive loading of the background image
  if (info.should_paint_image) {
    if (!geometry) {
      geometry.emplace();
      geometry->Calculate(obj, background_object, paint_info.PaintContainer(),
                          paint_info.GetGlobalPaintFlags(), bg_layer,
                          scrolled_paint_rect);
    } else {
      // The geometry was calculated in paintFastBottomLayer().
      DCHECK(info.is_bottom_layer && info.is_border_fill &&
             !info.is_clipped_with_local_scrolling);
    }

    if (!geometry->DestRect().IsEmpty()) {
      const ImagePaintContext image_context(obj, context, bg_layer, *info.image,
                                            op, background_object,
                                            geometry->TileSize());
      TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
                   "data", InspectorPaintImageEvent::Data(obj, *info.image));
      context.DrawTiledImage(
          image_context.GetImage(), FloatRect(geometry->DestRect()),
          FloatPoint(geometry->Phase()), FloatSize(geometry->TileSize()),
          image_context.CompositeOp(), FloatSize(geometry->SpaceSize()));
    }
  }

  if (bg_layer.Clip() == kTextFillBox) {
    // Create the text mask layer.
    context.BeginLayer(1, SkBlendMode::kDstIn);

    // Now draw the text into the mask. We do this by painting using a special
    // paint phase that signals to
    // InlineTextBoxes that they should just add their contents to the clip.
    PaintInfo info(context, mask_rect, kPaintPhaseTextClip,
                   kGlobalPaintNormalPhase, 0);
    if (box) {
      const RootInlineBox& root = box->Root();
      box->Paint(info,
                 LayoutPoint(scrolled_paint_rect.X() - box->X(),
                             scrolled_paint_rect.Y() - box->Y()),
                 root.LineTop(), root.LineBottom());
    } else {
      // FIXME: this should only have an effect for the line box list within
      // |obj|. Change this to create a LineBoxListPainter directly.
      LayoutSize local_offset =
          obj.IsBox() ? ToLayoutBox(&obj)->LocationOffset() : LayoutSize();
      obj.Paint(info, scrolled_paint_rect.Location() - local_offset);
    }

    context.EndLayer();
    context.EndLayer();
  }
}

void BoxPainter::PaintMask(const PaintInfo& paint_info,
                           const LayoutPoint& paint_offset) {
  if (layout_box_.Style()->Visibility() != EVisibility::kVisible ||
      paint_info.phase != kPaintPhaseMask)
    return;

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_box_, paint_info.phase))
    return;

  LayoutRect visual_overflow_rect(layout_box_.VisualOverflowRect());
  visual_overflow_rect.MoveBy(paint_offset);

  LayoutObjectDrawingRecorder recorder(paint_info.context, layout_box_,
                                       paint_info.phase, visual_overflow_rect);
  LayoutRect paint_rect = LayoutRect(paint_offset, layout_box_.Size());
  PaintMaskImages(paint_info, paint_rect);
}

void BoxPainter::PaintMaskImages(const PaintInfo& paint_info,
                                 const LayoutRect& paint_rect) {
  // Figure out if we need to push a transparency layer to render our mask.
  bool push_transparency_layer = false;
  bool flatten_compositing_layers =
      paint_info.GetGlobalPaintFlags() & kGlobalPaintFlattenCompositingLayers;
  bool mask_blending_applied_by_compositor =
      !flatten_compositing_layers && layout_box_.HasLayer() &&
      layout_box_.Layer()->MaskBlendingAppliedByCompositor();

  bool all_mask_images_loaded = true;

  if (!mask_blending_applied_by_compositor) {
    push_transparency_layer = true;
    StyleImage* mask_box_image = layout_box_.Style()->MaskBoxImage().GetImage();
    const FillLayer& mask_layers = layout_box_.Style()->MaskLayers();

    // Don't render a masked element until all the mask images have loaded, to
    // prevent a flash of unmasked content.
    if (mask_box_image)
      all_mask_images_loaded &= mask_box_image->IsLoaded();

    all_mask_images_loaded &= mask_layers.ImagesAreLoaded();

    paint_info.context.BeginLayer(1, SkBlendMode::kDstIn);
  }

  if (all_mask_images_loaded) {
    PaintFillLayers(paint_info, Color::kTransparent,
                    layout_box_.Style()->MaskLayers(), paint_rect);
    PaintNinePieceImage(layout_box_, paint_info.context, paint_rect,
                        layout_box_.StyleRef(),
                        layout_box_.Style()->MaskBoxImage());
  }

  if (push_transparency_layer)
    paint_info.context.EndLayer();
}

void BoxPainter::PaintClippingMask(const PaintInfo& paint_info,
                                   const LayoutPoint& paint_offset) {
  DCHECK(paint_info.phase == kPaintPhaseClippingMask);

  if (layout_box_.Style()->Visibility() != EVisibility::kVisible)
    return;

  if (!layout_box_.Layer() ||
      layout_box_.Layer()->GetCompositingState() != kPaintsIntoOwnBacking)
    return;

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_box_, paint_info.phase))
    return;

  IntRect paint_rect =
      PixelSnappedIntRect(LayoutRect(paint_offset, layout_box_.Size()));
  LayoutObjectDrawingRecorder drawing_recorder(paint_info.context, layout_box_,
                                               paint_info.phase, paint_rect);
  paint_info.context.FillRect(paint_rect, Color::kBlack);
}

InterpolationQuality BoxPainter::ChooseInterpolationQuality(
    const LayoutObject& obj,
    Image* image,
    const void* layer,
    const LayoutSize& size) {
  return ImageQualityController::GetImageQualityController()
      ->ChooseInterpolationQuality(obj, image, layer, size);
}

bool BoxPainter::PaintNinePieceImage(const LayoutBoxModelObject& obj,
                                     GraphicsContext& graphics_context,
                                     const LayoutRect& rect,
                                     const ComputedStyle& style,
                                     const NinePieceImage& nine_piece_image,
                                     SkBlendMode op) {
  NinePieceImagePainter nine_piece_image_painter(obj);
  return nine_piece_image_painter.Paint(graphics_context, rect, style,
                                        nine_piece_image, op);
}

void BoxPainter::PaintBorder(const LayoutBoxModelObject& obj,
                             const PaintInfo& info,
                             const LayoutRect& rect,
                             const ComputedStyle& style,
                             BackgroundBleedAvoidance bleed_avoidance,
                             bool include_logical_left_edge,
                             bool include_logical_right_edge) {
  // border-image is not affected by border-radius.
  if (PaintNinePieceImage(obj, info.context, rect, style, style.BorderImage()))
    return;

  const BoxBorderPainter border_painter(rect, style, bleed_avoidance,
                                        include_logical_left_edge,
                                        include_logical_right_edge);
  border_painter.PaintBorder(info, rect);
}

void BoxPainter::PaintNormalBoxShadow(const PaintInfo& info,
                                      const LayoutRect& paint_rect,
                                      const ComputedStyle& style,
                                      bool include_logical_left_edge,
                                      bool include_logical_right_edge) {
  if (!style.BoxShadow())
    return;
  GraphicsContext& context = info.context;

  // https://bugs.chromium.org/p/skia/issues/detail?id=237
  if (context.Printing())
    return;

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

void BoxPainter::PaintInsetBoxShadow(const PaintInfo& info,
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

void BoxPainter::PaintInsetBoxShadowInBounds(const PaintInfo& info,
                                             const FloatRoundedRect& bounds,
                                             const ComputedStyle& style,
                                             bool include_logical_left_edge,
                                             bool include_logical_right_edge) {
  // The caller should have checked style.boxShadow() when computing bounds.
  DCHECK(style.BoxShadow());
  GraphicsContext& context = info.context;

  // https://bugs.chromium.org/p/skia/issues/detail?id=237
  if (context.Printing())
    return;

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

bool BoxPainter::ShouldForceWhiteBackgroundForPrintEconomy(
    const ComputedStyle& style,
    const Document& document) {
  return document.Printing() &&
         style.PrintColorAdjust() == EPrintColorAdjust::kEconomy &&
         (!document.GetSettings() ||
          !document.GetSettings()->GetShouldPrintBackgrounds());
}

}  // namespace blink
