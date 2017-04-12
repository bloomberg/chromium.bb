// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreeBuilder.h"

#include <memory>
#include "core/dom/DOMNodeIds.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
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

PaintPropertyTreeBuilderContext::PaintPropertyTreeBuilderContext()
    : container_for_absolute_position(nullptr),
      current_effect(EffectPaintPropertyNode::Root()),
      input_clip_of_current_effect(ClipPaintPropertyNode::Root()),
      force_subtree_update(false) {
  current.clip = absolute_position.clip = fixed_position.clip =
      ClipPaintPropertyNode::Root();
  current.transform = absolute_position.transform = fixed_position.transform =
      TransformPaintPropertyNode::Root();
  current.scroll = absolute_position.scroll = fixed_position.scroll =
      ScrollPaintPropertyNode::Root();
}

// True if a new property was created, false if an existing one was updated.
static bool UpdatePreTranslation(
    FrameView& frame_view,
    PassRefPtr<const TransformPaintPropertyNode> parent,
    const TransformationMatrix& matrix,
    const FloatPoint3D& origin) {
  DCHECK(!RuntimeEnabledFeatures::rootLayerScrollingEnabled());
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
    FrameView& frame_view,
    PassRefPtr<const ClipPaintPropertyNode> parent,
    PassRefPtr<const TransformPaintPropertyNode> local_transform_space,
    const FloatRoundedRect& clip_rect) {
  DCHECK(!RuntimeEnabledFeatures::rootLayerScrollingEnabled());
  if (auto* existing_content_clip = frame_view.ContentClip()) {
    existing_content_clip->Update(std::move(parent),
                                  std::move(local_transform_space), clip_rect);
    return false;
  }
  frame_view.SetContentClip(ClipPaintPropertyNode::Create(
      std::move(parent), std::move(local_transform_space), clip_rect));
  return true;
}

static CompositorElementId CreateDomNodeBasedCompositorElementId(
    const LayoutObject& object) {
  // TODO(wkorman): Centralize this implementation with similar across
  // animation, scrolling and compositing logic.
  return CreateCompositorElementId(DOMNodeIds::IdForNode(object.GetNode()),
                                   CompositorSubElementId::kPrimary);
}

// True if a new property was created or a main thread scrolling reason changed
// (which can affect descendants), false if an existing one was updated.
static bool UpdateScrollTranslation(
    FrameView& frame_view,
    PassRefPtr<const TransformPaintPropertyNode> parent,
    const TransformationMatrix& matrix,
    const FloatPoint3D& origin,
    PassRefPtr<const ScrollPaintPropertyNode> scroll_parent,
    const IntSize& clip,
    const IntSize& bounds,
    bool user_scrollable_horizontal,
    bool user_scrollable_vertical,
    MainThreadScrollingReasons main_thread_scrolling_reasons,
    WebLayerScrollClient* scroll_client) {
  DCHECK(!RuntimeEnabledFeatures::rootLayerScrollingEnabled());
  CompositorElementId compositor_element_id =
      CreateDomNodeBasedCompositorElementId(*frame_view.GetLayoutView());
  if (auto* existing_scroll_translation = frame_view.ScrollTranslation()) {
    auto existing_reasons = existing_scroll_translation->ScrollNode()
                                ->GetMainThreadScrollingReasons();
    existing_scroll_translation->UpdateScrollTranslation(
        std::move(parent), matrix, origin, false, 0, kCompositingReasonNone,
        compositor_element_id, std::move(scroll_parent), clip, bounds,
        user_scrollable_horizontal, user_scrollable_vertical,
        main_thread_scrolling_reasons, scroll_client);
    return existing_reasons != main_thread_scrolling_reasons;
  }
  frame_view.SetScrollTranslation(
      TransformPaintPropertyNode::CreateScrollTranslation(
          std::move(parent), matrix, origin, false, 0, kCompositingReasonNone,
          compositor_element_id, std::move(scroll_parent), clip, bounds,
          user_scrollable_horizontal, user_scrollable_vertical,
          main_thread_scrolling_reasons, scroll_client));
  return true;
}

static MainThreadScrollingReasons GetMainThreadScrollingReasons(
    const FrameView& frame_view,
    MainThreadScrollingReasons ancestor_reasons) {
  auto reasons = ancestor_reasons;
  if (!frame_view.GetFrame().GetSettings()->GetThreadedScrollingEnabled())
    reasons |= MainThreadScrollingReason::kThreadedScrollingDisabled;
  if (frame_view.HasBackgroundAttachmentFixedObjects())
    reasons |= MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  return reasons;
}

void PaintPropertyTreeBuilder::UpdateProperties(
    FrameView& frame_view,
    PaintPropertyTreeBuilderContext& context) {
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    // With root layer scrolling, the LayoutView (a LayoutObject) properties are
    // updated like other objects (see updatePropertiesAndContextForSelf and
    // updatePropertiesAndContextForChildren) instead of needing LayoutView-
    // specific property updates here.
    context.current.paint_offset.MoveBy(frame_view.Location());
    context.current.rendering_context_id = 0;
    context.current.should_flatten_inherited_transform = true;
    context.absolute_position = context.current;
    context.container_for_absolute_position = nullptr;
    context.fixed_position = context.current;
    return;
  }

#if DCHECK_IS_ON()
  FindFrameViewPropertiesNeedingUpdateScope check_scope(&frame_view, context);
#endif

  if (frame_view.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
    TransformationMatrix frame_translate;
    frame_translate.Translate(
        frame_view.X() + context.current.paint_offset.X(),
        frame_view.Y() + context.current.paint_offset.Y());
    context.force_subtree_update |= UpdatePreTranslation(
        frame_view, context.current.transform, frame_translate, FloatPoint3D());

    FloatRoundedRect content_clip(
        IntRect(IntPoint(), frame_view.VisibleContentSize()));
    context.force_subtree_update |=
        UpdateContentClip(frame_view, context.current.clip,
                          frame_view.PreTranslation(), content_clip);

    ScrollOffset scroll_offset = frame_view.GetScrollOffset();
    if (frame_view.IsScrollable() || !scroll_offset.IsZero()) {
      TransformationMatrix frame_scroll;
      frame_scroll.Translate(-scroll_offset.Width(), -scroll_offset.Height());

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

      context.force_subtree_update |= UpdateScrollTranslation(
          frame_view, frame_view.PreTranslation(), frame_scroll, FloatPoint3D(),
          context.current.scroll, scroll_clip, scroll_bounds,
          user_scrollable_horizontal, user_scrollable_vertical, reasons,
          frame_view.GetScrollableArea());
    } else {
      if (frame_view.ScrollTranslation()) {
        // Ensure pre-existing properties are cleared if there is no scrolling.
        frame_view.SetScrollTranslation(nullptr);
        // Rebuild all descendant properties because a property was removed.
        context.force_subtree_update = true;
      }
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
  if (const auto* scroll_translation = frame_view.ScrollTranslation()) {
    context.current.transform = scroll_translation;
    context.current.scroll = scroll_translation->ScrollNode();
  }
  context.current.paint_offset = LayoutPoint();
  context.current.rendering_context_id = 0;
  context.current.should_flatten_inherited_transform = true;
  context.absolute_position = context.current;
  context.container_for_absolute_position = nullptr;
  context.fixed_position = context.current;
  context.fixed_position.transform = fixed_transform_node;
  context.fixed_position.scroll = fixed_scroll_node;

  std::unique_ptr<PropertyTreeState> contents_state(new PropertyTreeState(
      context.current.transform, context.current.clip, context.current_effect));
  frame_view.SetTotalPropertyTreeStateForContents(std::move(contents_state));
}

void PaintPropertyTreeBuilder::UpdatePaintOffsetTranslation(
    const LayoutBoxModelObject& object,
    PaintPropertyTreeBuilderContext& context) {
  bool uses_paint_offset_translation = false;
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled() &&
      object.IsLayoutView()) {
    // Root layer scrolling always creates a translation node for LayoutView to
    // ensure fixed and absolute contexts use the correct transform space.
    uses_paint_offset_translation = true;
  } else if (object.HasLayer() &&
             context.current.paint_offset != LayoutPoint() &&
             object.Layer()->PaintsWithTransform(
                 kGlobalPaintFlattenCompositingLayers)) {
    uses_paint_offset_translation = true;
  }

  if (!uses_paint_offset_translation) {
    if (auto* properties = object.GetMutableForPainting().PaintProperties())
      context.force_subtree_update |= properties->ClearPaintOffsetTranslation();
    return;
  }

  // We should use the same subpixel paint offset values for snapping
  // regardless of whether a transform is present. If there is a transform
  // we round the paint offset but keep around the residual fractional
  // component for the transformed content to paint with.  In spv1 this was
  // called "subpixel accumulation". For more information, see
  // PaintLayer::subpixelAccumulation() and
  // PaintLayerPainter::paintFragmentByApplyingTransform.
  IntPoint rounded_paint_offset = RoundedIntPoint(context.current.paint_offset);
  LayoutPoint fractional_paint_offset =
      LayoutPoint(context.current.paint_offset - rounded_paint_offset);

  auto& properties = object.GetMutableForPainting().EnsurePaintProperties();
  context.force_subtree_update |= properties.UpdatePaintOffsetTranslation(
      context.current.transform,
      TransformationMatrix().Translate(rounded_paint_offset.X(),
                                       rounded_paint_offset.Y()),
      FloatPoint3D(), context.current.should_flatten_inherited_transform,
      context.current.rendering_context_id);

  context.current.transform = properties.PaintOffsetTranslation();
  context.current.paint_offset = fractional_paint_offset;
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled() &&
      object.IsLayoutView()) {
    context.absolute_position.transform = properties.PaintOffsetTranslation();
    context.fixed_position.transform = properties.PaintOffsetTranslation();
    context.absolute_position.paint_offset = LayoutPoint();
    context.fixed_position.paint_offset = LayoutPoint();
  }
}

// SVG does not use the general transform update of |updateTransform|, instead
// creating a transform node for SVG-specific transforms without 3D.
void PaintPropertyTreeBuilder::UpdateTransformForNonRootSVG(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  DCHECK(object.IsSVGChild());
  // SVG does not use paint offset internally, except for SVGForeignObject which
  // has different SVG and HTML coordinate spaces.
  DCHECK(object.IsSVGForeignObject() ||
         context.current.paint_offset == LayoutPoint());

  if (object.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
    AffineTransform transform = object.LocalToSVGParentTransform();
    // TODO(pdr): Check for the presence of a transform instead of the value.
    // Checking for an identity matrix will cause the property tree structure
    // to change during animations if the animation passes through the
    // identity matrix.
    if (!transform.IsIdentity()) {
      // The origin is included in the local transform, so leave origin empty.
      auto& properties = object.GetMutableForPainting().EnsurePaintProperties();
      context.force_subtree_update |= properties.UpdateTransform(
          context.current.transform, TransformationMatrix(transform),
          FloatPoint3D());
    } else {
      if (auto* properties = object.GetMutableForPainting().PaintProperties())
        context.force_subtree_update |= properties->ClearTransform();
    }
  }

  if (object.PaintProperties() && object.PaintProperties()->Transform()) {
    context.current.transform = object.PaintProperties()->Transform();
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
    if (style.UsedTransformStyle3D() == kTransformStyle3DPreserve3D)
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

void PaintPropertyTreeBuilder::UpdateTransform(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.IsSVGChild()) {
    UpdateTransformForNonRootSVG(object, context);
    return;
  }

  if (object.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
    const ComputedStyle& style = object.StyleRef();

    // A transform node is allocated for transforms, preserves-3d and any
    // direct compositing reason. The latter is required because this is the
    // only way to represent compositing both an element and its stacking
    // descendants.
    bool has_transform = false;
    if (object.IsBox()) {
      auto& box = ToLayoutBox(object);

      CompositingReasons compositing_reasons =
          CompositingReasonsForTransform(box);

      if (style.HasTransform() || style.Preserves3D() ||
          compositing_reasons != kCompositingReasonNone) {
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

        CompositorElementId compositor_element_id =
            style.HasCurrentTransformAnimation()
                ? CreateDomNodeBasedCompositorElementId(object)
                : CompositorElementId();

        auto& properties =
            object.GetMutableForPainting().EnsurePaintProperties();
        context.force_subtree_update |= properties.UpdateTransform(
            context.current.transform, matrix, TransformOrigin(box),
            context.current.should_flatten_inherited_transform,
            rendering_context_id, compositing_reasons, compositor_element_id);
        has_transform = true;
      }
    }
    if (!has_transform) {
      if (auto* properties = object.GetMutableForPainting().PaintProperties())
        context.force_subtree_update |= properties->ClearTransform();
    }
  }

  const auto* properties = object.PaintProperties();
  if (properties && properties->Transform()) {
    context.current.transform = properties->Transform();
    if (object.StyleRef().Preserves3D()) {
      context.current.rendering_context_id =
          properties->Transform()->RenderingContextId();
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

void PaintPropertyTreeBuilder::UpdateEffect(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  const ComputedStyle& style = object.StyleRef();

  const bool is_css_isolated_group =
      object.IsBoxModelObject() && style.IsStackingContext();
  if (!is_css_isolated_group && !object.IsSVGChild()) {
    if (object.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
      if (auto* properties = object.GetMutableForPainting().PaintProperties()) {
        context.force_subtree_update |= properties->ClearEffect();
        context.force_subtree_update |= properties->ClearMask();
        context.force_subtree_update |= properties->ClearMaskClip();
      }
    }
    return;
  }

  // TODO(trchen): Can't omit effect node if we have 3D children.
  if (object.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
    const ClipPaintPropertyNode* output_clip =
        context.input_clip_of_current_effect;

    bool effect_node_needed = false;

    // Can't omit effect node if we have paint children with exotic blending.
    if (object.IsSVG()) {
      // This handles SVGRoot objects which have PaintLayers.
      if (object.IsSVGRoot() && object.HasNonIsolatedBlendingDescendants())
        effect_node_needed = true;
      else if (SVGLayoutSupport::IsIsolationRequired(&object))
        effect_node_needed = true;
    } else if (PaintLayer* layer = ToLayoutBoxModelObject(object).Layer()) {
      if (layer->HasNonIsolatedDescendantWithBlendMode())
        effect_node_needed = true;
    }

    SkBlendMode blend_mode = object.IsBlendingAllowed()
                                 ? WebCoreCompositeToSkiaComposite(
                                       kCompositeSourceOver, style.BlendMode())
                                 : SkBlendMode::kSrcOver;
    if (blend_mode != SkBlendMode::kSrcOver)
      effect_node_needed = true;

    float opacity = style.Opacity();
    if (opacity != 1.0f)
      effect_node_needed = true;

    // We may begin to composite our subtree prior to an animation starts,
    // but a compositor element ID is only needed when an animation is current.
    CompositorElementId compositor_element_id =
        style.HasCurrentOpacityAnimation()
            ? CreateDomNodeBasedCompositorElementId(object)
            : CompositorElementId();
    CompositingReasons compositing_reasons = kCompositingReasonNone;
    if (CompositingReasonFinder::RequiresCompositingForOpacityAnimation(
            style)) {
      compositing_reasons = kCompositingReasonActiveAnimation;
      effect_node_needed = true;
    }
    DCHECK(!style.HasCurrentOpacityAnimation() ||
           compositing_reasons != kCompositingReasonNone);

    IntRect mask_clip;
    ColorFilter mask_color_filter;
    bool has_mask = ComputeMaskParameters(mask_clip, mask_color_filter, object,
                                          context.current.paint_offset);
    if (has_mask) {
      effect_node_needed = true;

      auto& properties = object.GetMutableForPainting().EnsurePaintProperties();
      context.force_subtree_update |= properties.UpdateMaskClip(
          context.current.clip, context.current.transform,
          FloatRoundedRect(mask_clip));
      output_clip = properties.MaskClip();

      // TODO(crbug.com/683425): PaintArtifactCompositor does not handle
      // grouping (i.e. descendant-dependent compositing reason) properly yet.
      // This forces masked subtree always create a layer for now.
      compositing_reasons |= kCompositingReasonIsolateCompositedDescendants;
    } else {
      if (auto* properties = object.GetMutableForPainting().PaintProperties())
        context.force_subtree_update |= properties->ClearMaskClip();
    }

    if (effect_node_needed) {
      auto& properties = object.GetMutableForPainting().EnsurePaintProperties();
      context.force_subtree_update |= properties.UpdateEffect(
          context.current_effect, context.current.transform, output_clip,
          kColorFilterNone, CompositorFilterOperations(), opacity, blend_mode,
          compositing_reasons, compositor_element_id);
      if (has_mask) {
        // TODO(crbug.com/683425): PaintArtifactCompositor does not handle
        // grouping (i.e. descendant-dependent compositing reason) properly yet.
        // Adding CompositingReasonSquashingDisallowed forces mask not getting
        // squashed into a child effect. Have no compositing reason otherwise.
        context.force_subtree_update |= properties.UpdateMask(
            properties.Effect(), context.current.transform, output_clip,
            mask_color_filter, CompositorFilterOperations(), 1.f,
            SkBlendMode::kDstIn, kCompositingReasonSquashingDisallowed,
            CompositorElementId());
      } else {
        context.force_subtree_update |= properties.ClearMask();
      }
    } else {
      if (auto* properties = object.GetMutableForPainting().PaintProperties()) {
        context.force_subtree_update |= properties->ClearEffect();
        context.force_subtree_update |= properties->ClearMask();
      }
    }
  }

  const auto* properties = object.PaintProperties();
  if (properties && properties->Effect()) {
    context.current_effect = properties->Effect();
    if (properties->MaskClip()) {
      context.input_clip_of_current_effect = context.current.clip =
          context.absolute_position.clip = context.fixed_position.clip =
              properties->MaskClip();
    }
  }
}

void PaintPropertyTreeBuilder::UpdateFilter(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  const ComputedStyle& style = object.StyleRef();

  if (object.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
    CompositorFilterOperations filter;
    if (object.IsSVGChild()) {
      // TODO(trchen): SVG caches filters in SVGResources. Implement it.
    } else if (PaintLayer* layer = ToLayoutBoxModelObject(object).Layer()) {
      // TODO(trchen): Eliminate PaintLayer dependency.
      filter = layer->CreateCompositorFilterOperationsForFilter(style);
    }

    // The CSS filter spec didn't specify how filters interact with overflow
    // clips. The implementation here mimics the old Blink/WebKit behavior for
    // backward compatibility.
    // Basically the output of the filter will be affected by clips that applies
    // to the current element. The descendants that paints into the input of the
    // filter ignores any clips collected so far. For example:
    // <div style="overflow:scroll">
    //   <div style="filter:blur(1px);">
    //     <div>A</div>
    //     <div style="position:absolute;">B</div>
    //   </div>
    // </div>
    // In this example "A" should be clipped if the filter was not present.
    // With the filter, "A" will be rastered without clipping, but instead
    // the blurred result will be clipped.
    // On the other hand, "B" should not be clipped because the overflow clip is
    // not in its containing block chain, but as the filter output will be
    // clipped, so a blurred "B" may still be invisible.
    const ClipPaintPropertyNode* output_clip = context.current.clip;

    // TODO(trchen): A filter may contain spatial operations such that an
    // output pixel may depend on an input pixel outside of the output clip.
    // We should generate a special clip node to represent this expansion.

    // We may begin to composite our subtree prior to an animation starts,
    // but a compositor element ID is only needed when an animation is current.
    CompositorElementId compositor_element_id =
        style.HasCurrentFilterAnimation()
            ? CreateDomNodeBasedCompositorElementId(object)
            : CompositorElementId();
    CompositingReasons compositing_reasons =
        CompositingReasonFinder::RequiresCompositingForFilterAnimation(style)
            ? kCompositingReasonActiveAnimation
            : kCompositingReasonNone;
    DCHECK(!style.HasCurrentFilterAnimation() ||
           compositing_reasons != kCompositingReasonNone);

    if (compositing_reasons == kCompositingReasonNone && filter.IsEmpty()) {
      if (auto* properties = object.GetMutableForPainting().PaintProperties())
        context.force_subtree_update |= properties->ClearFilter();
    } else {
      auto& properties = object.GetMutableForPainting().EnsurePaintProperties();
      context.force_subtree_update |= properties.UpdateFilter(
          context.current_effect, context.current.transform, output_clip,
          kColorFilterNone, std::move(filter), 1.f, SkBlendMode::kSrcOver,
          compositing_reasons, compositor_element_id);
    }
  }

  const auto* properties = object.PaintProperties();
  if (properties && properties->Filter()) {
    context.current_effect = properties->Filter();
    // TODO(trchen): Change input clip to expansion hint once implemented.
    const ClipPaintPropertyNode* input_clip =
        properties->Filter()->OutputClip();
    context.input_clip_of_current_effect = context.current.clip =
        context.absolute_position.clip = context.fixed_position.clip =
            input_clip;
  }
}

void PaintPropertyTreeBuilder::UpdateCssClip(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
    if (object.HasClip()) {
      // Create clip node for descendants that are not fixed position.
      // We don't have to setup context.absolutePosition.clip here because this
      // object must be a container for absolute position descendants, and will
      // copy from in-flow context later at updateOutOfFlowContext() step.
      DCHECK(object.CanContainAbsolutePositionObjects());
      LayoutRect clip_rect =
          ToLayoutBox(object).ClipRect(context.current.paint_offset);
      auto& properties = object.GetMutableForPainting().EnsurePaintProperties();
      context.force_subtree_update |= properties.UpdateCssClip(
          context.current.clip, context.current.transform,
          FloatRoundedRect(FloatRect(clip_rect)));
    } else {
      if (auto* properties = object.GetMutableForPainting().PaintProperties())
        context.force_subtree_update |= properties->ClearCssClip();
    }
  }

  const auto* properties = object.PaintProperties();
  if (properties && properties->CssClip())
    context.current.clip = properties->CssClip();
}

void PaintPropertyTreeBuilder::UpdateLocalBorderBoxContext(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.NeedsPaintPropertyUpdate() && !context.force_subtree_update)
    return;

  // We only need to cache the local border box properties for layered objects.
  if (!object.HasLayer()) {
    object.GetMutableForPainting().ClearLocalBorderBoxProperties();
  } else {
    PropertyTreeState local_border_box =
        PropertyTreeState(context.current.transform, context.current.clip,
                          context.current_effect);
    object.GetMutableForPainting().SetLocalBorderBoxProperties(
        local_border_box);
  }
}

// TODO(trchen): Remove this once we bake the paint offset into frameRect.
void PaintPropertyTreeBuilder::UpdateScrollbarPaintOffset(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.NeedsPaintPropertyUpdate() && !context.force_subtree_update)
    return;

  bool needs_scrollbar_paint_offset = false;
  IntPoint rounded_paint_offset = RoundedIntPoint(context.current.paint_offset);
  if (rounded_paint_offset != IntPoint() && object.IsBoxModelObject()) {
    if (auto* area = ToLayoutBoxModelObject(object).GetScrollableArea()) {
      if (area->HorizontalScrollbar() || area->VerticalScrollbar()) {
        auto paint_offset = TransformationMatrix().Translate(
            rounded_paint_offset.X(), rounded_paint_offset.Y());
        auto& properties =
            object.GetMutableForPainting().EnsurePaintProperties();
        context.force_subtree_update |= properties.UpdateScrollbarPaintOffset(
            context.current.transform, paint_offset, FloatPoint3D());
        needs_scrollbar_paint_offset = true;
      }
    }
  }

  auto* properties = object.GetMutableForPainting().PaintProperties();
  if (!needs_scrollbar_paint_offset && properties)
    context.force_subtree_update |= properties->ClearScrollbarPaintOffset();
}

void PaintPropertyTreeBuilder::UpdateOverflowClip(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.IsBox())
    return;

  if (object.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
    const LayoutBox& box = ToLayoutBox(object);
    // The <input> elements can't have contents thus CSS overflow property
    // doesn't apply.  However for layout purposes we do generate child layout
    // objects for them, e.g. button label.  We should clip the overflow from
    // those children. This is called control clip and we technically treat them
    // like overflow clip.
    LayoutRect clip_rect;
    if (box.ShouldClipOverflow()) {
      clip_rect =
          LayoutRect(box.OverflowClipRect(context.current.paint_offset));
    } else {
      if (auto* properties = object.GetMutableForPainting().PaintProperties()) {
        context.force_subtree_update |=
            properties->ClearInnerBorderRadiusClip();
        context.force_subtree_update |= properties->ClearOverflowClip();
      }
      return;
    }

    auto& properties = object.GetMutableForPainting().EnsurePaintProperties();
    const auto* current_clip = context.current.clip;
    if (box.StyleRef().HasBorderRadius()) {
      auto inner_border = box.StyleRef().GetRoundedInnerBorderFor(
          LayoutRect(context.current.paint_offset, box.Size()));
      context.force_subtree_update |= properties.UpdateInnerBorderRadiusClip(
          context.current.clip, context.current.transform, inner_border);
      current_clip = properties.InnerBorderRadiusClip();
    } else {
      context.force_subtree_update |= properties.ClearInnerBorderRadiusClip();
    }

    context.force_subtree_update |=
        properties.UpdateOverflowClip(current_clip, context.current.transform,
                                      FloatRoundedRect(FloatRect(clip_rect)));
  }

  const auto* properties = object.PaintProperties();
  if (properties && properties->OverflowClip())
    context.current.clip = properties->OverflowClip();
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

void PaintPropertyTreeBuilder::UpdatePerspective(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
    const ComputedStyle& style = object.StyleRef();
    if (object.IsBox() && style.HasPerspective()) {
      // The perspective node must not flatten (else nothing will get
      // perspective), but it should still extend the rendering context as
      // most transform nodes do.
      TransformationMatrix matrix =
          TransformationMatrix().ApplyPerspective(style.Perspective());
      FloatPoint3D origin = PerspectiveOrigin(ToLayoutBox(object)) +
                            ToLayoutSize(context.current.paint_offset);
      auto& properties = object.GetMutableForPainting().EnsurePaintProperties();
      context.force_subtree_update |= properties.UpdatePerspective(
          context.current.transform, matrix, origin,
          context.current.should_flatten_inherited_transform,
          context.current.rendering_context_id);
    } else {
      if (auto* properties = object.GetMutableForPainting().PaintProperties())
        context.force_subtree_update |= properties->ClearPerspective();
    }
  }

  const auto* properties = object.PaintProperties();
  if (properties && properties->Perspective()) {
    context.current.transform = properties->Perspective();
    context.current.should_flatten_inherited_transform = false;
  }
}

void PaintPropertyTreeBuilder::UpdateSvgLocalToBorderBoxTransform(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.IsSVGRoot())
    return;

  if (object.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
    AffineTransform transform_to_border_box =
        SVGRootPainter(ToLayoutSVGRoot(object))
            .TransformToPixelSnappedBorderBox(context.current.paint_offset);
    if (!transform_to_border_box.IsIdentity()) {
      auto& properties = object.GetMutableForPainting().EnsurePaintProperties();
      context.force_subtree_update |=
          properties.UpdateSvgLocalToBorderBoxTransform(
              context.current.transform, transform_to_border_box,
              FloatPoint3D());
    } else {
      if (auto* properties = object.GetMutableForPainting().PaintProperties()) {
        context.force_subtree_update |=
            properties->ClearSvgLocalToBorderBoxTransform();
      }
    }
  }

  const auto* properties = object.PaintProperties();
  if (properties && properties->SvgLocalToBorderBoxTransform()) {
    context.current.transform = properties->SvgLocalToBorderBoxTransform();
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

void PaintPropertyTreeBuilder::UpdateScrollAndScrollTranslation(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
    bool needs_scroll_properties = false;
    if (object.HasOverflowClip()) {
      auto ancestor_reasons =
          context.current.scroll->GetMainThreadScrollingReasons();
      auto reasons = GetMainThreadScrollingReasons(object, ancestor_reasons);
      bool scroll_node_needed_for_main_thread_reasons =
          ancestor_reasons != reasons;

      const LayoutBox& box = ToLayoutBox(object);
      auto* scrollable_area = box.GetScrollableArea();
      IntSize scroll_offset = box.ScrolledContentOffset();
      if (scroll_node_needed_for_main_thread_reasons ||
          !scroll_offset.IsZero() || scrollable_area->ScrollsOverflow()) {
        needs_scroll_properties = true;
        auto& properties =
            object.GetMutableForPainting().EnsurePaintProperties();

        IntSize scroll_clip = scrollable_area->VisibleContentRect().Size();
        IntSize scroll_bounds = scrollable_area->ContentsSize();
        bool user_scrollable_horizontal =
            scrollable_area->UserInputScrollable(kHorizontalScrollbar);
        bool user_scrollable_vertical =
            scrollable_area->UserInputScrollable(kVerticalScrollbar);

        // Main thread scrolling reasons depend on their ancestor's reasons
        // so ensure the entire subtree is updated when reasons change.
        if (auto* existing_scroll_translation =
                properties.ScrollTranslation()) {
          auto* existing_scroll_node =
              existing_scroll_translation->ScrollNode();
          if (existing_scroll_node->GetMainThreadScrollingReasons() != reasons)
            context.force_subtree_update = true;
        }

        CompositorElementId compositor_element_id =
            CreateDomNodeBasedCompositorElementId(object);
        TransformationMatrix matrix = TransformationMatrix().Translate(
            -scroll_offset.Width(), -scroll_offset.Height());
        context.force_subtree_update |= properties.UpdateScrollTranslation(
            context.current.transform, matrix, FloatPoint3D(),
            context.current.should_flatten_inherited_transform,
            context.current.rendering_context_id, kCompositingReasonNone,
            compositor_element_id, context.current.scroll, scroll_clip,
            scroll_bounds, user_scrollable_horizontal, user_scrollable_vertical,
            reasons, scrollable_area);
      }
    }

    if (!needs_scroll_properties) {
      // Ensure pre-existing properties are cleared.
      if (auto* properties = object.GetMutableForPainting().PaintProperties())
        context.force_subtree_update |= properties->ClearScrollTranslation();
    }
  }

  if (object.PaintProperties() &&
      object.PaintProperties()->ScrollTranslation()) {
    context.current.transform = object.PaintProperties()->ScrollTranslation();
    context.current.scroll = context.current.transform->ScrollNode();
    context.current.should_flatten_inherited_transform = false;
  }
}

void PaintPropertyTreeBuilder::UpdateOutOfFlowContext(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.IsLayoutBlock())
    context.paint_offset_for_float = context.current.paint_offset;

  if (object.CanContainAbsolutePositionObjects()) {
    context.absolute_position = context.current;
    context.container_for_absolute_position = &object;
  }

  if (object.IsLayoutView()) {
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      const auto* initial_fixed_transform = context.fixed_position.transform;
      const auto* initial_fixed_scroll = context.fixed_position.scroll;

      context.fixed_position = context.current;

      // Fixed position transform and scroll nodes should not be affected.
      context.fixed_position.transform = initial_fixed_transform;
      context.fixed_position.scroll = initial_fixed_scroll;
    }
  } else if (object.CanContainFixedPositionObjects()) {
    context.fixed_position = context.current;
  } else if (object.GetMutableForPainting().PaintProperties() &&
             object.PaintProperties()->CssClip()) {
    // CSS clip applies to all descendants, even if this object is not a
    // containing block ancestor of the descendant. It is okay for
    // absolute-position descendants because having CSS clip implies being
    // absolute position container. However for fixed-position descendants we
    // need to insert the clip here if we are not a containing block ancestor of
    // them.
    auto* css_clip =
        object.GetMutableForPainting().PaintProperties()->CssClip();

    // Before we actually create anything, check whether in-flow context and
    // fixed-position context has exactly the same clip. Reuse if possible.
    if (context.fixed_position.clip == css_clip->Parent()) {
      context.fixed_position.clip = css_clip;
    } else {
      if (object.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
        auto& properties =
            object.GetMutableForPainting().EnsurePaintProperties();
        context.force_subtree_update |= properties.UpdateCssClipFixedPosition(
            context.fixed_position.clip,
            const_cast<TransformPaintPropertyNode*>(
                css_clip->LocalTransformSpace()),
            css_clip->ClipRect());
      }
      const auto* properties = object.PaintProperties();
      if (properties && properties->CssClipFixedPosition())
        context.fixed_position.clip = properties->CssClipFixedPosition();
      return;
    }
  }

  if (object.NeedsPaintPropertyUpdate() || context.force_subtree_update) {
    if (auto* properties = object.GetMutableForPainting().PaintProperties())
      context.force_subtree_update |= properties->ClearCssClipFixedPosition();
  }
}

void PaintPropertyTreeBuilder::UpdatePaintOffset(
    const LayoutBoxModelObject& object,
    PaintPropertyTreeBuilderContext& context) {
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
      DCHECK(context.container_for_absolute_position == object.Container());
      context.current = context.absolute_position;

      // Absolutely positioned content in an inline should be positioned
      // relative to the inline.
      const LayoutObject* container = context.container_for_absolute_position;
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
      ASSERT_NOT_REACHED();
  }

  if (object.IsBox()) {
    // TODO(pdr): Several calls in this function walk back up the tree to
    // calculate containers (e.g., physicalLocation, offsetForInFlowPosition*).
    // The containing block and other containers can be stored on
    // PaintPropertyTreeBuilderContext instead of recomputing them.
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
    PaintPropertyTreeBuilderContext& context) {
#if DCHECK_IS_ON()
  FindPaintOffsetNeedingUpdateScope check_scope(object, context);
#endif

  if (object.IsBoxModelObject()) {
    UpdatePaintOffset(ToLayoutBoxModelObject(object), context);
    UpdatePaintOffsetTranslation(ToLayoutBoxModelObject(object), context);
  }

  if (object.PaintOffset() != context.current.paint_offset) {
    // Many paint properties depend on paint offset so we force an update of
    // the entire subtree on paint offset changes.
    context.force_subtree_update = true;

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      object.GetMutableForPainting().SetShouldDoFullPaintInvalidation(
          kPaintInvalidationLocationChange);
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
      RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
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

void PaintPropertyTreeBuilder::UpdatePropertiesForSelf(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.IsSVGHiddenContainer()) {
    // SVG resources are painted within one or more other locations in the
    // SVG during paint, and hence have their own independent paint property
    // trees, paint offset, etc.
    context = PaintPropertyTreeBuilderContext();
  }

  // This is not in FindObjectPropertiesNeedingUpdateScope because paint offset
  // can change without needsPaintPropertyUpdate.
  UpdateForObjectLocationAndSize(object, context);

#if DCHECK_IS_ON()
  FindObjectPropertiesNeedingUpdateScope check_needs_update_scope(object,
                                                                  context);
#endif

  if (object.IsBoxModelObject() || object.IsSVG()) {
    UpdateTransform(object, context);
    UpdateCssClip(object, context);
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      UpdateEffect(object, context);
      UpdateFilter(object, context);
    }
    UpdateLocalBorderBoxContext(object, context);
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
      UpdateScrollbarPaintOffset(object, context);
  }
}

void PaintPropertyTreeBuilder::UpdatePropertiesForChildren(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
#if DCHECK_IS_ON()
  FindObjectPropertiesNeedingUpdateScope check_needs_update_scope(object,
                                                                  context);
#endif

  if (!object.IsBoxModelObject() && !object.IsSVG())
    return;

  UpdateOverflowClip(object, context);
  UpdatePerspective(object, context);
  UpdateSvgLocalToBorderBoxTransform(object, context);
  UpdateScrollAndScrollTranslation(object, context);
  UpdateOutOfFlowContext(object, context);

  context.force_subtree_update |= object.SubtreeNeedsPaintPropertyUpdate();
}

}  // namespace blink
