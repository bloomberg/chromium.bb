/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "core/paint/PaintLayerClipper.h"

#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutView.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/paint/GeometryMapper.h"

namespace blink {

static bool HasOverflowClip(
    const PaintLayer& layer) {
  if (!layer.GetLayoutObject().IsBox())
    return false;
  const LayoutBox& box = ToLayoutBox(layer.GetLayoutObject());
  return box.ShouldClipOverflow();
}

bool ClipRectsContext::ShouldRespectRootLayerClip() const {
  if (respect_overflow_clip == kIgnoreOverflowClip)
    return false;

  if (root_layer->IsRootLayer() &&
      respect_overflow_clip_for_viewport == kIgnoreOverflowClip)
    return false;

  return true;
}

static void AdjustClipRectsForChildren(
    const LayoutBoxModelObject& layout_object,
    ClipRects& clip_rects) {
  EPosition position = layout_object.StyleRef().GetPosition();
  // A fixed object is essentially the root of its containing block hierarchy,
  // so when we encounter such an object, we reset our clip rects to the
  // fixedClipRect.
  if (position == EPosition::kFixed) {
    clip_rects.SetPosClipRect(clip_rects.FixedClipRect());
    clip_rects.SetOverflowClipRect(clip_rects.FixedClipRect());
    clip_rects.SetFixed(true);
  } else if (position == EPosition::kRelative) {
    clip_rects.SetPosClipRect(clip_rects.OverflowClipRect());
  } else if (position == EPosition::kAbsolute) {
    clip_rects.SetOverflowClipRect(clip_rects.PosClipRect());
  }
}

static void ApplyClipRects(const ClipRectsContext& context,
                           const LayoutBoxModelObject& layout_object,
                           LayoutPoint offset,
                           ClipRects& clip_rects) {
  DCHECK(layout_object.IsBox());
  const LayoutBox& box = *ToLayoutBox(&layout_object);

  DCHECK(box.ShouldClipOverflow() || box.HasClip());
  LayoutView* view = box.View();
  DCHECK(view);
  if (clip_rects.Fixed() && &context.root_layer->GetLayoutObject() == view)
    offset -= LayoutSize(view->GetFrameView()->GetScrollOffset());

  if (box.ShouldClipOverflow()) {
    ClipRect new_overflow_clip =
        box.OverflowClipRect(offset, context.overlay_scrollbar_clip_behavior);
    new_overflow_clip.SetHasRadius(box.StyleRef().HasBorderRadius());
    clip_rects.SetOverflowClipRect(
        Intersection(new_overflow_clip, clip_rects.OverflowClipRect()));
    if (box.IsPositioned())
      clip_rects.SetPosClipRect(
          Intersection(new_overflow_clip, clip_rects.PosClipRect()));
    if (box.CanContainFixedPositionObjects())
      clip_rects.SetFixedClipRect(
          Intersection(new_overflow_clip, clip_rects.FixedClipRect()));
    if (box.StyleRef().ContainsPaint())
      clip_rects.SetPosClipRect(
          Intersection(new_overflow_clip, clip_rects.PosClipRect()));
  }
  if (box.HasClip()) {
    LayoutRect new_clip = box.ClipRect(offset);
    clip_rects.SetPosClipRect(Intersection(new_clip, clip_rects.PosClipRect()));
    clip_rects.SetOverflowClipRect(
        Intersection(new_clip, clip_rects.OverflowClipRect()));
    clip_rects.SetFixedClipRect(
        Intersection(new_clip, clip_rects.FixedClipRect()));
  }
}

PaintLayerClipper::PaintLayerClipper(const PaintLayer& layer,
                                     bool usegeometry_mapper)
    : layer_(layer), use_geometry_mapper_(usegeometry_mapper) {}

ClipRects* PaintLayerClipper::ClipRectsIfCached(
    const ClipRectsContext& context) const {
  DCHECK(context.UsesCache());
  if (!layer_.GetClipRectsCache())
    return nullptr;
  ClipRectsCache::Entry& entry =
      layer_.GetClipRectsCache()->Get(context.CacheSlot());
  // FIXME: We used to ASSERT that we always got a consistent root layer.
  // We should add a test that has an inconsistent root. See
  // http://crbug.com/366118 for an example.
  if (context.root_layer != entry.root)
    return 0;
#if DCHECK_IS_ON()
  DCHECK(entry.overlay_scrollbar_clip_behavior ==
         context.overlay_scrollbar_clip_behavior);
#endif
  return entry.clip_rects.Get();
}

ClipRects& PaintLayerClipper::StoreClipRectsInCache(
    const ClipRectsContext& context,
    ClipRects* parent_clip_rects,
    const ClipRects& clip_rects) const {
  ClipRectsCache::Entry& entry =
      layer_.EnsureClipRectsCache().Get(context.CacheSlot());
  entry.root = context.root_layer;
#if DCHECK_IS_ON()
  entry.overlay_scrollbar_clip_behavior =
      context.overlay_scrollbar_clip_behavior;
#endif
  if (parent_clip_rects) {
    // If our clip rects match the clip rects of our parent, we share storage.
    if (clip_rects == *parent_clip_rects) {
      entry.clip_rects = parent_clip_rects;
      return *parent_clip_rects;
    }
  }
  entry.clip_rects = ClipRects::Create(clip_rects);
  return *entry.clip_rects;
}

ClipRects& PaintLayerClipper::GetClipRects(
    const ClipRectsContext& context) const {
  DCHECK(!use_geometry_mapper_);
  if (ClipRects* result = ClipRectsIfCached(context))
    return *result;
  // Note that it's important that we call getClipRects on our parent
  // before we call calculateClipRects so that calculateClipRects will hit
  // the cache.
  ClipRects* parent_clip_rects = nullptr;
  if (context.root_layer != &layer_ && layer_.Parent()) {
    parent_clip_rects =
        &PaintLayerClipper(*layer_.Parent(), false).GetClipRects(context);
  }
  RefPtr<ClipRects> clip_rects = ClipRects::Create();
  CalculateClipRects(context, *clip_rects);
  return StoreClipRectsInCache(context, parent_clip_rects, *clip_rects);
}

void PaintLayerClipper::ClearCache(ClipRectsCacheSlot cache_slot) {
  if (cache_slot == kNumberOfClipRectsCacheSlots)
    layer_.ClearClipRectsCache();
  else if (ClipRectsCache* cache = layer_.GetClipRectsCache())
    cache->Clear(cache_slot);
}

void PaintLayerClipper::ClearClipRectsIncludingDescendants() {
  ClearClipRectsIncludingDescendants(kNumberOfClipRectsCacheSlots);
}

void PaintLayerClipper::ClearClipRectsIncludingDescendants(
    ClipRectsCacheSlot cache_slot) {
  std::stack<const PaintLayer*> layers;
  layers.push(&layer_);

  while (!layers.empty()) {
    const PaintLayer* current_layer = layers.top();
    layers.pop();
    PaintLayerClipper(*current_layer, use_geometry_mapper_)
        .ClearCache(cache_slot);
    for (const PaintLayer* layer = current_layer->FirstChild(); layer;
         layer = layer->NextSibling())
      layers.push(layer);
  }
}

LayoutRect PaintLayerClipper::LocalClipRect(
    const PaintLayer& clipping_root_layer) const {
  ClipRectsContext context(&clipping_root_layer, kPaintingClipRects);
  if (use_geometry_mapper_) {
    ClipRect clip_rect;
    CalculateBackgroundClipRectWithGeometryMapper(context, clip_rect);
    LayoutRect premapped_rect = clip_rect.Rect();

    // The rect now needs to be transformed to the local space of this
    // PaintLayer.
    premapped_rect.MoveBy(context.root_layer->GetLayoutObject().PaintOffset());

    const auto* clip_root_layer_transform =
        clipping_root_layer.GetLayoutObject()
            .FirstFragment()
            ->LocalBorderBoxProperties()
            ->Transform();
    const auto* layer_transform = layer_.GetLayoutObject()
                                      .FirstFragment()
                                      ->LocalBorderBoxProperties()
                                      ->Transform();
    FloatRect clipped_rect_in_local_space(premapped_rect);
    GeometryMapper::SourceToDestinationRect(clip_root_layer_transform,
                                            layer_transform,
                                            clipped_rect_in_local_space);
    clipped_rect_in_local_space.MoveBy(
        -FloatPoint(layer_.GetLayoutObject().PaintOffset()));

    return LayoutRect(clipped_rect_in_local_space);
  }

  LayoutRect layer_bounds;
  ClipRect background_rect, foreground_rect;
  CalculateRects(context, LayoutRect(LayoutRect::InfiniteIntRect()),
                 layer_bounds, background_rect, foreground_rect);

  LayoutRect clip_rect = background_rect.Rect();
  // TODO(chrishtr): avoid converting to IntRect and back.
  if (clip_rect == LayoutRect(LayoutRect::InfiniteIntRect()))
    return clip_rect;

  LayoutPoint clipping_root_offset;
  layer_.ConvertToLayerCoords(&clipping_root_layer, clipping_root_offset);
  clip_rect.MoveBy(-clipping_root_offset);

  return clip_rect;
}

void PaintLayerClipper::CalculateRectsWithGeometryMapper(
    const ClipRectsContext& context,
    const LayoutRect& paint_dirty_rect,
    LayoutRect& layer_bounds,
    ClipRect& background_rect,
    ClipRect& foreground_rect,
    const LayoutPoint* offset_from_root) const {
  LayoutPoint offset(context.sub_pixel_accumulation);
  if (offset_from_root)
    offset = *offset_from_root;
  else
    layer_.ConvertToLayerCoords(context.root_layer, offset);
  layer_bounds = LayoutRect(offset, LayoutSize(layer_.size()));

  // TODO(chrishtr): fix the underlying bug that causes this situation.
  if (!layer_.GetLayoutObject().FirstFragment()->PaintProperties() &&
      !layer_.GetLayoutObject().FirstFragment()->LocalBorderBoxProperties()) {
    background_rect = ClipRect(LayoutRect(LayoutRect::InfiniteIntRect()));
    foreground_rect = ClipRect(LayoutRect(LayoutRect::InfiniteIntRect()));
  } else {
    CalculateBackgroundClipRectWithGeometryMapper(context, background_rect);
    background_rect.Intersect(paint_dirty_rect);

    foreground_rect = background_rect;

    LayoutBoxModelObject& layout_object = layer_.GetLayoutObject();
    if (layout_object.HasClip()) {
      LayoutRect new_pos_clip = ToLayoutBox(layout_object).ClipRect(offset);
      foreground_rect.Intersect(new_pos_clip);
    }
    if (ShouldClipOverflow(context)) {
      LayoutRect overflow_and_clip_rect =
          ToLayoutBox(layout_object)
              .OverflowClipRect(offset,
                                context.overlay_scrollbar_clip_behavior);
      foreground_rect.Intersect(overflow_and_clip_rect);
      if (layout_object.StyleRef().HasBorderRadius())
        foreground_rect.SetHasRadius(true);
    }
  }
}

void PaintLayerClipper::CalculateRects(
    const ClipRectsContext& context,
    const LayoutRect& paint_dirty_rect,
    LayoutRect& layer_bounds,
    ClipRect& background_rect,
    ClipRect& foreground_rect,
    const LayoutPoint* offset_from_root) const {
  if (use_geometry_mapper_) {
    CalculateRectsWithGeometryMapper(context, paint_dirty_rect, layer_bounds,
                                     background_rect, foreground_rect,
                                     offset_from_root);
    return;
  }

  bool is_clipping_root = &layer_ == context.root_layer;
  LayoutBoxModelObject& layout_object = layer_.GetLayoutObject();

  if (!is_clipping_root && layer_.Parent()) {
    CalculateBackgroundClipRect(context, background_rect);
    background_rect.Move(context.sub_pixel_accumulation);
    background_rect.Intersect(paint_dirty_rect);
  } else {
    background_rect = paint_dirty_rect;
  }

  foreground_rect = background_rect;

  LayoutPoint offset(context.sub_pixel_accumulation);
  if (offset_from_root)
    offset = *offset_from_root;
  else
    layer_.ConvertToLayerCoords(context.root_layer, offset);
  layer_bounds = LayoutRect(offset, LayoutSize(layer_.size()));

  // Update the clip rects that will be passed to child layers.
  if (ShouldClipOverflow(context)) {
    LayoutRect overflow_and_clip_rect =
        ToLayoutBox(layout_object)
            .OverflowClipRect(offset, context.overlay_scrollbar_clip_behavior);
    foreground_rect.Intersect(overflow_and_clip_rect);
    if (layout_object.StyleRef().HasBorderRadius())
      foreground_rect.SetHasRadius(true);

    // FIXME: Does not do the right thing with columns yet, since we don't yet
    // factor in the individual column boxes as overflow.

    LayoutRect layer_bounds_with_visual_overflow = LocalVisualRect();
    layer_bounds_with_visual_overflow.MoveBy(offset);
    background_rect.Intersect(layer_bounds_with_visual_overflow);
  }

  // CSS clip (different than clipping due to overflow) can clip to any box,
  // even if it falls outside of the border box.
  if (layout_object.HasClip()) {
    // Clip applies to *us* as well, so go ahead and update the damageRect.
    LayoutRect new_pos_clip = ToLayoutBox(layout_object).ClipRect(offset);
    background_rect.Intersect(new_pos_clip);
    foreground_rect.Intersect(new_pos_clip);
  }
}

void PaintLayerClipper::CalculateClipRects(const ClipRectsContext& context,
                                           ClipRects& clip_rects) const {
  const LayoutBoxModelObject& layout_object = layer_.GetLayoutObject();
  if (!layer_.Parent() &&
      !RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    // The root layer's clip rect is always infinite.
    clip_rects.Reset(LayoutRect(LayoutRect::InfiniteIntRect()));
    return;
  }

  bool is_clipping_root = &layer_ == context.root_layer;

  if (is_clipping_root && !context.ShouldRespectRootLayerClip()) {
    clip_rects.Reset(LayoutRect(LayoutRect::InfiniteIntRect()));
    if (layout_object.StyleRef().GetPosition() == EPosition::kFixed)
      clip_rects.SetFixed(true);
    return;
  }

  // For transformed layers, the root layer was shifted to be us, so there is no
  // need to examine the parent. We want to cache clip rects with us as the
  // root.
  PaintLayer* parent_layer = !is_clipping_root ? layer_.Parent() : nullptr;
  // Ensure that our parent's clip has been calculated so that we can examine
  // the values.
  if (parent_layer) {
    PaintLayerClipper(*parent_layer, use_geometry_mapper_)
        .GetOrCalculateClipRects(context, clip_rects);
  } else {
    clip_rects.Reset(LayoutRect(LayoutRect::InfiniteIntRect()));
  }

  AdjustClipRectsForChildren(layout_object, clip_rects);

  // Computing paint offset is expensive, skip the computation if the object
  // is known to have no clip. This check is redundant otherwise.
  if (HasOverflowClip(layer_) || layout_object.HasClip()) {
    // This offset cannot use convertToLayerCoords, because sometimes our
    // rootLayer may be across some transformed layer boundary, for example, in
    // the PaintLayerCompositor overlapMap, where clipRects are needed in view
    // space.
    ApplyClipRects(context, layout_object,
                   LayoutPoint(layout_object.LocalToAncestorPoint(
                       FloatPoint(), &context.root_layer->GetLayoutObject())),
                   clip_rects);
  }
}

static ClipRect BackgroundClipRectForPosition(const ClipRects& parent_rects,
                                              EPosition position) {
  if (position == EPosition::kFixed)
    return parent_rects.FixedClipRect();

  if (position == EPosition::kAbsolute)
    return parent_rects.PosClipRect();

  return parent_rects.OverflowClipRect();
}

void PaintLayerClipper::CalculateBackgroundClipRectWithGeometryMapper(
    const ClipRectsContext& context,
    ClipRect& output) const {
  DCHECK(use_geometry_mapper_);

  bool is_clipping_root = &layer_ == context.root_layer;
  if (is_clipping_root && !context.ShouldRespectRootLayerClip()) {
    output.SetRect(FloatClipRect());
    return;
  }

  PropertyTreeState source_property_tree_state(nullptr, nullptr, nullptr);
  PropertyTreeState destination_property_tree_state(nullptr, nullptr, nullptr);
  InitializeCommonClipRectState(context, source_property_tree_state,
                                destination_property_tree_state);

  // The background rect applies all clips *above* m_layer, but not the overflow
  // clip of m_layer. It also applies a clip to the total painting bounds
  // of m_layer, because nothing in m_layer or its children within the clip can
  // paint outside of those bounds.
  // The total painting bounds includes any visual overflow (such as shadow) and
  // filter bounds.
  //
  // TODO(chrishtr): sourceToDestinationVisualRect and
  // sourceToDestinationClipRect may not compute tight results in the presence
  // of transforms. Tight results are required for most use cases of these
  // rects, so we should add methods to GeometryMapper that guarantee there
  // are tight results, or else signal an error.
  if (HasOverflowClip(layer_)) {
    FloatClipRect clip_rect((FloatRect(LocalVisualRect())));
    clip_rect.MoveBy(FloatPoint(layer_.GetLayoutObject().PaintOffset()));
    GeometryMapper::LocalToAncestorVisualRect(
        source_property_tree_state, destination_property_tree_state, clip_rect);
    output.SetRect(clip_rect);
  } else {
    const FloatClipRect& clipped_rect_in_root_layer_space =
        GeometryMapper::LocalToAncestorClipRect(
            source_property_tree_state, destination_property_tree_state);
    output.SetRect(clipped_rect_in_root_layer_space);
  }

  output.MoveBy(-context.root_layer->GetLayoutObject().PaintOffset());
  output.Move(context.sub_pixel_accumulation);
}

void PaintLayerClipper::InitializeCommonClipRectState(
    const ClipRectsContext& context,
    PropertyTreeState& source_property_tree_state,
    PropertyTreeState& destination_property_tree_state) const {
  DCHECK(use_geometry_mapper_);

  DCHECK(layer_.GetLayoutObject().FirstFragment()->LocalBorderBoxProperties());
  source_property_tree_state =
      *layer_.GetLayoutObject().FirstFragment()->LocalBorderBoxProperties();

  DCHECK(context.root_layer->GetLayoutObject()
             .FirstFragment()
             ->LocalBorderBoxProperties());
  destination_property_tree_state = *context.root_layer->GetLayoutObject()
                                         .FirstFragment()
                                         ->LocalBorderBoxProperties();

  auto* ancestor_properties =
      context.root_layer->GetLayoutObject().FirstFragment()->PaintProperties();
  if (!ancestor_properties)
    return;

  if (context.ShouldRespectRootLayerClip()) {
    const auto* ancestor_css_clip = ancestor_properties->CssClip();
    if (ancestor_css_clip) {
      DCHECK_EQ(destination_property_tree_state.Clip(),
                ancestor_css_clip);
      destination_property_tree_state.SetClip(ancestor_css_clip->Parent());
    }
  } else {
    const auto* ancestor_overflow_clip = ancestor_properties->OverflowClip();
    if (ancestor_overflow_clip) {
      DCHECK_EQ(destination_property_tree_state.Clip(),
                ancestor_overflow_clip->Parent());
      destination_property_tree_state.SetClip(ancestor_overflow_clip);
    }
  }
}

LayoutRect PaintLayerClipper::LocalVisualRect() const {
  const LayoutObject& layout_object = layer_.GetLayoutObject();
  // The LayoutView is special since its overflow clipping rect may be larger
  // than its box rect (crbug.com/492871).
  LayoutRect layer_bounds_with_visual_overflow =
      layout_object.IsLayoutView()
          ? ToLayoutView(layout_object).ViewRect()
          : ToLayoutBox(layout_object).VisualOverflowRect();
  ToLayoutBox(layout_object)
      .FlipForWritingMode(
          // PaintLayer are in physical coordinates, so the overflow has to be
          // flipped.
          layer_bounds_with_visual_overflow);
  return layer_bounds_with_visual_overflow;
}

void PaintLayerClipper::CalculateBackgroundClipRect(
    const ClipRectsContext& context,
    ClipRect& output) const {
  if (use_geometry_mapper_) {
    // TODO(chrishtr): fix the underlying bug that causes this situation.
    if (!layer_.GetLayoutObject().FirstFragment()->PaintProperties() &&
        !layer_.GetLayoutObject().FirstFragment()->LocalBorderBoxProperties()) {
      output.SetRect(FloatClipRect());
      return;
    }

    CalculateBackgroundClipRectWithGeometryMapper(context, output);
    return;
  }
  DCHECK(layer_.Parent());
  LayoutView* layout_view = layer_.GetLayoutObject().View();
  DCHECK(layout_view);

  RefPtr<ClipRects> parent_clip_rects = ClipRects::Create();
  if (&layer_ == context.root_layer) {
    parent_clip_rects->Reset(LayoutRect(LayoutRect::InfiniteIntRect()));
  } else {
    PaintLayerClipper(*layer_.Parent(), use_geometry_mapper_)
        .GetOrCalculateClipRects(context, *parent_clip_rects);
  }

  output = BackgroundClipRectForPosition(
      *parent_clip_rects, layer_.GetLayoutObject().StyleRef().GetPosition());

  // Note: infinite clipRects should not be scrolled here, otherwise they will
  // accidentally no longer be considered infinite.
  if (parent_clip_rects->Fixed() &&
      &context.root_layer->GetLayoutObject() == layout_view &&
      output != LayoutRect(LayoutRect::InfiniteIntRect()))
    output.Move(LayoutSize(layout_view->GetFrameView()->GetScrollOffset()));
}

void PaintLayerClipper::GetOrCalculateClipRects(const ClipRectsContext& context,
                                                ClipRects& clip_rects) const {
  DCHECK(!use_geometry_mapper_);

  if (context.UsesCache())
    clip_rects = GetClipRects(context);
  else
    CalculateClipRects(context, clip_rects);
}

bool PaintLayerClipper::ShouldClipOverflow(
    const ClipRectsContext& context) const {
  if (&layer_ == context.root_layer && !context.ShouldRespectRootLayerClip())
    return false;
  return HasOverflowClip(layer_);
}

ClipRects& PaintLayerClipper::PaintingClipRects(
    const PaintLayer* root_layer,
    ShouldRespectOverflowClipType respect_overflow_clip,
    const LayoutSize& subpixel_accumulation) const {
  DCHECK(!use_geometry_mapper_);
  ClipRectsContext context(root_layer, kPaintingClipRects,
                           kIgnorePlatformOverlayScrollbarSize,
                           subpixel_accumulation);
  if (respect_overflow_clip == kIgnoreOverflowClip)
    context.SetIgnoreOverflowClip();
  return GetClipRects(context);
}

}  // namespace blink
