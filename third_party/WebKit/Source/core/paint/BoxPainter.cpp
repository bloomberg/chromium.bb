// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxPainter.h"

#include "core/HTMLNames.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/BoxDecorationData.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/NinePieceImagePainter.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/ScrollRecorder.h"
#include "core/paint/ThemePainter.h"
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
    paint_rect.Expand(layout_box_.BorderBoxOutsets());
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
  Optional<DisplayItemCacheSkipper> cache_skipper;
  // Disable cache in under-invalidation checking mode for MediaSliderPart
  // because we always paint using the latest data (buffered ranges, current
  // time and duration) which may be different from the cached data.
  if ((RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
       layout_box_.StyleRef().Appearance() == kMediaSliderPart)
      // We may paint a delayed-invalidation object before it's actually
      // invalidated. Note this would be handled for us by
      // LayoutObjectDrawingRecorder but we have to use DrawingRecorder as we
      // may use the scrolling contents layer as DisplayItemClient below.
      || layout_box_.FullPaintInvalidationReason() ==
             PaintInvalidationReason::kDelayedFull) {
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
    PaintNormalBoxShadow(paint_info, paint_rect, layout_box_.StyleRef());

    if (BleedAvoidanceIsClipping(box_decoration_data.bleed_avoidance)) {
      state_saver.Save();
      FloatRoundedRect border =
          layout_box_.StyleRef().GetRoundedBorderFor(paint_rect);
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
    PaintInsetBoxShadow(paint_info, paint_rect, layout_box_.StyleRef());

    // The theme will tell us whether or not we should also paint the CSS
    // border.
    if (box_decoration_data.has_border_decoration &&
        (!box_decoration_data.has_appearance ||
         (!theme_painted &&
          LayoutTheme::GetTheme().Painter().PaintBorderOnly(
              layout_box_, paint_info, snapped_paint_rect))) &&
        !(layout_box_.IsTable() &&
          ToLayoutTable(&layout_box_)->ShouldCollapseBorders())) {
      PaintBorder(layout_box_, layout_box_.GetDocument(), GetNode(), paint_info,
                  paint_rect, layout_box_.StyleRef(),
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
  BackgroundImageGeometry geometry(layout_box_);
  PaintFillLayers(paint_info, background_color,
                  layout_box_.Style()->BackgroundLayers(), paint_rect, geometry,
                  bleed_avoidance);
}

void BoxPainter::PaintFillLayers(const PaintInfo& paint_info,
                                 const Color& c,
                                 const FillLayer& fill_layer,
                                 const LayoutRect& rect,
                                 BackgroundImageGeometry& geometry,
                                 BackgroundBleedAvoidance bleed_avoidance,
                                 SkBlendMode op) {
  FillLayerOcclusionOutputList reversed_paint_list;
  bool should_draw_background_in_separate_buffer =
      CalculateFillLayerOcclusionCulling(reversed_paint_list, fill_layer,
                                         layout_box_.GetDocument(),
                                         layout_box_.StyleRef());

  // TODO(trchen): We can optimize out isolation group if we have a
  // non-transparent background color and the bottom layer encloses all other
  // layers.
  GraphicsContext& context = paint_info.context;
  if (should_draw_background_in_separate_buffer)
    context.BeginLayer();

  for (auto it = reversed_paint_list.rbegin(); it != reversed_paint_list.rend();
       ++it) {
    PaintFillLayer(layout_box_, paint_info, c, **it, rect, bleed_avoidance,
                   geometry, 0, LayoutSize(), op);
  }

  if (should_draw_background_in_separate_buffer)
    context.EndLayer();
}

namespace {

class ImagePaintContext {
 public:
  ImagePaintContext(const ImageResourceObserver& image_client,
                    const Document& document,
                    const ComputedStyle& style,
                    GraphicsContext& context,
                    const FillLayer& layer,
                    const StyleImage& style_image,
                    SkBlendMode op,
                    const LayoutSize& container_size)
      : context_(context),
        previous_interpolation_quality_(context.ImageInterpolationQuality()) {
    SkBlendMode bg_op =
        WebCoreCompositeToSkiaComposite(layer.Composite(), layer.BlendMode());
    // if op != SkBlendMode::kSrcOver, a mask is being painted.
    composite_op_ = (op == SkBlendMode::kSrcOver) ? bg_op : op;

    image_ = style_image.GetImage(image_client, document, style,
                                  FlooredIntSize(container_size));
    interpolation_quality_ = style.GetInterpolationQuality();
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
                                 const BoxPainterBase::FillLayerInfo& info,
                                 const LayoutRect& rect,
                                 BackgroundBleedAvoidance bleed_avoidance,
                                 const LayoutSize& box_size,
                                 SkBlendMode op,
                                 BackgroundImageGeometry& geometry,
                                 Optional<ImagePaintContext>& image_context,
                                 bool has_line_box_sibling) {
  // Painting a background image from an ancestor onto a cell is a complex case.
  if (geometry.CellUsingContainerBackground())
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

    if (!geometry.DestRect().IsEmpty()) {
      // The tile is too small.
      if (geometry.TileSize().Width() < rect.Width() ||
          geometry.TileSize().Height() < rect.Height())
        return false;

      image_tile = Image::ComputeTileContaining(
          FloatPoint(geometry.DestRect().Location()),
          FloatSize(geometry.TileSize()), FloatPoint(geometry.Phase()),
          FloatSize(geometry.SpaceSize()));

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
          ? BoxPainterBase::BackgroundRoundedRectAdjustedForBleedAvoidance(
                obj.StyleRef(), rect, bleed_avoidance, has_line_box_sibling,
                box_size, info.include_left_edge, info.include_right_edge)
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

  if (!image_context || !image_context->GetImage())
    return true;

  const FloatSize intrinsic_tile_size =
      image_context->GetImage()->HasRelativeSize()
          ? image_tile.Size()
          : FloatSize(image_context->GetImage()->Size());
  const FloatRect src_rect = Image::ComputeSubsetForTile(
      image_tile, border.Rect(), intrinsic_tile_size);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
               "data", InspectorPaintImageEvent::Data(obj, *info.image));
  context.DrawImageRRect(image_context->GetImage(), border, src_rect,
                         image_context->CompositeOp());

  return true;
}

}  // anonymous namespace

void BoxPainter::PaintFillLayer(const LayoutBoxModelObject& obj,
                                const PaintInfo& paint_info,
                                const Color& color,
                                const FillLayer& bg_layer,
                                const LayoutRect& rect,
                                BackgroundBleedAvoidance bleed_avoidance,
                                BackgroundImageGeometry& geometry,
                                const InlineFlowBox* box,
                                const LayoutSize& box_size,
                                SkBlendMode op) {
  GraphicsContext& context = paint_info.context;
  if (rect.IsEmpty())
    return;

  const BoxPainterBase::FillLayerInfo info(
      obj.GetDocument(), obj.StyleRef(), obj.HasOverflowClip(), color, bg_layer,
      bleed_avoidance, (box ? box->IncludeLogicalLeftEdge() : true),
      (box ? box->IncludeLogicalRightEdge() : true));

  bool has_line_box_sibling = box && (box->NextLineBox() || box->PrevLineBox());
  LayoutRectOutsets border(
      obj.BorderTop(),
      info.include_right_edge ? obj.BorderRight() : LayoutUnit(),
      obj.BorderBottom(),
      info.include_left_edge ? obj.BorderLeft() : LayoutUnit());

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
    scrolled_paint_rect.SetWidth(border.Left() + this_box.ScrollWidth() +
                                 border.Right());
    scrolled_paint_rect.SetHeight(this_box.BorderTop() +
                                  this_box.ScrollHeight() +
                                  this_box.BorderBottom());
  }

  Optional<ImagePaintContext> image_context;
  if (info.should_paint_image) {
    geometry.Calculate(paint_info.PaintContainer(),
                       paint_info.GetGlobalPaintFlags(), bg_layer,
                       scrolled_paint_rect);
    image_context.emplace(geometry.ImageClient(), geometry.ImageDocument(),
                          geometry.ImageStyle(), context, bg_layer, *info.image,
                          op, geometry.TileSize());
  }

  // Fast path for drawing simple color backgrounds.
  if (PaintFastBottomLayer(obj, paint_info, info, rect, bleed_avoidance,
                           box_size, op, geometry, image_context,
                           has_line_box_sibling)) {
    return;
  }

  Optional<RoundedInnerRectClipper> clip_to_border;
  if (info.is_rounded_fill) {
    FloatRoundedRect border = RoundedBorderRectForClip(
        obj.StyleRef(), info, bg_layer, rect, bleed_avoidance,
        has_line_box_sibling, box_size, obj.BorderPaddingInsets());
    clip_to_border.emplace(obj, paint_info, rect, border, kApplyToContext);
  }

  GraphicsContextStateSaver background_clip_state_saver(context, false);
  IntRect mask_rect;

  switch (bg_layer.Clip()) {
    case kPaddingFillBox:
    case kContentFillBox: {
      if (info.is_rounded_fill)
        break;

      // Clip to the padding or content boxes as necessary.
      LayoutRect clip_rect = scrolled_paint_rect;
      clip_rect.Contract(border);
      if (bg_layer.Clip() == kContentFillBox) {
        LayoutRectOutsets padding(
            obj.PaddingTop(),
            info.include_right_edge ? obj.PaddingRight() : LayoutUnit(),
            obj.PaddingBottom(),
            info.include_left_edge ? obj.PaddingLeft() : LayoutUnit());
        clip_rect.Contract(padding);
      }
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
  if (info.should_paint_image && !geometry.DestRect().IsEmpty()) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
                 "data", InspectorPaintImageEvent::Data(obj, *info.image));
    context.DrawTiledImage(
        image_context->GetImage(), FloatRect(geometry.DestRect()),
        FloatPoint(geometry.Phase()), FloatSize(geometry.TileSize()),
        image_context->CompositeOp(), FloatSize(geometry.SpaceSize()));
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
    BackgroundImageGeometry geometry(layout_box_);
    PaintFillLayers(paint_info, Color::kTransparent,
                    layout_box_.Style()->MaskLayers(), paint_rect, geometry);
    NinePieceImagePainter::Paint(paint_info.context, layout_box_,
                                 layout_box_.GetDocument(), GetNode(),
                                 paint_rect, layout_box_.StyleRef(),
                                 layout_box_.StyleRef().MaskBoxImage());
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

Node* BoxPainter::GetNode() {
  Node* node = nullptr;
  const LayoutObject* layout_object = &layout_box_;
  for (; layout_object && !node; layout_object = layout_object->Parent()) {
    node = layout_object->GeneratingNode();
  }
  return node;
}

}  // namespace blink
