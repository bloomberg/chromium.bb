// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxModelObjectPainter.h"

#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRectOutsets.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/wtf/Optional.h"

namespace blink {

bool BoxModelObjectPainter::
    IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
        const LayoutBoxModelObject* box_model_,
        const PaintInfo& paint_info) {
  return paint_info.PaintFlags() & kPaintLayerPaintingOverflowContents &&
         !(paint_info.PaintFlags() &
           kPaintLayerPaintingCompositingBackgroundPhase) &&
         box_model_ == paint_info.PaintContainer();
}

namespace {

class InterpolationQualityContext {
 public:
  InterpolationQualityContext(const ComputedStyle& style,
                              GraphicsContext& context)
      : context_(context),
        previous_interpolation_quality_(context.ImageInterpolationQuality()) {
    interpolation_quality_ = style.GetInterpolationQuality();
    if (interpolation_quality_ != previous_interpolation_quality_)
      context.SetImageInterpolationQuality(interpolation_quality_);
  }

  ~InterpolationQualityContext() {
    if (interpolation_quality_ != previous_interpolation_quality_)
      context_.SetImageInterpolationQuality(previous_interpolation_quality_);
  }

 private:
  GraphicsContext& context_;
  InterpolationQuality interpolation_quality_;
  InterpolationQuality previous_interpolation_quality_;
};

inline bool PaintFastBottomLayer(const DisplayItemClient& image_client,
                                 Node* node,
                                 const PaintInfo& paint_info,
                                 const BoxPainterBase::FillLayerInfo& info,
                                 const LayoutRect& rect,
                                 const FloatRoundedRect& border_rect,
                                 BackgroundImageGeometry& geometry,
                                 Image* image,
                                 SkBlendMode composite_op) {
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
  FloatRoundedRect border = info.is_rounded_fill
                                ? border_rect
                                : FloatRoundedRect(PixelSnappedIntRect(rect));
  Optional<RoundedInnerRectClipper> clipper;
  if (info.is_rounded_fill && !border.IsRenderable()) {
    // When the rrect is not renderable, we resort to clipping.
    // RoundedInnerRectClipper handles this case via discrete, corner-wise
    // clipping.
    clipper.emplace(image_client, paint_info, rect, border, kApplyToContext);
    border.SetRadii(FloatRoundedRect::Radii());
  }

  // Paint the color if needed.
  if (info.should_paint_color)
    context.FillRoundedRect(border, info.color);

  // Paint the image if needed.
  if (!info.should_paint_image || image_tile.IsEmpty())
    return true;

  if (!image)
    return true;

  const FloatSize intrinsic_tile_size =
      image->HasRelativeSize() ? image_tile.Size() : FloatSize(image->Size());
  const FloatRect src_rect = Image::ComputeSubsetForTile(
      image_tile, border.Rect(), intrinsic_tile_size);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
               "data", InspectorPaintImageEvent::Data(node, *info.image));
  context.DrawImageRRect(image, border, src_rect, composite_op);

  return true;
}

void PaintFillLayerBackground(GraphicsContext& context,
                              const BoxPainterBase::FillLayerInfo& info,
                              Image* image,
                              SkBlendMode composite_op,
                              const BackgroundImageGeometry& geometry,
                              Node* node,
                              LayoutRect scrolled_paint_rect) {
  // Paint the color first underneath all images, culled if background image
  // occludes it.
  // TODO(trchen): In the !bgLayer.hasRepeatXY() case, we could improve the
  // culling test by verifying whether the background image covers the entire
  // painting area.
  if (info.is_bottom_layer && info.color.Alpha() && info.should_paint_color) {
    IntRect background_rect(PixelSnappedIntRect(scrolled_paint_rect));
    context.FillRect(background_rect, info.color);
  }

  // No progressive loading of the background image.
  if (info.should_paint_image && !geometry.DestRect().IsEmpty()) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
                 "data", InspectorPaintImageEvent::Data(node, *info.image));
    context.DrawTiledImage(image, FloatRect(geometry.DestRect()),
                           FloatPoint(geometry.Phase()),
                           FloatSize(geometry.TileSize()), composite_op,
                           FloatSize(geometry.SpaceSize()));
  }
}

void PaintFillLayerTextFillBox(GraphicsContext& context,
                               const BoxPainterBase::FillLayerInfo& info,
                               Image* image,
                               SkBlendMode composite_op,
                               const BackgroundImageGeometry& geometry,
                               Node* node,
                               const LayoutRect& rect,
                               LayoutRect scrolled_paint_rect,
                               const LayoutBoxModelObject& box_model_,
                               const InlineFlowBox* box) {
  // First figure out how big the mask has to be. It should be no bigger
  // than what we need to actually render, so we should intersect the dirty
  // rect with the border box of the background.
  IntRect mask_rect = PixelSnappedIntRect(rect);

  // We draw the background into a separate layer, to be later masked with
  // yet another layer holding the text content.
  GraphicsContextStateSaver background_clip_state_saver(context, false);
  background_clip_state_saver.Save();
  context.Clip(mask_rect);
  context.BeginLayer();

  PaintFillLayerBackground(context, info, image, composite_op, geometry,
                           box_model_.GeneratingNode(), scrolled_paint_rect);

  // Create the text mask layer and draw the text into the mask. We do this by
  // painting using a special paint phase that signals to InlineTextBoxes that
  // they should just add their contents to the clip.
  context.BeginLayer(1, SkBlendMode::kDstIn);
  PaintInfo paint_info(context, mask_rect, kPaintPhaseTextClip,
                       kGlobalPaintNormalPhase, 0);
  if (box) {
    const RootInlineBox& root = box->Root();
    box->Paint(paint_info,
               LayoutPoint(scrolled_paint_rect.X() - box->X(),
                           scrolled_paint_rect.Y() - box->Y()),
               root.LineTop(), root.LineBottom());
  } else {
    // FIXME: this should only have an effect for the line box list within
    // |box_model_|. Change this to create a LineBoxListPainter directly.
    LayoutSize local_offset = box_model_.IsBox()
                                  ? ToLayoutBox(&box_model_)->LocationOffset()
                                  : LayoutSize();
    box_model_.Paint(paint_info, scrolled_paint_rect.Location() - local_offset);
  }

  context.EndLayer();  // Text mask layer.
  context.EndLayer();  // Background layer.
}

}  // anonymous namespace

LayoutRectOutsets BoxModelObjectPainter::BorderOutsets(
    const BoxPainterBase::FillLayerInfo& info) const {
  return LayoutRectOutsets(
      box_model_.BorderTop(),
      info.include_right_edge ? box_model_.BorderRight() : LayoutUnit(),
      box_model_.BorderBottom(),
      info.include_left_edge ? box_model_.BorderLeft() : LayoutUnit());
}

LayoutRectOutsets BoxModelObjectPainter::PaddingOutsets(
    const BoxPainterBase::FillLayerInfo& info) const {
  return LayoutRectOutsets(
      box_model_.PaddingTop(),
      info.include_right_edge ? box_model_.PaddingRight() : LayoutUnit(),
      box_model_.PaddingBottom(),
      info.include_left_edge ? box_model_.PaddingLeft() : LayoutUnit());
}

void BoxModelObjectPainter::PaintFillLayer(
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
      box_model_.GetDocument(), box_model_.StyleRef(),
      box_model_.HasOverflowClip(), color, bg_layer, bleed_avoidance,
      (box ? box->IncludeLogicalLeftEdge() : true),
      (box ? box->IncludeLogicalRightEdge() : true));

  bool has_line_box_sibling = box && (box->NextLineBox() || box->PrevLineBox());
  LayoutRectOutsets border = BorderOutsets(info);

  GraphicsContextStateSaver clip_with_scrolling_state_saver(
      context, info.is_clipped_with_local_scrolling);
  LayoutRect scrolled_paint_rect = rect;
  if (info.is_clipped_with_local_scrolling &&
      !IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
          &box_model_, paint_info)) {
    // Clip to the overflow area.
    const LayoutBox& this_box = ToLayoutBox(box_model_);
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

  RefPtr<Image> image;
  SkBlendMode composite_op = op;
  Optional<InterpolationQualityContext> interpolation_quality_context;
  if (info.should_paint_image) {
    geometry.Calculate(paint_info.PaintContainer(),
                       paint_info.GetGlobalPaintFlags(), bg_layer,
                       scrolled_paint_rect);
    image = info.image->GetImage(
        geometry.ImageClient(), geometry.ImageDocument(), geometry.ImageStyle(),
        FlooredIntSize(geometry.TileSize()));
    interpolation_quality_context.emplace(geometry.ImageStyle(), context);

    if (bg_layer.MaskSourceType() == kMaskLuminance)
      context.SetColorFilter(kColorFilterLuminanceToAlpha);

    // If op != SkBlendMode::kSrcOver, a mask is being painted.
    SkBlendMode bg_op = WebCoreCompositeToSkiaComposite(bg_layer.Composite(),
                                                        bg_layer.BlendMode());
    composite_op = (op == SkBlendMode::kSrcOver) ? bg_op : op;
  }

  FloatRoundedRect border_rect = RoundedBorderRectForClip(
      box_model_.StyleRef(), info, bg_layer, rect, bleed_avoidance,
      has_line_box_sibling, box_size, box_model_.BorderPaddingInsets());

  // Fast path for drawing simple color backgrounds.
  if (PaintFastBottomLayer(box_model_, box_model_.GeneratingNode(), paint_info,
                           info, rect, border_rect, geometry, image.Get(),
                           composite_op)) {
    return;
  }

  Optional<RoundedInnerRectClipper> clip_to_border;
  if (info.is_rounded_fill) {
    clip_to_border.emplace(box_model_, paint_info, rect, border_rect,
                           kApplyToContext);
  }
  if (bg_layer.Clip() == kTextFillBox) {
    PaintFillLayerTextFillBox(context, info, image.Get(), composite_op,
                              geometry, box_model_.GeneratingNode(), rect,
                              scrolled_paint_rect, box_model_, box);
    return;
  }

  GraphicsContextStateSaver background_clip_state_saver(context, false);
  switch (bg_layer.Clip()) {
    case kPaddingFillBox:
    case kContentFillBox: {
      if (info.is_rounded_fill)
        break;

      // Clip to the padding or content boxes as necessary.
      LayoutRect clip_rect = scrolled_paint_rect;
      clip_rect.Contract(border);
      if (bg_layer.Clip() == kContentFillBox)
        clip_rect.Contract(PaddingOutsets(info));
      background_clip_state_saver.Save();
      // TODO(chrishtr): this should be pixel-snapped.
      context.Clip(FloatRect(clip_rect));
      break;
    }
    case kBorderFillBox:
      break;
    case kTextFillBox:  // fall through
    default:
      NOTREACHED();
      break;
  }

  PaintFillLayerBackground(context, info, image.Get(), composite_op, geometry,
                           box_model_.GeneratingNode(), scrolled_paint_rect);
}

Node* BoxModelObjectPainter::GetNode() const {
  Node* node = nullptr;
  const LayoutObject* layout_object = &box_model_;
  for (; layout_object && !node; layout_object = layout_object->Parent()) {
    node = layout_object->GeneratingNode();
  }
  return node;
}

}  // namespace blink
