// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreeBuilder.h"

#include <memory>
#include "core/dom/DOMNodeIds.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositingReasonFinder.h"
#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/FindPaintOffsetAndVisualRectNeedingUpdate.h"
#include "core/paint/FindPropertiesNeedingUpdate.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/SVGRootPainter.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

PaintPropertyTreeBuilderFragmentContext::
    PaintPropertyTreeBuilderFragmentContext()
    : current_effect(EffectPaintPropertyNode::Root()),
      input_clip_of_current_effect(ClipPaintPropertyNode::Root()) {
  current.clip = absolute_position.clip = fixed_position.clip =
      ClipPaintPropertyNode::Root();
  current.transform = absolute_position.transform = fixed_position.transform =
      TransformPaintPropertyNode::Root();
  current.scroll = absolute_position.scroll = fixed_position.scroll =
      ScrollPaintPropertyNode::Root();
}

// True if a new property was created, false if an existing one was updated.
static bool UpdatePreTranslation(
    LocalFrameView& frame_view,
    RefPtr<const TransformPaintPropertyNode> parent,
    const TransformationMatrix& matrix,
    const FloatPoint3D& origin) {
  DCHECK(!RuntimeEnabledFeatures::RootLayerScrollingEnabled());
  if (auto* existing_pre_translation = frame_view.PreTranslation()) {
    existing_pre_translation->Update(std::move(parent), matrix, origin);
    return false;
  }
  frame_view.SetPreTranslation(
      TransformPaintPropertyNode::Create(std::move(parent), matrix, origin));
  return true;
}

// True if a new property was created, false if an existing one was updated.
static bool UpdateContentClip(
    LocalFrameView& frame_view,
    RefPtr<const ClipPaintPropertyNode> parent,
    RefPtr<const TransformPaintPropertyNode> local_transform_space,
    const FloatRoundedRect& clip_rect,
    bool& clip_changed) {
  DCHECK(!RuntimeEnabledFeatures::RootLayerScrollingEnabled());
  if (auto* existing_content_clip = frame_view.ContentClip()) {
    if (existing_content_clip->ClipRect() != clip_rect)
      clip_changed = true;
    existing_content_clip->Update(std::move(parent),
                                  std::move(local_transform_space), clip_rect);
    return false;
  }
  frame_view.SetContentClip(ClipPaintPropertyNode::Create(
      std::move(parent), std::move(local_transform_space), clip_rect));
  clip_changed = true;
  return true;
}

// True if a new property was created or a main thread scrolling reason changed
// (which can affect descendants), false if an existing one was updated.
static bool UpdateScroll(
    LocalFrameView& frame_view,
    RefPtr<const ScrollPaintPropertyNode> parent,
    const IntSize& clip,
    const IntSize& bounds,
    bool user_scrollable_horizontal,
    bool user_scrollable_vertical,
    MainThreadScrollingReasons main_thread_scrolling_reasons,
    WebLayerScrollClient* scroll_client) {
  DCHECK(!RuntimeEnabledFeatures::RootLayerScrollingEnabled());
  auto element_id = CompositorElementIdFromLayoutObjectId(
      frame_view.GetLayoutView()->UniqueId(),
      CompositorElementIdNamespace::kScrollTranslation);
  if (auto* existing_scroll = frame_view.ScrollNode()) {
    auto existing_reasons = existing_scroll->GetMainThreadScrollingReasons();
    existing_scroll->Update(
        std::move(parent), IntPoint(), clip, bounds, user_scrollable_horizontal,
        user_scrollable_vertical, main_thread_scrolling_reasons, element_id,
        scroll_client);
    return existing_reasons != main_thread_scrolling_reasons;
  }
  frame_view.SetScrollNode(ScrollPaintPropertyNode::Create(
      std::move(parent), IntPoint(), clip, bounds, user_scrollable_horizontal,
      user_scrollable_vertical, main_thread_scrolling_reasons, element_id,
      scroll_client));
  return true;
}

// True if a new property was created, false if an existing one was updated.
static bool UpdateScrollTranslation(
    LocalFrameView& frame_view,
    RefPtr<const TransformPaintPropertyNode> parent,
    const TransformationMatrix& matrix,
    PassRefPtr<ScrollPaintPropertyNode> scroll) {
  DCHECK(!RuntimeEnabledFeatures::RootLayerScrollingEnabled());
  // TODO(pdr): Set the correct compositing reasons here.
  if (auto* existing_scroll_translation = frame_view.ScrollTranslation()) {
    existing_scroll_translation->Update(
        std::move(parent), matrix, FloatPoint3D(), false, 0,
        kCompositingReasonNone, CompositorElementId(), std::move(scroll));
    return false;
  }
  frame_view.SetScrollTranslation(TransformPaintPropertyNode::Create(
      std::move(parent), matrix, FloatPoint3D(), false, 0,
      kCompositingReasonNone, CompositorElementId(), std::move(scroll)));
  return true;
}

static MainThreadScrollingReasons GetMainThreadScrollingReasons(
    const LocalFrameView& frame_view,
    MainThreadScrollingReasons ancestor_reasons) {
  auto reasons = ancestor_reasons;
  if (!frame_view.GetFrame().GetSettings()->GetThreadedScrollingEnabled())
    reasons |= MainThreadScrollingReason::kThreadedScrollingDisabled;
  if (frame_view.HasBackgroundAttachmentFixedObjects())
    reasons |= MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  return reasons;
}

void PaintPropertyTreeBuilder::UpdateProperties(
    LocalFrameView& frame_view,
    PaintPropertyTreeBuilderContext& full_context) {
  if (full_context.fragments.IsEmpty())
    full_context.fragments.push_back(PaintPropertyTreeBuilderFragmentContext());

  PaintPropertyTreeBuilderFragmentContext& context = full_context.fragments[0];
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    // With root layer scrolling, the LayoutView (a LayoutObject) properties are
    // updated like other objects (see updatePropertiesAndContextForSelf and
    // updatePropertiesAndContextForChildren) instead of needing LayoutView-
    // specific property updates here.
    context.current.paint_offset.MoveBy(frame_view.Location());
    context.current.rendering_context_id = 0;
    context.current.should_flatten_inherited_transform = true;
    context.absolute_position = context.current;
    full_context.container_for_absolute_position = nullptr;
    context.fixed_position = context.current;
    return;
  }

#if DCHECK_IS_ON()
  FindFrameViewPropertiesNeedingUpdateScope check_scope(
      &frame_view, full_context.is_actually_needed);
#endif

  if (frame_view.NeedsPaintPropertyUpdate() ||
      full_context.force_subtree_update) {
    TransformationMatrix frame_translate;
    frame_translate.Translate(
        frame_view.X() + context.current.paint_offset.X(),
        frame_view.Y() + context.current.paint_offset.Y());
    full_context.force_subtree_update |= UpdatePreTranslation(
        frame_view, context.current.transform, frame_translate, FloatPoint3D());

    FloatRoundedRect content_clip(
        IntRect(IntPoint(), frame_view.VisibleContentSize()));
    full_context.force_subtree_update |= UpdateContentClip(
        frame_view, context.current.clip, frame_view.PreTranslation(),
        content_clip, full_context.clip_changed);

    if (frame_view.IsScrollable()) {
      IntSize scroll_clip = frame_view.VisibleContentSize();
      IntSize scroll_bounds = frame_view.ContentsSize();
      bool user_scrollable_horizontal =
          frame_view.UserInputScrollable(kHorizontalScrollbar);
      bool user_scrollable_vertical =
          frame_view.UserInputScrollable(kVerticalScrollbar);

      auto ancestor_reasons =
          context.current.scroll->GetMainThreadScrollingReasons();
      auto reasons =
          GetMainThreadScrollingReasons(frame_view, ancestor_reasons);

      full_context.force_subtree_update |= UpdateScroll(
          frame_view, context.current.scroll, scroll_clip, scroll_bounds,
          user_scrollable_horizontal, user_scrollable_vertical, reasons,
          frame_view.GetScrollableArea());
    } else if (frame_view.ScrollNode()) {
      // Ensure pre-existing properties are cleared if there is no scrolling.
      frame_view.SetScrollNode(nullptr);
      // Rebuild all descendant properties because a property was removed.
      full_context.force_subtree_update = true;
    }

    // A scroll translation node is created for static offset (e.g., overflow
    // hidden with scroll offset) or cases that scroll and have a scroll node.
    ScrollOffset scroll_offset = frame_view.GetScrollOffset();
    if (frame_view.IsScrollable() || !scroll_offset.IsZero()) {
      TransformationMatrix frame_scroll;
      frame_scroll.Translate(-scroll_offset.Width(), -scroll_offset.Height());
      full_context.force_subtree_update |=
          UpdateScrollTranslation(frame_view, frame_view.PreTranslation(),
                                  frame_scroll, frame_view.ScrollNode());
    } else if (frame_view.ScrollTranslation()) {
      // Ensure pre-existing properties are cleared if there is no scrolling.
      frame_view.SetScrollTranslation(nullptr);
      // Rebuild all descendant properties because a property was removed.
      full_context.force_subtree_update = true;
    }
  }

  // Initialize the context for current, absolute and fixed position cases.
  // They are the same, except that scroll translation does not apply to
  // fixed position descendants.
  const auto* fixed_transform_node = frame_view.PreTranslation()
                                         ? frame_view.PreTranslation()
                                         : context.current.transform;
  auto* fixed_scroll_node = context.current.scroll;
  DCHECK(frame_view.PreTranslation());
  context.current.transform = frame_view.PreTranslation();
  DCHECK(frame_view.ContentClip());
  context.current.clip = frame_view.ContentClip();
  if (const auto* scroll_node = frame_view.ScrollNode())
    context.current.scroll = scroll_node;
  if (const auto* scroll_translation = frame_view.ScrollTranslation())
    context.current.transform = scroll_translation;
  context.current.paint_offset = LayoutPoint();
  context.current.rendering_context_id = 0;
  context.current.should_flatten_inherited_transform = true;
  context.absolute_position = context.current;
  full_context.container_for_absolute_position = nullptr;
  context.fixed_position = context.current;
  context.fixed_position.transform = fixed_transform_node;
  context.fixed_position.scroll = fixed_scroll_node;

  std::unique_ptr<PropertyTreeState> contents_state(new PropertyTreeState(
      context.current.transform, context.current.clip, context.current_effect));
  frame_view.SetTotalPropertyTreeStateForContents(std::move(contents_state));
}

static bool NeedsPaintOffsetTranslation(const LayoutObject& object) {
  if (!object.IsBoxModelObject())
    return false;
  const LayoutBoxModelObject& box_model = ToLayoutBoxModelObject(object);
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled() &&
      box_model.IsLayoutView()) {
    // Root layer scrolling always creates a translation node for LayoutView to
    // ensure fixed and absolute contexts use the correct transform space.
    return true;
  }
  if (box_model.HasLayer() && box_model.Layer()->PaintsWithTransform(
                                  kGlobalPaintFlattenCompositingLayers)) {
    return true;
  }
  return false;
}

void PaintPropertyTreeBuilder::UpdatePaintOffsetTranslation(
    const LayoutBoxModelObject& object,
    PaintPropertyTreeBuilderFragmentContext& context,
    ObjectPaintProperties& properties,
    bool& force_subtree_update) {
  if (NeedsPaintOffsetTranslation(object) &&
      // As an optimization, skip these paint offset translation nodes when
      // the offset is an identity. An exception is the layout view because root
      // layer scrolling needs a transform node to ensure fixed and absolute
      // descendants use the correct transform space.
      (object.IsLayoutView() ||
       context.current.paint_offset != LayoutPoint())) {
    // We should use the same subpixel paint offset values for snapping
    // regardless of whether a transform is present. If there is a transform
    // we round the paint offset but keep around the residual fractional
    // component for the transformed content to paint with.  In spv1 this was
    // called "subpixel accumulation". For more information, see
    // PaintLayer::subpixelAccumulation() and
    // PaintLayerPainter::paintFragmentByApplyingTransform.
    IntPoint rounded_paint_offset =
        RoundedIntPoint(context.current.paint_offset);
    LayoutPoint fractional_paint_offset =
        LayoutPoint(context.current.paint_offset - rounded_paint_offset);
    if (fractional_paint_offset != LayoutPoint()) {
      // If the object has a non-translation transform, discard the fractional
      // paint offset which can't be transformed by the transform.
      TransformationMatrix matrix;
      object.StyleRef().ApplyTransform(
          matrix, LayoutSize(), ComputedStyle::kExcludeTransformOrigin,
          ComputedStyle::kIncludeMotionPath,
          ComputedStyle::kIncludeIndependentTransformProperties);
      if (!matrix.IsIdentityOrTranslation())
        fractional_paint_offset = LayoutPoint();
    }

    auto result = properties.UpdatePaintOffsetTranslation(
        context.current.transform,
        TransformationMatrix().Translate(rounded_paint_offset.X(),
                                         rounded_paint_offset.Y()),
        FloatPoint3D(), context.current.should_flatten_inherited_transform,
        context.current.rendering_context_id);
    force_subtree_update |= result.NewNodeCreated();

    context.current.transform = properties.PaintOffsetTranslation();
    context.current.paint_offset = fractional_paint_offset;
    if (RuntimeEnabledFeatures::RootLayerScrollingEnabled() &&
        object.IsLayoutView()) {
      context.absolute_position.transform = properties.PaintOffsetTranslation();
      context.fixed_position.transform = properties.PaintOffsetTranslation();
      context.absolute_position.paint_offset = fractional_paint_offset;
      context.fixed_position.paint_offset = fractional_paint_offset;
    }
  } else {
    force_subtree_update |= properties.ClearPaintOffsetTranslation();
  }
}

static bool NeedsTransformForNonRootSVG(const LayoutObject& object) {
  // TODO(pdr): Check for the presence of a transform instead of the value.
  // Checking for an identity matrix will cause the property tree structure
  // to change during animations if the animation passes through the
  // identity matrix.
  return object.IsSVGChild() &&
         !object.LocalToSVGParentTransform().IsIdentity();
}

// SVG does not use the general transform update of |updateTransform|, instead
// creating a transform node for SVG-specific transforms without 3D.
void PaintPropertyTreeBuilder::UpdateTransformForNonRootSVG(
    const LayoutObject& object,
    ObjectPaintProperties& properties,
    PaintPropertyTreeBuilderFragmentContext& context,
    bool& force_subtree_update) {
  DCHECK(object.IsSVGChild());
  // SVG does not use paint offset internally, except for SVGForeignObject which
  // has different SVG and HTML coordinate spaces.
  DCHECK(object.IsSVGForeignObject() ||
         context.current.paint_offset == LayoutPoint());

  if (object.NeedsPaintPropertyUpdate() || force_subtree_update) {
    AffineTransform transform = object.LocalToSVGParentTransform();
    if (NeedsTransformForNonRootSVG(object)) {
      // The origin is included in the local transform, so leave origin empty.
      auto result = properties.UpdateTransform(context.current.transform,
                                               TransformationMatrix(transform),
                                               FloatPoint3D());
      force_subtree_update |= result.NewNodeCreated();
    } else {
      force_subtree_update |= properties.ClearTransform();
    }
  }

  if (properties.Transform()) {
    context.current.transform = properties.Transform();
    context.current.should_flatten_inherited_transform = false;
    context.current.rendering_context_id = 0;
  }
}

static CompositingReasons CompositingReasonsForTransform(const LayoutBox& box) {
  const ComputedStyle& style = box.StyleRef();
  CompositingReasons compositing_reasons = kCompositingReasonNone;
  if (CompositingReasonFinder::RequiresCompositingForTransform(box))
    compositing_reasons |= kCompositingReason3DTransform;

  if (CompositingReasonFinder::RequiresCompositingForTransformAnimation(style))
    compositing_reasons |= kCompositingReasonActiveAnimation;

  if (style.HasWillChangeCompositingHint() &&
      !style.SubtreeWillChangeContents())
    compositing_reasons |= kCompositingReasonWillChangeCompositingHint;

  if (box.HasLayer() && box.Layer()->Has3DTransformedDescendant()) {
    if (style.HasPerspective())
      compositing_reasons |= kCompositingReasonPerspectiveWith3DDescendants;
    if (style.UsedTransformStyle3D() == ETransformStyle3D::kPreserve3d)
      compositing_reasons |= kCompositingReasonPreserve3DWith3DDescendants;
  }

  return compositing_reasons;
}

static FloatPoint3D TransformOrigin(const LayoutBox& box) {
  const ComputedStyle& style = box.StyleRef();
  // Transform origin has no effect without a transform or motion path.
  if (!style.HasTransform())
    return FloatPoint3D();
  FloatSize border_box_size(box.Size());
  return FloatPoint3D(
      FloatValueForLength(style.TransformOriginX(), border_box_size.Width()),
      FloatValueForLength(style.TransformOriginY(), border_box_size.Height()),
      style.TransformOriginZ());
}

static bool NeedsTransform(const LayoutObject& object) {
  if (!object.IsBox())
    return false;
  return object.StyleRef().HasTransform() || object.StyleRef().Preserves3D() ||
         CompositingReasonsForTransform(ToLayoutBox(object)) !=
             kCompositingReasonNone;
}

void PaintPropertyTreeBuilder::UpdateTransform(
    const LayoutObject& object,
    ObjectPaintProperties& properties,
    PaintPropertyTreeBuilderFragmentContext& context,
    bool& force_subtree_update) {
  if (object.IsSVGChild()) {
    UpdateTransformForNonRootSVG(object, properties, context,
                                 force_subtree_update);
    return;
  }

  if (object.NeedsPaintPropertyUpdate() || force_subtree_update) {
    const ComputedStyle& style = object.StyleRef();
    // A transform node is allocated for transforms, preserves-3d and any
    // direct compositing reason. The latter is required because this is the
    // only way to represent compositing both an element and its stacking
    // descendants.
    if (NeedsTransform(object)) {
      auto& box = ToLayoutBox(object);

      CompositingReasons compositing_reasons =
          CompositingReasonsForTransform(box);

      TransformationMatrix matrix;
      style.ApplyTransform(
          matrix, box.Size(), ComputedStyle::kExcludeTransformOrigin,
          ComputedStyle::kIncludeMotionPath,
          ComputedStyle::kIncludeIndependentTransformProperties);

      // TODO(trchen): transform-style should only be respected if a
      // PaintLayer is created. If a node with transform-style: preserve-3d
      // does not exist in an existing rendering context, it establishes a new
      // one.
      unsigned rendering_context_id = context.current.rendering_context_id;
      if (style.Preserves3D() && !rendering_context_id)
        rendering_context_id = PtrHash<const LayoutObject>::GetHash(&object);

      auto result = properties.UpdateTransform(
          context.current.transform, matrix, TransformOrigin(box),
          context.current.should_flatten_inherited_transform,
          rendering_context_id, compositing_reasons,
          CompositorElementIdFromLayoutObjectId(
              object.UniqueId(), CompositorElementIdNamespace::kPrimary));
      force_subtree_update |= result.NewNodeCreated();
    } else {
      force_subtree_update |= properties.ClearTransform();
    }
  }

  if (properties.Transform()) {
    context.current.transform = properties.Transform();
    if (object.StyleRef().Preserves3D()) {
      context.current.rendering_context_id =
          properties.Transform()->RenderingContextId();
      context.current.should_flatten_inherited_transform = false;
    } else {
      context.current.rendering_context_id = 0;
      context.current.should_flatten_inherited_transform = true;
    }
  }
}

static bool ComputeMaskParameters(IntRect& mask_clip,
                                  ColorFilter& mask_color_filter,
                                  const LayoutObject& object,
                                  const LayoutPoint& paint_offset) {
  DCHECK(object.IsBoxModelObject() || object.IsSVGChild());
  const ComputedStyle& style = object.StyleRef();

  if (object.IsSVGChild()) {
    SVGResources* resources =
        SVGResourcesCache::CachedResourcesForLayoutObject(&object);
    LayoutSVGResourceMasker* masker = resources ? resources->Masker() : nullptr;
    if (!masker)
      return false;
    mask_clip = EnclosingIntRect(object.ObjectBoundingBox());
    mask_color_filter = masker->Style()->SvgStyle().MaskType() == MT_LUMINANCE
                            ? kColorFilterLuminanceToAlpha
                            : kColorFilterNone;
    return true;
  }
  if (!style.HasMask())
    return false;

  LayoutRect maximum_mask_region;
  // For HTML/CSS objects, the extent of the mask is known as "mask
  // painting area", which is determined by CSS mask-clip property.
  // We don't implement mask-clip:margin-box or no-clip currently,
  // so the maximum we can get is border-box.
  if (object.IsBox()) {
    maximum_mask_region = ToLayoutBox(object).BorderBoxRect();
  } else {
    // For inline elements, depends on the value of box-decoration-break
    // there could be one box in multiple fragments or multiple boxes.
    // Either way here we are only interested in the bounding box of them.
    DCHECK(object.IsLayoutInline());
    maximum_mask_region = ToLayoutInline(object).LinesBoundingBox();
  }
  maximum_mask_region.MoveBy(paint_offset);
  mask_clip = EnclosingIntRect(maximum_mask_region);
  mask_color_filter = kColorFilterNone;
  return true;
}

static bool NeedsEffect(const LayoutObject& object) {
  const ComputedStyle& style = object.StyleRef();

  const bool is_css_isolated_group =
      object.IsBoxModelObject() && style.IsStackingContext();

  if (!is_css_isolated_group && !object.IsSVGChild())
    return false;

  if (object.IsSVG()) {
    // This handles SVGRoot objects which have PaintLayers.
    if (object.IsSVGRoot() && object.HasNonIsolatedBlendingDescendants())
      return true;
    if (SVGLayoutSupport::IsIsolationRequired(&object))
      return true;
  } else if (object.IsBoxModelObject()) {
    if (PaintLayer* layer = ToLayoutBoxModelObject(object).Layer()) {
      if (layer->HasNonIsolatedDescendantWithBlendMode())
        return true;
    }
  }

  SkBlendMode blend_mode = object.IsBlendingAllowed()
                               ? WebCoreCompositeToSkiaComposite(
                                     kCompositeSourceOver, style.BlendMode())
                               : SkBlendMode::kSrcOver;
  if (blend_mode != SkBlendMode::kSrcOver)
    return true;

  float opacity = style.Opacity();
  if (opacity != 1.0f)
    return true;

  if (CompositingReasonFinder::RequiresCompositingForOpacityAnimation(style))
    return true;

  if (object.IsSVGChild()) {
    if (SVGResources* resources =
            SVGResourcesCache::CachedResourcesForLayoutObject(&object)) {
      if (resources->Masker())
        return true;
    }
  }

  if (object.StyleRef().HasMask())
    return true;

  return false;
}

void PaintPropertyTreeBuilder::UpdateEffect(
    const LayoutObject& object,
    ObjectPaintProperties& properties,
    PaintPropertyTreeBuilderFragmentContext& context,
    bool& force_subtree_update,
    bool& clip_changed) {
  const ComputedStyle& style = object.StyleRef();

  // TODO(trchen): Can't omit effect node if we have 3D children.
  if (object.NeedsPaintPropertyUpdate() || force_subtree_update) {
    const ClipPaintPropertyNode* output_clip =
        context.input_clip_of_current_effect;
    bool local_clip_added_or_removed = false;
    bool local_clip_changed = false;
    if (NeedsEffect(object)) {

      // We may begin to composite our subtree prior to an animation starts,
      // but a compositor element ID is only needed when an animation is
      // current.
      CompositingReasons compositing_reasons = kCompositingReasonNone;
      if (CompositingReasonFinder::RequiresCompositingForOpacityAnimation(
              style)) {
        compositing_reasons = kCompositingReasonActiveAnimation;
      }

      IntRect mask_clip;
      ColorFilter mask_color_filter;
      bool has_mask = ComputeMaskParameters(
          mask_clip, mask_color_filter, object, context.current.paint_offset);
      if (has_mask) {
        FloatRoundedRect rounded_mask_clip(mask_clip);
        if (properties.MaskClip() &&
            rounded_mask_clip != properties.MaskClip()->ClipRect())
          local_clip_changed = true;
        auto result = properties.UpdateMaskClip(context.current.clip,
                                                context.current.transform,
                                                FloatRoundedRect(mask_clip));
        local_clip_added_or_removed |= result.NewNodeCreated();
        output_clip = properties.MaskClip();
      } else {
        force_subtree_update |= properties.ClearMaskClip();
      }

      SkBlendMode blend_mode =
          object.IsBlendingAllowed()
              ? WebCoreCompositeToSkiaComposite(kCompositeSourceOver,
                                                style.BlendMode())
              : SkBlendMode::kSrcOver;

      DCHECK(!style.HasCurrentOpacityAnimation() ||
             compositing_reasons != kCompositingReasonNone);
      auto result = properties.UpdateEffect(
          context.current_effect, context.current.transform, output_clip,
          kColorFilterNone, CompositorFilterOperations(), style.Opacity(),
          blend_mode, compositing_reasons,
          CompositorElementIdFromLayoutObjectId(
              object.UniqueId(), CompositorElementIdNamespace::kPrimary));
      force_subtree_update |= result.NewNodeCreated();
      if (has_mask) {
        auto result = properties.UpdateMask(
            properties.Effect(), context.current.transform, output_clip,
            mask_color_filter, CompositorFilterOperations(), 1.f,
            SkBlendMode::kDstIn, kCompositingReasonNone,
            CompositorElementIdFromLayoutObjectId(
                object.UniqueId(), CompositorElementIdNamespace::kEffectMask));
        force_subtree_update |= result.NewNodeCreated();
      } else {
        force_subtree_update |= properties.ClearMask();
      }
    } else {
      force_subtree_update |= properties.ClearEffect();
      force_subtree_update |= properties.ClearMask();
      local_clip_added_or_removed |= properties.ClearMaskClip();
    }
    force_subtree_update |= local_clip_added_or_removed;
    clip_changed |= local_clip_changed || local_clip_added_or_removed;
  }

  if (properties.Effect()) {
    context.current_effect = properties.Effect();
    if (properties.MaskClip()) {
      context.input_clip_of_current_effect = context.current.clip =
          context.absolute_position.clip = context.fixed_position.clip =
              properties.MaskClip();
    }
  }
}

static bool NeedsFilter(const LayoutObject& object) {
  // TODO(trchen): SVG caches filters in SVGResources. Implement it.
  return object.IsBoxModelObject() && ToLayoutBoxModelObject(object).Layer() &&
         (object.StyleRef().HasFilter() || object.HasReflection());
}

void PaintPropertyTreeBuilder::UpdateFilter(
    const LayoutObject& object,
    ObjectPaintProperties& properties,
    PaintPropertyTreeBuilderFragmentContext& context,
    bool& force_subtree_update) {
  const ComputedStyle& style = object.StyleRef();

  if (object.NeedsPaintPropertyUpdate() || force_subtree_update) {
    if (NeedsFilter(object)) {
      CompositorFilterOperations filter =
          ToLayoutBoxModelObject(object)
              .Layer()
              ->CreateCompositorFilterOperationsForFilter(style);

      // The CSS filter spec didn't specify how filters interact with overflow
      // clips. The implementation here mimics the old Blink/WebKit behavior for
      // backward compatibility.
      // Basically the output of the filter will be affected by clips that
      // applies to the current element. The descendants that paints into the
      // input of the filter ignores any clips collected so far. For example:
      // <div style="overflow:scroll">
      //   <div style="filter:blur(1px);">
      //     <div>A</div>
      //     <div style="position:absolute;">B</div>
      //   </div>
      // </div>
      // In this example "A" should be clipped if the filter was not present.
      // With the filter, "A" will be rastered without clipping, but instead
      // the blurred result will be clipped.
      // On the other hand, "B" should not be clipped because the overflow clip
      // is not in its containing block chain, but as the filter output will be
      // clipped, so a blurred "B" may still be invisible.
      const ClipPaintPropertyNode* output_clip = context.current.clip;

      // TODO(trchen): A filter may contain spatial operations such that an
      // output pixel may depend on an input pixel outside of the output clip.
      // We should generate a special clip node to represent this expansion.

      // We may begin to composite our subtree prior to an animation starts,
      // but a compositor element ID is only needed when an animation is
      // current.
      CompositingReasons compositing_reasons =
          CompositingReasonFinder::RequiresCompositingForFilterAnimation(style)
              ? kCompositingReasonActiveAnimation
              : kCompositingReasonNone;
      DCHECK(!style.HasCurrentFilterAnimation() ||
             compositing_reasons != kCompositingReasonNone);

      auto result = properties.UpdateFilter(
          context.current_effect, context.current.transform, output_clip,
          kColorFilterNone, std::move(filter), 1.f, SkBlendMode::kSrcOver,
          compositing_reasons,
          CompositorElementIdFromLayoutObjectId(
              object.UniqueId(), CompositorElementIdNamespace::kEffectFilter));
      force_subtree_update |= result.NewNodeCreated();
    } else {
      force_subtree_update |= properties.ClearFilter();
    }
  }

  if (properties.Filter()) {
    context.current_effect = properties.Filter();
    // TODO(trchen): Change input clip to expansion hint once implemented.
    const ClipPaintPropertyNode* input_clip = properties.Filter()->OutputClip();
    context.input_clip_of_current_effect = context.current.clip =
        context.absolute_position.clip = context.fixed_position.clip =
            input_clip;
  }
}

static bool NeedsCssClip(const LayoutObject& object) {
  return object.HasClip();
}

void PaintPropertyTreeBuilder::UpdateCssClip(
    const LayoutObject& object,
    ObjectPaintProperties& properties,
    PaintPropertyTreeBuilderFragmentContext& context,
    bool& force_subtree_update,
    bool& clip_changed) {
  if (object.NeedsPaintPropertyUpdate() || force_subtree_update) {
    bool local_clip_added_or_removed = false;
    bool local_clip_changed = false;
    if (NeedsCssClip(object)) {
      // Create clip node for descendants that are not fixed position.
      // We don't have to setup context.absolutePosition.clip here because this
      // object must be a container for absolute position descendants, and will
      // copy from in-flow context later at updateOutOfFlowContext() step.
      DCHECK(object.CanContainAbsolutePositionObjects());
      LayoutRect clip_rect =
          ToLayoutBox(object).ClipRect(context.current.paint_offset);

      FloatRoundedRect rounded_clip_rect((FloatRect(clip_rect)));
      if (properties.CssClip() &&
          properties.CssClip()->ClipRect() != rounded_clip_rect)
        local_clip_changed = true;

      auto result = properties.UpdateCssClip(
          context.current.clip, context.current.transform,
          FloatRoundedRect(FloatRect(clip_rect)));
      local_clip_added_or_removed |= result.NewNodeCreated();
    } else {
      local_clip_added_or_removed |= properties.ClearCssClip();
    }
    force_subtree_update |= local_clip_added_or_removed;
    clip_changed |= local_clip_changed || local_clip_added_or_removed;
  }

  if (properties.CssClip())
    context.current.clip = properties.CssClip();
}

void PaintPropertyTreeBuilder::UpdateLocalBorderBoxContext(
    const LayoutObject& object,
    PaintPropertyTreeBuilderFragmentContext& context,
    FragmentData* fragment_data,
    bool& force_subtree_update) {
  if (!object.NeedsPaintPropertyUpdate() && !force_subtree_update)
    return;

  // We only need to cache the local border box properties for layered objects.
  if (!object.HasLayer()) {
    if (fragment_data)
      fragment_data->ClearLocalBorderBoxProperties();
  } else {
    PropertyTreeState local_border_box =
        PropertyTreeState(context.current.transform, context.current.clip,
                          context.current_effect);
    fragment_data->SetLocalBorderBoxProperties(local_border_box);
  }
}

static bool NeedsScrollbarPaintOffset(const LayoutObject& object) {
  if (!object.IsBoxModelObject())
    return false;
  if (auto* area = ToLayoutBoxModelObject(object).GetScrollableArea()) {
    if (area->HorizontalScrollbar() || area->VerticalScrollbar())
      return true;
  }
  return false;
}

// TODO(trchen): Remove this once we bake the paint offset into frameRect.
void PaintPropertyTreeBuilder::UpdateScrollbarPaintOffset(
    const LayoutObject& object,
    ObjectPaintProperties& properties,
    PaintPropertyTreeBuilderFragmentContext& context,
    bool& force_subtree_update) {
  if (!object.NeedsPaintPropertyUpdate() && !force_subtree_update)
    return;

  if (NeedsScrollbarPaintOffset(object)) {
    IntPoint rounded_paint_offset =
        RoundedIntPoint(context.current.paint_offset);
    auto paint_offset = TransformationMatrix().Translate(
        rounded_paint_offset.X(), rounded_paint_offset.Y());
    auto result = properties.UpdateScrollbarPaintOffset(
        context.current.transform, paint_offset, FloatPoint3D());
    force_subtree_update |= result.NewNodeCreated();
  } else {
    force_subtree_update |= properties.ClearScrollbarPaintOffset();
  }
}

static bool NeedsOverflowClip(const LayoutObject& object) {
  return object.IsBox() && ToLayoutBox(object).ShouldClipOverflow();
}

void PaintPropertyTreeBuilder::UpdateOverflowClip(
    const LayoutObject& object,
    ObjectPaintProperties& properties,
    PaintPropertyTreeBuilderFragmentContext& context,
    bool& force_subtree_update,
    bool& clip_changed) {
  if (object.NeedsPaintPropertyUpdate() || force_subtree_update) {
    bool local_clip_added_or_removed = false;
    bool local_clip_changed = false;
    if (NeedsOverflowClip(object)) {
      const LayoutBox& box = ToLayoutBox(object);
      LayoutRect clip_rect;
      clip_rect =
          LayoutRect(box.OverflowClipRect(context.current.paint_offset));

      const auto* current_clip = context.current.clip;
      if (box.StyleRef().HasBorderRadius()) {
        auto inner_border = box.StyleRef().GetRoundedInnerBorderFor(
            LayoutRect(context.current.paint_offset, box.Size()));
        auto result = properties.UpdateInnerBorderRadiusClip(
            context.current.clip, context.current.transform, inner_border);
        local_clip_added_or_removed |= result.NewNodeCreated();
        current_clip = properties.InnerBorderRadiusClip();
      } else {
        local_clip_added_or_removed |= properties.ClearInnerBorderRadiusClip();
      }

      FloatRoundedRect clipping_rect((FloatRect(clip_rect)));
      if (properties.OverflowClip() &&
          clipping_rect != properties.OverflowClip()->ClipRect()) {
        local_clip_changed = true;
      }

      auto result =
          properties.UpdateOverflowClip(current_clip, context.current.transform,
                                        FloatRoundedRect(FloatRect(clip_rect)));
      local_clip_added_or_removed |= result.NewNodeCreated();
    } else {
      local_clip_added_or_removed |= properties.ClearInnerBorderRadiusClip();
      local_clip_added_or_removed |= properties.ClearOverflowClip();
    }
    force_subtree_update |= local_clip_added_or_removed;
    clip_changed |= local_clip_changed || local_clip_added_or_removed;
  }

  if (properties.OverflowClip())
    context.current.clip = properties.OverflowClip();
}

static FloatPoint PerspectiveOrigin(const LayoutBox& box) {
  const ComputedStyle& style = box.StyleRef();
  // Perspective origin has no effect without perspective.
  DCHECK(style.HasPerspective());
  FloatSize border_box_size(box.Size());
  return FloatPoint(
      FloatValueForLength(style.PerspectiveOriginX(), border_box_size.Width()),
      FloatValueForLength(style.PerspectiveOriginY(),
                          border_box_size.Height()));
}

static bool NeedsPerspective(const LayoutObject& object) {
  return object.IsBox() && object.StyleRef().HasPerspective();
}

void PaintPropertyTreeBuilder::UpdatePerspective(
    const LayoutObject& object,
    ObjectPaintProperties& properties,
    PaintPropertyTreeBuilderFragmentContext& context,
    bool& force_subtree_update) {
  if (object.NeedsPaintPropertyUpdate() || force_subtree_update) {
    if (NeedsPerspective(object)) {
      const ComputedStyle& style = object.StyleRef();
      // The perspective node must not flatten (else nothing will get
      // perspective), but it should still extend the rendering context as
      // most transform nodes do.
      TransformationMatrix matrix =
          TransformationMatrix().ApplyPerspective(style.Perspective());
      FloatPoint3D origin = PerspectiveOrigin(ToLayoutBox(object)) +
                            ToLayoutSize(context.current.paint_offset);
      auto result = properties.UpdatePerspective(
          context.current.transform, matrix, origin,
          context.current.should_flatten_inherited_transform,
          context.current.rendering_context_id);
      force_subtree_update |= result.NewNodeCreated();
    } else {
      force_subtree_update |= properties.ClearPerspective();
    }
  }

  if (properties.Perspective()) {
    context.current.transform = properties.Perspective();
    context.current.should_flatten_inherited_transform = false;
  }
}

static bool NeedsSVGLocalToBorderBoxTransform(const LayoutObject& object) {
  return object.IsSVGRoot();
}

void PaintPropertyTreeBuilder::UpdateSvgLocalToBorderBoxTransform(
    const LayoutObject& object,
    ObjectPaintProperties& properties,
    PaintPropertyTreeBuilderFragmentContext& context,
    bool& force_subtree_update) {
  if (!object.IsSVGRoot())
    return;

  if (object.NeedsPaintPropertyUpdate() || force_subtree_update) {
    AffineTransform transform_to_border_box =
        SVGRootPainter(ToLayoutSVGRoot(object))
            .TransformToPixelSnappedBorderBox(context.current.paint_offset);
    if (!transform_to_border_box.IsIdentity() &&
        NeedsSVGLocalToBorderBoxTransform(object)) {
      auto result = properties.UpdateSvgLocalToBorderBoxTransform(
          context.current.transform, transform_to_border_box, FloatPoint3D());
      force_subtree_update |= result.NewNodeCreated();
    } else {
      force_subtree_update |= properties.ClearSvgLocalToBorderBoxTransform();
    }
  }

  if (properties.SvgLocalToBorderBoxTransform()) {
    context.current.transform = properties.SvgLocalToBorderBoxTransform();
    context.current.should_flatten_inherited_transform = false;
    context.current.rendering_context_id = 0;
  }
  // The paint offset is included in |transformToBorderBox| so SVG does not need
  // to handle paint offset internally.
  context.current.paint_offset = LayoutPoint();
}

static MainThreadScrollingReasons GetMainThreadScrollingReasons(
    const LayoutObject& object,
    MainThreadScrollingReasons ancestor_reasons) {
  // The current main thread scrolling reasons implementation only changes
  // reasons at frame boundaries, so we can early-out when not at a LayoutView.
  // TODO(pdr): Need to find a solution to the style-related main thread
  // scrolling reasons such as opacity and transform which violate this.
  if (!object.IsLayoutView())
    return ancestor_reasons;
  return GetMainThreadScrollingReasons(*object.GetFrameView(),
                                       ancestor_reasons);
}

static bool NeedsScrollNode(const LayoutObject& object) {
  if (!object.HasOverflowClip())
    return false;
  return ToLayoutBox(object).GetScrollableArea()->ScrollsOverflow();
}

// True if a scroll translation is needed for static scroll offset (e.g.,
// overflow hidden with scroll), or if a scroll node is needed for composited
// scrolling.
static bool NeedsScrollOrScrollTranslation(const LayoutObject& object) {
  if (!object.HasOverflowClip())
    return false;
  IntSize scroll_offset = ToLayoutBox(object).ScrolledContentOffset();
  return !scroll_offset.IsZero() || NeedsScrollNode(object);
}

void PaintPropertyTreeBuilder::UpdateScrollAndScrollTranslation(
    const LayoutObject& object,
    ObjectPaintProperties& properties,
    PaintPropertyTreeBuilderFragmentContext& context,
    bool& force_subtree_update) {
  if (object.NeedsPaintPropertyUpdate() || force_subtree_update) {
    if (NeedsScrollNode(object)) {
      const LayoutBox& box = ToLayoutBox(object);
      auto* scrollable_area = box.GetScrollableArea();

      // The container bounds are snapped to integers to match the equivalent
      // bounds on cc::ScrollNode. The offset is snapped to match the current
      // integer offsets used in CompositedLayerMapping.
      auto clip_rect = PixelSnappedIntRect(
          box.OverflowClipRect(context.current.paint_offset));
      IntPoint bounds_offset = clip_rect.Location();
      IntSize container_bounds = clip_rect.Size();
      IntSize scroll_bounds = scrollable_area->ContentsSize();

      bool user_scrollable_horizontal =
          scrollable_area->UserInputScrollable(kHorizontalScrollbar);
      bool user_scrollable_vertical =
          scrollable_area->UserInputScrollable(kVerticalScrollbar);

      auto ancestor_reasons =
          context.current.scroll->GetMainThreadScrollingReasons();
      auto reasons = GetMainThreadScrollingReasons(object, ancestor_reasons);

      // Main thread scrolling reasons depend on their ancestor's reasons
      // so ensure the entire subtree is updated when reasons change.
      if (auto* existing_scroll = properties.Scroll()) {
        if (existing_scroll->GetMainThreadScrollingReasons() != reasons)
          force_subtree_update = true;
      }

      auto element_id = CompositorElementIdFromLayoutObjectId(
          object.UniqueId(), CompositorElementIdNamespace::kScrollTranslation);

      // TODO(pdr): Set the correct compositing reasons here.
      auto result = properties.UpdateScroll(
          context.current.scroll, bounds_offset, container_bounds,
          scroll_bounds, user_scrollable_horizontal, user_scrollable_vertical,
          reasons, element_id, scrollable_area);
      force_subtree_update |= result.NewNodeCreated();
    } else {
      // Ensure pre-existing properties are cleared.
      force_subtree_update |= properties.ClearScroll();
    }

    // A scroll translation node is created for static offset (e.g., overflow
    // hidden with scroll offset) or cases that scroll and have a scroll node.
    if (NeedsScrollOrScrollTranslation(object)) {
      const LayoutBox& box = ToLayoutBox(object);
      IntSize scroll_offset = box.ScrolledContentOffset();
      TransformationMatrix scroll_offset_matrix =
          TransformationMatrix().Translate(-scroll_offset.Width(),
                                           -scroll_offset.Height());
      auto result = properties.UpdateScrollTranslation(
          context.current.transform, scroll_offset_matrix, FloatPoint3D(),
          context.current.should_flatten_inherited_transform,
          context.current.rendering_context_id, kCompositingReasonNone,
          CompositorElementId(), properties.Scroll());
      force_subtree_update |= result.NewNodeCreated();
    } else {
      // Ensure pre-existing properties are cleared.
      force_subtree_update |= properties.ClearScrollTranslation();
    }
  }

  if (properties.Scroll())
    context.current.scroll = properties.Scroll();
  if (properties.ScrollTranslation()) {
    context.current.transform = properties.ScrollTranslation();
    context.current.should_flatten_inherited_transform = false;
  }
}

void PaintPropertyTreeBuilder::UpdateOutOfFlowContext(
    const LayoutObject& object,
    PaintPropertyTreeBuilderFragmentContext& context,
    ObjectPaintProperties* paint_properties,
    bool& force_subtree_update) {
  if (!object.IsBoxModelObject() && !paint_properties)
    return;

  if (object.IsLayoutBlock())
    context.paint_offset_for_float = context.current.paint_offset;

  if (object.CanContainAbsolutePositionObjects())
    context.absolute_position = context.current;

  if (object.IsLayoutView()) {
    if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      const auto* initial_fixed_transform = context.fixed_position.transform;
      const auto* initial_fixed_scroll = context.fixed_position.scroll;

      context.fixed_position = context.current;

      // Fixed position transform and scroll nodes should not be affected.
      context.fixed_position.transform = initial_fixed_transform;
      context.fixed_position.scroll = initial_fixed_scroll;
    }
  } else if (object.CanContainFixedPositionObjects()) {
    context.fixed_position = context.current;
  } else if (paint_properties && paint_properties->CssClip()) {
    // CSS clip applies to all descendants, even if this object is not a
    // containing block ancestor of the descendant. It is okay for
    // absolute-position descendants because having CSS clip implies being
    // absolute position container. However for fixed-position descendants we
    // need to insert the clip here if we are not a containing block ancestor of
    // them.
    auto* css_clip = paint_properties->CssClip();

    // Before we actually create anything, check whether in-flow context and
    // fixed-position context has exactly the same clip. Reuse if possible.
    if (context.fixed_position.clip == css_clip->Parent()) {
      context.fixed_position.clip = css_clip;
    } else {
      if (object.NeedsPaintPropertyUpdate() || force_subtree_update) {
        auto result = paint_properties->UpdateCssClipFixedPosition(
            context.fixed_position.clip,
            const_cast<TransformPaintPropertyNode*>(
                css_clip->LocalTransformSpace()),
            css_clip->ClipRect());
        force_subtree_update |= result.NewNodeCreated();
      }
      if (paint_properties->CssClipFixedPosition())
        context.fixed_position.clip = paint_properties->CssClipFixedPosition();
      return;
    }
  }

  if (object.NeedsPaintPropertyUpdate() || force_subtree_update) {
    if (paint_properties)
      force_subtree_update |= paint_properties->ClearCssClipFixedPosition();
  }
}

void PaintPropertyTreeBuilder::UpdatePaintOffset(
    const LayoutBoxModelObject& object,
    PaintPropertyTreeBuilderFragmentContext& context,
    const LayoutObject* container_for_absolute_position) {
  if (object.IsFloating())
    context.current.paint_offset = context.paint_offset_for_float;

  // Multicolumn spanners are painted starting at the multicolumn container (but
  // still inherit properties in layout-tree order) so reset the paint offset.
  if (object.IsColumnSpanAll())
    context.current.paint_offset = object.Container()->PaintOffset();

  switch (object.StyleRef().GetPosition()) {
    case EPosition::kStatic:
      break;
    case EPosition::kRelative:
      context.current.paint_offset += object.OffsetForInFlowPosition();
      break;
    case EPosition::kAbsolute: {
      DCHECK(container_for_absolute_position == object.Container());
      context.current = context.absolute_position;

      // Absolutely positioned content in an inline should be positioned
      // relative to the inline.
      const LayoutObject* container = container_for_absolute_position;
      if (container && container->IsInFlowPositioned() &&
          container->IsLayoutInline()) {
        DCHECK(object.IsBox());
        context.current.paint_offset +=
            ToLayoutInline(container)->OffsetForInFlowPositionedInline(
                ToLayoutBox(object));
      }
      break;
    }
    case EPosition::kSticky:
      context.current.paint_offset += object.OffsetForInFlowPosition();
      break;
    case EPosition::kFixed:
      context.current = context.fixed_position;
      break;
    default:
      NOTREACHED();
  }

  if (object.IsBox()) {
    // TODO(pdr): Several calls in this function walk back up the tree to
    // calculate containers (e.g., physicalLocation, offsetForInFlowPosition*).
    // The containing block and other containers can be stored on
    // PaintPropertyTreeBuilderFragmentContext instead of recomputing them.
    context.current.paint_offset.MoveBy(ToLayoutBox(object).PhysicalLocation());
    // This is a weird quirk that table cells paint as children of table rows,
    // but their location have the row's location baked-in.
    // Similar adjustment is done in LayoutTableCell::offsetFromContainer().
    if (object.IsTableCell()) {
      LayoutObject* parent_row = object.Parent();
      DCHECK(parent_row && parent_row->IsTableRow());
      context.current.paint_offset.MoveBy(
          -ToLayoutBox(parent_row)->PhysicalLocation());
    }
  }
}

void PaintPropertyTreeBuilder::UpdateForObjectLocationAndSize(
    const LayoutObject& object,
    const LayoutObject* container_for_absolute_position,
    ObjectPaintProperties* paint_properties,
    bool& is_actually_needed,
    PaintPropertyTreeBuilderFragmentContext& context,
    bool& force_subtree_update) {
#if DCHECK_IS_ON()
  FindPaintOffsetNeedingUpdateScope check_scope(object, is_actually_needed);
#endif

  if (object.IsBoxModelObject()) {
    UpdatePaintOffset(ToLayoutBoxModelObject(object), context,
                      container_for_absolute_position);
    if (paint_properties) {
      UpdatePaintOffsetTranslation(ToLayoutBoxModelObject(object), context,
                                   *paint_properties, force_subtree_update);
    }
  }

  if (object.PaintOffset() != context.current.paint_offset) {
    // Many paint properties depend on paint offset so we force an update of
    // the entire subtree on paint offset changes.
    force_subtree_update = true;

    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      object.GetMutableForPainting().SetShouldDoFullPaintInvalidation(
          PaintInvalidationReason::kGeometry);
    }
    object.GetMutableForPainting().SetPaintOffset(context.current.paint_offset);
  }

  if (!object.IsBox())
    return;
  const LayoutBox& box = ToLayoutBox(object);
  if (box.Size() == box.PreviousSize())
    return;

  // CSS mask and clip-path comes with an implicit clip to the border box.
  // Currently only SPv2 generate and take advantage of those.
  const bool box_generates_property_nodes_for_mask_and_clip_path =
      RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      (box.HasMask() || box.HasClipPath());
  // The overflow clip paint property depends on the border box rect through
  // overflowClipRect(). The border box rect's size equals the frame rect's
  // size so we trigger a paint property update when the frame rect changes.
  if (box.ShouldClipOverflow() ||
      // The used value of CSS clip may depend on size of the box, e.g. for
      // clip: rect(auto auto auto -5px).
      box.HasClip() ||
      // Relative lengths (e.g., percentage values) in transform, perspective,
      // transform-origin, and perspective-origin can depend on the size of the
      // frame rect, so force a property update if it changes. TODO(pdr): We
      // only need to update properties if there are relative lengths.
      box.StyleRef().HasTransform() || box.StyleRef().HasPerspective() ||
      box_generates_property_nodes_for_mask_and_clip_path)
    box.GetMutableForPainting().SetNeedsPaintPropertyUpdate();
}

void PaintPropertyTreeBuilder::UpdatePaintProperties(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& full_context) {
  bool needs_paint_properties =
      NeedsPaintOffsetTranslation(object) || NeedsTransform(object) ||
      NeedsEffect(object) || NeedsTransformForNonRootSVG(object) ||
      NeedsFilter(object) || NeedsCssClip(object) ||
      NeedsScrollbarPaintOffset(object) || NeedsOverflowClip(object) ||
      NeedsPerspective(object) || NeedsSVGLocalToBorderBoxTransform(object) ||
      NeedsScrollOrScrollTranslation(object);

  // We need at least the fragment for all PaintLayers, which store their
  // local border box properties on the fragment.
  if (needs_paint_properties || object.HasLayer())
    object.GetMutableForPainting().EnsureFirstFragment();

  bool had_paint_properties =
      object.FirstFragment() && object.FirstFragment()->PaintProperties();
  if (needs_paint_properties && !had_paint_properties) {
    object.GetMutableForPainting().FirstFragment()->EnsurePaintProperties();
  } else if (!needs_paint_properties && had_paint_properties) {
    object.GetMutableForPainting().FirstFragment()->ClearPaintProperties();
    full_context.force_subtree_update = true;
  }
}

static inline bool ObjectTypeMightNeedPaintProperties(
    const LayoutObject& object) {
  return object.IsBoxModelObject() || object.IsSVG();
}

void PaintPropertyTreeBuilder::UpdatePropertiesForSelf(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& full_context) {
  PaintPropertyTreeBuilderFragmentContext& context = full_context.fragments[0];
  if (object.IsSVGHiddenContainer()) {
    // SVG resources are painted within one or more other locations in the
    // SVG during paint, and hence have their own independent paint property
    // trees, paint offset, etc.
    context = PaintPropertyTreeBuilderFragmentContext();
  }

  if (ObjectTypeMightNeedPaintProperties(object))
    UpdatePaintProperties(object, full_context);

  bool is_actually_needed = false;
#if DCHECK_IS_ON()
  is_actually_needed = full_context.is_actually_needed;
#endif
  // This is not in FindObjectPropertiesNeedingUpdateScope because paint offset
  // can change without needsPaintPropertyUpdate.
  UpdateForObjectLocationAndSize(
      object, full_context.container_for_absolute_position,
      object.FirstFragment() ? object.FirstFragment()->PaintProperties()
                             : nullptr,
      is_actually_needed, context, full_context.force_subtree_update);

#if DCHECK_IS_ON()
  FindObjectPropertiesNeedingUpdateScope check_needs_update_scope(
      object, full_context.force_subtree_update);
#endif

  if (object.FirstFragment() && object.FirstFragment()->PaintProperties()) {
    ObjectPaintProperties* properties =
        object.GetMutableForPainting().FirstFragment()->PaintProperties();
    UpdateTransform(object, *properties, context,
                    full_context.force_subtree_update);
    UpdateCssClip(object, *properties, context,
                  full_context.force_subtree_update, full_context.clip_changed);
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      UpdateEffect(object, *properties, context,
                   full_context.force_subtree_update,
                   full_context.clip_changed);
      UpdateFilter(object, *properties, context,
                   full_context.force_subtree_update);
    }
  }
  UpdateLocalBorderBoxContext(object, context, object.FirstFragment(),
                              full_context.force_subtree_update);
  if (object.FirstFragment() && object.FirstFragment()->PaintProperties()) {
    ObjectPaintProperties* properties =
        object.GetMutableForPainting().FirstFragment()->PaintProperties();
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      UpdateScrollbarPaintOffset(object, *properties, context,
                                 full_context.force_subtree_update);
    }
  }
}

void PaintPropertyTreeBuilder::UpdatePropertiesForChildren(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!ObjectTypeMightNeedPaintProperties(object))
    return;

  for (auto& fragment_context : context.fragments) {
#if DCHECK_IS_ON()
    FindObjectPropertiesNeedingUpdateScope check_needs_update_scope(
        object, context.force_subtree_update);
#endif

    if (object.FirstFragment() && object.FirstFragment()->PaintProperties()) {
      ObjectPaintProperties* properties =
          object.GetMutableForPainting().FirstFragment()->PaintProperties();
      UpdateOverflowClip(object, *properties, fragment_context,
                         context.force_subtree_update, context.clip_changed);
      UpdatePerspective(object, *properties, fragment_context,
                        context.force_subtree_update);
      UpdateSvgLocalToBorderBoxTransform(object, *properties, fragment_context,
                                         context.force_subtree_update);
      UpdateScrollAndScrollTranslation(object, *properties, fragment_context,
                                       context.force_subtree_update);
    }

    UpdateOutOfFlowContext(object, fragment_context,
                           object.FirstFragment()
                               ? object.FirstFragment()->PaintProperties()
                               : nullptr,
                           context.force_subtree_update);

    context.force_subtree_update |= object.SubtreeNeedsPaintPropertyUpdate();
  }

  if (object.CanContainAbsolutePositionObjects())
    context.container_for_absolute_position = &object;
}

}  // namespace blink
