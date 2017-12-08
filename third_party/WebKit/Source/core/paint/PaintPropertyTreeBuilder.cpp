// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreeBuilder.h"

#include <memory>
#include "core/dom/DOMNodeIds.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/FragmentainerIterator.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutView.h"
#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/paint/FindPaintOffsetAndVisualRectNeedingUpdate.h"
#include "core/paint/FindPropertiesNeedingUpdate.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/SVGRootPainter.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "core/paint/compositing/CompositingReasonFinder.h"
#include "platform/geometry/TransformState.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

PaintPropertyTreeBuilderFragmentContext::
    PaintPropertyTreeBuilderFragmentContext()
    : current_effect(EffectPaintPropertyNode::Root()) {
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
    scoped_refptr<const TransformPaintPropertyNode> parent,
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
    scoped_refptr<const ClipPaintPropertyNode> parent,
    scoped_refptr<const TransformPaintPropertyNode> local_transform_space,
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

// True if a new property was created or a main thread scrolling reason changed
// (which can affect descendants), false if an existing one was updated.
static bool UpdateScroll(LocalFrameView& frame_view,
                         PaintPropertyTreeBuilderFragmentContext& context) {
  DCHECK(!RuntimeEnabledFeatures::RootLayerScrollingEnabled());
  IntRect container_rect(IntPoint(), frame_view.VisibleContentSize());
  IntRect contents_rect(-frame_view.ScrollOrigin(), frame_view.ContentsSize());
  bool user_scrollable_horizontal =
      frame_view.UserInputScrollable(kHorizontalScrollbar);
  bool user_scrollable_vertical =
      frame_view.UserInputScrollable(kVerticalScrollbar);
  auto ancestor_reasons =
      context.current.scroll->GetMainThreadScrollingReasons();
  auto main_thread_scrolling_reasons =
      GetMainThreadScrollingReasons(frame_view, ancestor_reasons);
  auto element_id = frame_view.GetCompositorElementId();

  if (auto* existing_scroll = frame_view.ScrollNode()) {
    auto existing_reasons = existing_scroll->GetMainThreadScrollingReasons();
    existing_scroll->Update(context.current.scroll, container_rect,
                            contents_rect, user_scrollable_horizontal,
                            user_scrollable_vertical,
                            main_thread_scrolling_reasons, element_id);
    return existing_reasons != main_thread_scrolling_reasons;
  }
  frame_view.SetScrollNode(ScrollPaintPropertyNode::Create(
      context.current.scroll, container_rect, contents_rect,
      user_scrollable_horizontal, user_scrollable_vertical,
      main_thread_scrolling_reasons, element_id));
  return true;
}

// True if a new property was created, false if an existing one was updated.
static bool UpdateScrollTranslation(
    LocalFrameView& frame_view,
    scoped_refptr<const TransformPaintPropertyNode> parent,
    const TransformationMatrix& matrix,
    scoped_refptr<ScrollPaintPropertyNode> scroll) {
  DCHECK(!RuntimeEnabledFeatures::RootLayerScrollingEnabled());
  // TODO(pdr): Set the correct compositing reasons here.
  if (auto* existing_scroll_translation = frame_view.ScrollTranslation()) {
    existing_scroll_translation->Update(
        std::move(parent), matrix, FloatPoint3D(), false, 0,
        CompositingReason::kNone, CompositorElementId(), std::move(scroll));
    return false;
  }
  frame_view.SetScrollTranslation(TransformPaintPropertyNode::Create(
      std::move(parent), matrix, FloatPoint3D(), false, 0,
      CompositingReason::kNone, CompositorElementId(), std::move(scroll)));
  return true;
}

void FrameViewPaintPropertyTreeBuilder::Update(
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
    context.fixed_position.fixed_position_children_fixed_to_root = true;
    return;
  } else {
    context.current.paint_offset_root = frame_view.GetLayoutView();
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
      full_context.force_subtree_update |= UpdateScroll(frame_view, context);
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
    full_context.painting_layer = frame_view.GetLayoutView()->Layer();
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
  context.fixed_position.fixed_position_children_fixed_to_root = true;

  std::unique_ptr<PropertyTreeState> contents_state(new PropertyTreeState(
      context.current.transform, context.current.clip, context.current_effect));
  frame_view.SetTotalPropertyTreeStateForContents(std::move(contents_state));
}

namespace {

class FragmentPaintPropertyTreeBuilder {
 public:
  FragmentPaintPropertyTreeBuilder(
      const LayoutObject& object,
      PaintPropertyTreeBuilderContext& full_context,
      PaintPropertyTreeBuilderFragmentContext& context,
      FragmentData& fragment_data)
      : object_(object),
        full_context_(full_context),
        context_(context),
        fragment_data_(fragment_data),
        properties_(fragment_data.GetRarePaintData()
                        ? fragment_data.GetRarePaintData()->PaintProperties()
                        : nullptr) {}

  ALWAYS_INLINE void UpdateForSelf();
  ALWAYS_INLINE void UpdateForChildren();

 private:
  ALWAYS_INLINE void UpdatePaintOffset();
  ALWAYS_INLINE void UpdateForPaintOffsetTranslation(Optional<IntPoint>&);
  ALWAYS_INLINE void UpdatePaintOffsetTranslation(const Optional<IntPoint>&);
  ALWAYS_INLINE void UpdateForObjectLocationAndSize(
      Optional<IntPoint>& paint_offset_translation);
  ALWAYS_INLINE void UpdateTransform();
  ALWAYS_INLINE void UpdateTransformForNonRootSVG();
  ALWAYS_INLINE void UpdateEffect();
  ALWAYS_INLINE void UpdateFilter();
  ALWAYS_INLINE void UpdateFragmentClip(const PaintLayer&);
  ALWAYS_INLINE void UpdateCssClip();
  ALWAYS_INLINE void UpdateLocalBorderBoxContext();
  ALWAYS_INLINE void UpdateInnerBorderRadiusClip();
  ALWAYS_INLINE void UpdateOverflowClip();
  ALWAYS_INLINE void UpdatePerspective();
  ALWAYS_INLINE void UpdateSvgLocalToBorderBoxTransform();
  ALWAYS_INLINE void UpdateScrollAndScrollTranslation();
  ALWAYS_INLINE void UpdateOutOfFlowContext();

  const LayoutObject& object_;
  // The tree builder context for the whole object.
  PaintPropertyTreeBuilderContext& full_context_;
  // The tree builder context for the current fragment, which is one of the
  // entries in |full_context.fragments|.
  PaintPropertyTreeBuilderFragmentContext& context_;
  FragmentData& fragment_data_;
  ObjectPaintProperties* properties_;
};

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

static bool NeedsSVGLocalToBorderBoxTransform(const LayoutObject& object) {
  return object.IsSVGRoot();
}

static bool NeedsPaintOffsetTranslationForScrollbars(
    const LayoutBoxModelObject& object) {
  if (auto* area = object.GetScrollableArea()) {
    if (area->HorizontalScrollbar() || area->VerticalScrollbar())
      return true;
  }
  return false;
}

static bool NeedsPaintOffsetTranslation(const LayoutObject& object) {
  if (!object.IsBoxModelObject())
    return false;
  const LayoutBoxModelObject& box_model = ToLayoutBoxModelObject(object);

  if (box_model.IsLayoutView()) {
    // Root layer scrolling always creates a translation node for LayoutView to
    // ensure fixed and absolute contexts use the correct transform space.
    // Otherwise we have created all needed property nodes on the FrameView.
    return RuntimeEnabledFeatures::RootLayerScrollingEnabled();
  }

  if (box_model.HasLayer() && box_model.Layer()->PaintsWithTransform(
                                  kGlobalPaintFlattenCompositingLayers)) {
    return true;
  }
  if (NeedsScrollOrScrollTranslation(object))
    return true;
  if (NeedsPaintOffsetTranslationForScrollbars(box_model))
    return true;
  if (NeedsSVGLocalToBorderBoxTransform(object))
    return true;

  // Don't let paint offset cross composited layer boundaries, to avoid
  // unnecessary full layer paint/raster invalidation when paint offset in
  // ancestor transform node changes which should not affect the descendants
  // of the composited layer.
  // TODO(wangxianzhu): For SPv2, we also need a avoid unnecessary paint/raster
  // invalidation in composited layers when their paint offset changes.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      // For only LayoutBlocks that won't be escaped by floating objects and
      // column spans when finding their containing blocks.
      // TODO(crbug.com/780242): This can be avoided if we have fully correct
      // paint property tree states for floating objects and column spans.
      object.IsLayoutBlock() && object.HasLayer() &&
      !ToLayoutBoxModelObject(object).Layer()->EnclosingPaginationLayer() &&
      object.GetCompositingState() == kPaintsIntoOwnBacking)
    return true;

  return false;
}

IntPoint ApplyPaintOffsetTranslation(const LayoutObject& object,
                                     LayoutPoint& paint_offset) {
  // We should use the same subpixel paint offset values for snapping
  // regardless of whether a transform is present. If there is a transform
  // we round the paint offset but keep around the residual fractional
  // component for the transformed content to paint with.  In spv1 this was
  // called "subpixel accumulation". For more information, see
  // PaintLayer::subpixelAccumulation() and
  // PaintLayerPainter::paintFragmentByApplyingTransform.
  IntPoint paint_offset_translation = RoundedIntPoint(paint_offset);
  LayoutPoint fractional_paint_offset =
      LayoutPoint(paint_offset - paint_offset_translation);
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
  paint_offset = fractional_paint_offset;
  return paint_offset_translation;
}

void FragmentPaintPropertyTreeBuilder::UpdateForPaintOffsetTranslation(
    Optional<IntPoint>& paint_offset_translation) {
  if (!NeedsPaintOffsetTranslation(object_))
    return;

  paint_offset_translation =
      ApplyPaintOffsetTranslation(object_, context_.current.paint_offset);
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled() &&
      object_.IsLayoutView()) {
    context_.absolute_position.paint_offset = context_.current.paint_offset;
    context_.fixed_position.paint_offset = context_.current.paint_offset;
  }
}

void FragmentPaintPropertyTreeBuilder::UpdatePaintOffsetTranslation(
    const Optional<IntPoint>& paint_offset_translation) {
  DCHECK(properties_);

  if (paint_offset_translation) {
    auto result = properties_->UpdatePaintOffsetTranslation(
        context_.current.transform,
        TransformationMatrix().Translate(paint_offset_translation->X(),
                                         paint_offset_translation->Y()),
        FloatPoint3D(), context_.current.should_flatten_inherited_transform,
        context_.current.rendering_context_id);
    context_.current.transform = properties_->PaintOffsetTranslation();
    if (RuntimeEnabledFeatures::RootLayerScrollingEnabled() &&
        object_.IsLayoutView()) {
      context_.absolute_position.transform =
          properties_->PaintOffsetTranslation();
      context_.fixed_position.transform = properties_->PaintOffsetTranslation();
    }

    full_context_.force_subtree_update |= result.NewNodeCreated();
  } else {
    full_context_.force_subtree_update |=
        properties_->ClearPaintOffsetTranslation();
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

// SVG does not use the general transform update of |UpdateTransform|, instead
// creating a transform node for SVG-specific transforms without 3D.
void FragmentPaintPropertyTreeBuilder::UpdateTransformForNonRootSVG() {
  DCHECK(properties_);
  DCHECK(object_.IsSVGChild());
  // SVG does not use paint offset internally, except for SVGForeignObject which
  // has different SVG and HTML coordinate spaces.
  DCHECK(object_.IsSVGForeignObject() ||
         context_.current.paint_offset == LayoutPoint());

  if (object_.NeedsPaintPropertyUpdate() ||
      full_context_.force_subtree_update) {
    AffineTransform transform = object_.LocalToSVGParentTransform();
    if (NeedsTransformForNonRootSVG(object_)) {
      // The origin is included in the local transform, so leave origin empty.
      auto result = properties_->UpdateTransform(
          context_.current.transform, TransformationMatrix(transform),
          FloatPoint3D());
      full_context_.force_subtree_update |= result.NewNodeCreated();
    } else {
      full_context_.force_subtree_update |= properties_->ClearTransform();
    }
  }

  if (properties_->Transform()) {
    context_.current.transform = properties_->Transform();
    context_.current.should_flatten_inherited_transform = false;
    context_.current.rendering_context_id = 0;
  }
}

static CompositingReasons CompositingReasonsForTransform(const LayoutBox& box) {
  const ComputedStyle& style = box.StyleRef();
  CompositingReasons compositing_reasons = CompositingReason::kNone;
  if (CompositingReasonFinder::RequiresCompositingForTransform(box))
    compositing_reasons |= CompositingReason::k3DTransform;

  if (CompositingReasonFinder::RequiresCompositingForTransformAnimation(style))
    compositing_reasons |= CompositingReason::kActiveAnimation;

  if (style.HasWillChangeCompositingHint() &&
      !style.SubtreeWillChangeContents())
    compositing_reasons |= CompositingReason::kWillChangeCompositingHint;

  if (box.HasLayer() && box.Layer()->Has3DTransformedDescendant()) {
    if (style.HasPerspective())
      compositing_reasons |= CompositingReason::kPerspectiveWith3DDescendants;
    if (style.UsedTransformStyle3D() == ETransformStyle3D::kPreserve3d)
      compositing_reasons |= CompositingReason::kPreserve3DWith3DDescendants;
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
             CompositingReason::kNone;
}

void FragmentPaintPropertyTreeBuilder::UpdateTransform() {
  if (object_.IsSVGChild()) {
    UpdateTransformForNonRootSVG();
    return;
  }

  DCHECK(properties_);

  if (object_.NeedsPaintPropertyUpdate() ||
      full_context_.force_subtree_update) {
    const ComputedStyle& style = object_.StyleRef();
    // A transform node is allocated for transforms, preserves-3d and any
    // direct compositing reason. The latter is required because this is the
    // only way to represent compositing both an element and its stacking
    // descendants.
    if (NeedsTransform(object_)) {
      auto& box = ToLayoutBox(object_);

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
      unsigned rendering_context_id = context_.current.rendering_context_id;
      if (style.Preserves3D() && !rendering_context_id)
        rendering_context_id = PtrHash<const LayoutObject>::GetHash(&object_);

      auto result = properties_->UpdateTransform(
          context_.current.transform, matrix, TransformOrigin(box),
          context_.current.should_flatten_inherited_transform,
          rendering_context_id, compositing_reasons,
          CompositorElementIdFromUniqueObjectId(
              object_.UniqueId(), CompositorElementIdNamespace::kPrimary));
      full_context_.force_subtree_update |= result.NewNodeCreated();
    } else {
      full_context_.force_subtree_update |= properties_->ClearTransform();
    }
  }

  if (properties_->Transform()) {
    context_.current.transform = properties_->Transform();
    if (object_.StyleRef().Preserves3D()) {
      context_.current.rendering_context_id =
          properties_->Transform()->RenderingContextId();
      context_.current.should_flatten_inherited_transform = false;
    } else {
      context_.current.rendering_context_id = 0;
      context_.current.should_flatten_inherited_transform = true;
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
  if (style.HasMaskBoxImageOutsets())
    maximum_mask_region.Expand(style.MaskBoxImageOutsets());
  maximum_mask_region.MoveBy(paint_offset);
  mask_clip = PixelSnappedIntRect(maximum_mask_region);
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

void FragmentPaintPropertyTreeBuilder::UpdateEffect() {
  DCHECK(properties_);
  const ComputedStyle& style = object_.StyleRef();

  // TODO(trchen): Can't omit effect node if we have 3D children.
  if (object_.NeedsPaintPropertyUpdate() ||
      full_context_.force_subtree_update) {
    const ClipPaintPropertyNode* output_clip = nullptr;
    bool local_clip_added_or_removed = false;
    bool local_clip_changed = false;
    if (NeedsEffect(object_)) {
      // We may begin to composite our subtree prior to an animation starts,
      // but a compositor element ID is only needed when an animation is
      // current.
      CompositingReasons compositing_reasons = CompositingReason::kNone;
      if (CompositingReasonFinder::RequiresCompositingForOpacityAnimation(
              style)) {
        compositing_reasons = CompositingReason::kActiveAnimation;
      }

      IntRect mask_clip;
      ColorFilter mask_color_filter;
      bool has_mask = ComputeMaskParameters(
          mask_clip, mask_color_filter, object_, context_.current.paint_offset);
      if (has_mask &&
          // TODO(crbug.com/768691): Remove the following condition after mask
          // clip doesn't fail fast/borders/inline-mask-overlay-image-outset-
          // vertical-rl.html.
          RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
        FloatRoundedRect rounded_mask_clip(mask_clip);
        if (properties_->MaskClip() &&
            rounded_mask_clip != properties_->MaskClip()->ClipRect())
          local_clip_changed = true;
        auto result = properties_->UpdateMaskClip(context_.current.clip,
                                                  context_.current.transform,
                                                  FloatRoundedRect(mask_clip));
        local_clip_added_or_removed |= result.NewNodeCreated();
        output_clip = properties_->MaskClip();
      } else {
        full_context_.force_subtree_update |= properties_->ClearMaskClip();
      }

      SkBlendMode blend_mode =
          object_.IsBlendingAllowed()
              ? WebCoreCompositeToSkiaComposite(kCompositeSourceOver,
                                                style.BlendMode())
              : SkBlendMode::kSrcOver;

      auto result = properties_->UpdateEffect(
          context_.current_effect, context_.current.transform, output_clip,
          kColorFilterNone, CompositorFilterOperations(), style.Opacity(),
          blend_mode, compositing_reasons,
          CompositorElementIdFromUniqueObjectId(
              object_.UniqueId(), CompositorElementIdNamespace::kPrimary));
      full_context_.force_subtree_update |= result.NewNodeCreated();
      if (has_mask) {
        auto result = properties_->UpdateMask(
            properties_->Effect(), context_.current.transform, output_clip,
            mask_color_filter, CompositorFilterOperations(), 1.f,
            SkBlendMode::kDstIn, CompositingReason::kNone,
            CompositorElementIdFromUniqueObjectId(
                object_.UniqueId(), CompositorElementIdNamespace::kEffectMask));
        full_context_.force_subtree_update |= result.NewNodeCreated();
      } else {
        full_context_.force_subtree_update |= properties_->ClearMask();
      }
    } else {
      full_context_.force_subtree_update |= properties_->ClearEffect();
      full_context_.force_subtree_update |= properties_->ClearMask();
      local_clip_added_or_removed |= properties_->ClearMaskClip();
    }
    full_context_.force_subtree_update |= local_clip_added_or_removed;
    full_context_.clip_changed |=
        local_clip_changed || local_clip_added_or_removed;
  }

  if (properties_->Effect()) {
    context_.current_effect = properties_->Effect();
    if (properties_->MaskClip()) {
      context_.current.clip = context_.absolute_position.clip =
          context_.fixed_position.clip = properties_->MaskClip();
    }
  }
}

static bool NeedsFilter(const LayoutObject& object) {
  // TODO(trchen): SVG caches filters in SVGResources. Implement it.
  return object.IsBoxModelObject() && ToLayoutBoxModelObject(object).Layer() &&
         (object.StyleRef().HasFilter() || object.HasReflection());
}

void FragmentPaintPropertyTreeBuilder::UpdateFilter() {
  DCHECK(properties_);
  const ComputedStyle& style = object_.StyleRef();

  if (object_.NeedsPaintPropertyUpdate() ||
      full_context_.force_subtree_update) {
    if (NeedsFilter(object_)) {
      CompositorFilterOperations filter;
      // Try to use the cached filter.
      if (properties_->Filter())
        filter = properties_->Filter()->Filter();
      auto& layer = *ToLayoutBoxModelObject(object_).Layer();
      layer.UpdateCompositorFilterOperationsForFilter(filter);
      layer.ClearFilterOnEffectNodeDirty();

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
      const ClipPaintPropertyNode* output_clip = context_.current.clip;

      // TODO(trchen): A filter may contain spatial operations such that an
      // output pixel may depend on an input pixel outside of the output clip.
      // We should generate a special clip node to represent this expansion.

      // We may begin to composite our subtree prior to an animation starts,
      // but a compositor element ID is only needed when an animation is
      // current.
      CompositingReasons compositing_reasons =
          CompositingReasonFinder::RequiresCompositingForFilterAnimation(style)
              ? CompositingReason::kActiveAnimation
              : CompositingReason::kNone;
      DCHECK(!style.HasCurrentFilterAnimation() ||
             compositing_reasons != CompositingReason::kNone);

      auto result = properties_->UpdateFilter(
          context_.current_effect, context_.current.transform, output_clip,
          kColorFilterNone, std::move(filter), 1.f, SkBlendMode::kSrcOver,
          compositing_reasons,
          CompositorElementIdFromUniqueObjectId(
              object_.UniqueId(), CompositorElementIdNamespace::kEffectFilter),
          FloatPoint(context_.current.paint_offset));
      full_context_.force_subtree_update |= result.NewNodeCreated();
    } else {
      full_context_.force_subtree_update |= properties_->ClearFilter();
    }
  }

  if (properties_->Filter()) {
    context_.current_effect = properties_->Filter();
    // TODO(trchen): Change input clip to expansion hint once implemented.
    const ClipPaintPropertyNode* input_clip =
        properties_->Filter()->OutputClip();
    context_.current.clip = context_.absolute_position.clip =
        context_.fixed_position.clip = input_clip;
  }
}

static bool NeedsFragmentation(const LayoutObject& object,
                               const PaintLayer& painting_layer) {
  return painting_layer.ShouldFragmentCompositedBounds();
}

static bool NeedsFragmentationClip(const LayoutObject& object,
                                   const PaintLayer& painting_layer) {
  return object.HasLayer() && NeedsFragmentation(object, painting_layer);
}

void FragmentPaintPropertyTreeBuilder::UpdateFragmentClip(
    const PaintLayer& painting_layer) {
  DCHECK(properties_);

  if (object_.NeedsPaintPropertyUpdate() ||
      full_context_.force_subtree_update) {
    bool local_clip_added_or_removed = false;
    bool local_clip_changed = false;
    // It's possible to still have no clips even if NeedsFragmentationClip is
    // true, in the case when the FragmentainerIterator returns none.
    if (NeedsFragmentationClip(object_, painting_layer) &&
        context_.fragment_clip) {
      LayoutRect clip_rect(*context_.fragment_clip);

      FloatRoundedRect rounded_clip_rect((FloatRect(clip_rect)));

      if (properties_->FragmentClip() &&
          properties_->FragmentClip()->ClipRect() != rounded_clip_rect)
        local_clip_changed = true;

      auto result = properties_->UpdateFragmentClip(
          context_.current.clip, context_.current.transform, rounded_clip_rect);
      local_clip_added_or_removed |= result.NewNodeCreated();
    } else {
      local_clip_added_or_removed |= properties_->ClearFragmentClip();
    }
    full_context_.force_subtree_update |= local_clip_added_or_removed;
    full_context_.clip_changed |=
        local_clip_changed || local_clip_added_or_removed;
  }
}

static bool NeedsCssClip(const LayoutObject& object) {
  return object.HasClip();
}

void FragmentPaintPropertyTreeBuilder::UpdateCssClip() {
  DCHECK(properties_);

  if (object_.NeedsPaintPropertyUpdate() ||
      full_context_.force_subtree_update) {
    bool local_clip_added_or_removed = false;
    bool local_clip_changed = false;
    if (NeedsCssClip(object_)) {
      // Create clip node for descendants that are not fixed position.
      // We don't have to setup context.absolutePosition.clip here because this
      // object must be a container for absolute position descendants, and will
      // copy from in-flow context later at updateOutOfFlowContext() step.
      DCHECK(object_.CanContainAbsolutePositionObjects());
      LayoutRect clip_rect =
          ToLayoutBox(object_).ClipRect(context_.current.paint_offset);

      FloatRoundedRect rounded_clip_rect((FloatRect(clip_rect)));
      if (properties_->CssClip() &&
          properties_->CssClip()->ClipRect() != rounded_clip_rect)
        local_clip_changed = true;

      auto result = properties_->UpdateCssClip(
          context_.current.clip, context_.current.transform,
          FloatRoundedRect(FloatRect(clip_rect)));
      local_clip_added_or_removed |= result.NewNodeCreated();
    } else {
      local_clip_added_or_removed |= properties_->ClearCssClip();
    }
    full_context_.force_subtree_update |= local_clip_added_or_removed;
    full_context_.clip_changed |=
        local_clip_changed || local_clip_added_or_removed;
  }

  if (properties_->CssClip())
    context_.current.clip = properties_->CssClip();
}

void FragmentPaintPropertyTreeBuilder::UpdateLocalBorderBoxContext() {
  if (!object_.NeedsPaintPropertyUpdate() &&
      !full_context_.force_subtree_update)
    return;

  if (!object_.HasLayer() && !NeedsPaintOffsetTranslation(object_)) {
    if (auto* rare_paint_data = fragment_data_.GetRarePaintData())
      rare_paint_data->ClearLocalBorderBoxProperties();
  } else {
    auto& rare_paint_data = fragment_data_.EnsureRarePaintData();
    const ClipPaintPropertyNode* clip = context_.current.clip;
    if (rare_paint_data.PaintProperties() &&
        rare_paint_data.PaintProperties()->FragmentClip()) {
      clip = rare_paint_data.PaintProperties()->FragmentClip();
    }

    PropertyTreeState local_border_box = PropertyTreeState(
        context_.current.transform, clip, context_.current_effect);

    rare_paint_data.SetLocalBorderBoxProperties(local_border_box);
  }
}

static bool NeedsOverflowClip(const LayoutObject& object) {
  return object.IsBox() && ToLayoutBox(object).ShouldClipOverflow();
}

static bool NeedsInnerBorderRadiusClip(const LayoutObject& object) {
  return object.StyleRef().HasBorderRadius() &&
         // IsLayoutEmbeddedContent() is for iframes with border-radius.
         (object.IsLayoutEmbeddedContent() || NeedsOverflowClip(object));
}

static bool NeedsControlClipFragmentationAdjustment(const LayoutBox& box) {
  return box.HasControlClip() && !box.Layer() &&
         box.PaintingLayer()->EnclosingPaginationLayer();
}

static LayoutPoint VisualOffsetFromPaintOffsetRoot(
    const PaintPropertyTreeBuilderFragmentContext& context,
    const PaintLayer* child) {
  const LayoutObject* paint_offset_root = context.current.paint_offset_root;
  PaintLayer* painting_layer = paint_offset_root->PaintingLayer();
  LayoutPoint result = child->VisualOffsetFromAncestor(painting_layer);
  if (!paint_offset_root->HasLayer()) {
    result.Move(-paint_offset_root->OffsetFromAncestorContainer(
        &painting_layer->GetLayoutObject()));
  }

  // Don't include scroll offset of paint_offset_root. Any scroll is
  // already included in a separate transform node.
  if (paint_offset_root->HasOverflowClip())
    result += ToLayoutBox(paint_offset_root)->ScrolledContentOffset();
  return result;
}

void FragmentPaintPropertyTreeBuilder::UpdateInnerBorderRadiusClip() {
  DCHECK(properties_);

  if (object_.NeedsPaintPropertyUpdate() ||
      full_context_.force_subtree_update) {
    bool clip_added_or_removed;
    if (NeedsInnerBorderRadiusClip(object_)) {
      const LayoutBox& box = ToLayoutBox(object_);
      auto inner_border = box.StyleRef().GetRoundedInnerBorderFor(
          LayoutRect(context_.current.paint_offset, box.Size()));
      auto result = properties_->UpdateInnerBorderRadiusClip(
          context_.current.clip, context_.current.transform, inner_border);

      if (!full_context_.clip_changed && properties_->InnerBorderRadiusClip() &&
          inner_border != properties_->InnerBorderRadiusClip()->ClipRect())
        full_context_.clip_changed = true;
      clip_added_or_removed = result.NewNodeCreated();
    } else {
      clip_added_or_removed = properties_->ClearInnerBorderRadiusClip();
    }

    full_context_.force_subtree_update |= clip_added_or_removed;
    full_context_.clip_changed |= clip_added_or_removed;
  }

  if (auto* border_radius_clip = properties_->InnerBorderRadiusClip())
    context_.current.clip = border_radius_clip;
}

void FragmentPaintPropertyTreeBuilder::UpdateOverflowClip() {
  DCHECK(properties_);

  if (object_.NeedsPaintPropertyUpdate() ||
      full_context_.force_subtree_update) {
    bool clip_added_or_removed;
    if (NeedsOverflowClip(object_)) {
      const LayoutBox& box = ToLayoutBox(object_);
      LayoutRect clip_rect;
      clip_rect = box.OverflowClipRect(context_.current.paint_offset);
      FloatRoundedRect clipping_rect((FloatRect(clip_rect)));
      if (!full_context_.clip_changed && properties_->OverflowClip() &&
          clipping_rect != properties_->OverflowClip()->ClipRect())
        full_context_.clip_changed = true;

      auto result = properties_->UpdateOverflowClip(
          context_.current.clip, context_.current.transform,
          FloatRoundedRect(FloatRect(clip_rect)));
      clip_added_or_removed = result.NewNodeCreated();
    } else {
      clip_added_or_removed = properties_->ClearOverflowClip();
    }

    full_context_.force_subtree_update |= clip_added_or_removed;
    full_context_.clip_changed |= clip_added_or_removed;
  }

  if (auto* overflow_clip = properties_->OverflowClip())
    context_.current.clip = overflow_clip;
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

void FragmentPaintPropertyTreeBuilder::UpdatePerspective() {
  DCHECK(properties_);

  if (object_.NeedsPaintPropertyUpdate() ||
      full_context_.force_subtree_update) {
    if (NeedsPerspective(object_)) {
      const ComputedStyle& style = object_.StyleRef();
      // The perspective node must not flatten (else nothing will get
      // perspective), but it should still extend the rendering context as
      // most transform nodes do.
      TransformationMatrix matrix =
          TransformationMatrix().ApplyPerspective(style.Perspective());
      FloatPoint3D origin = PerspectiveOrigin(ToLayoutBox(object_)) +
                            ToLayoutSize(context_.current.paint_offset);
      auto result = properties_->UpdatePerspective(
          context_.current.transform, matrix, origin,
          context_.current.should_flatten_inherited_transform,
          context_.current.rendering_context_id);
      full_context_.force_subtree_update |= result.NewNodeCreated();
    } else {
      full_context_.force_subtree_update |= properties_->ClearPerspective();
    }
  }

  if (properties_->Perspective()) {
    context_.current.transform = properties_->Perspective();
    context_.current.should_flatten_inherited_transform = false;
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateSvgLocalToBorderBoxTransform() {
  DCHECK(properties_);
  if (!object_.IsSVGRoot())
    return;

  if (object_.NeedsPaintPropertyUpdate() ||
      full_context_.force_subtree_update) {
    AffineTransform transform_to_border_box =
        SVGRootPainter(ToLayoutSVGRoot(object_))
            .TransformToPixelSnappedBorderBox(context_.current.paint_offset);
    if (!transform_to_border_box.IsIdentity() &&
        NeedsSVGLocalToBorderBoxTransform(object_)) {
      auto result = properties_->UpdateSvgLocalToBorderBoxTransform(
          context_.current.transform, transform_to_border_box, FloatPoint3D());
      full_context_.force_subtree_update |= result.NewNodeCreated();
    } else {
      full_context_.force_subtree_update |=
          properties_->ClearSvgLocalToBorderBoxTransform();
    }
  }

  if (properties_->SvgLocalToBorderBoxTransform()) {
    context_.current.transform = properties_->SvgLocalToBorderBoxTransform();
    context_.current.should_flatten_inherited_transform = false;
    context_.current.rendering_context_id = 0;
  }
  // The paint offset is included in |transformToBorderBox| so SVG does not need
  // to handle paint offset internally.
  context_.current.paint_offset = LayoutPoint();
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

void FragmentPaintPropertyTreeBuilder::UpdateScrollAndScrollTranslation() {
  DCHECK(properties_);

  if (object_.NeedsPaintPropertyUpdate() ||
      full_context_.force_subtree_update) {
    if (NeedsScrollNode(object_)) {
      const LayoutBox& box = ToLayoutBox(object_);
      auto* scrollable_area = box.GetScrollableArea();

      // The container bounds are snapped to integers to match the equivalent
      // bounds on cc::ScrollNode. The offset is snapped to match the current
      // integer offsets used in CompositedLayerMapping.
      auto container_rect = PixelSnappedIntRect(
          box.OverflowClipRect(context_.current.paint_offset));

      IntRect contents_rect(-scrollable_area->ScrollOrigin(),
                            scrollable_area->ContentsSize());
      contents_rect.MoveBy(container_rect.Location());
      // In flipped blocks writing mode, if there is scrollbar on the right,
      // we move the contents to the left with extra amount of ScrollTranslation
      // (-VerticalScrollbarWidth, 0). As contents_rect is in the space of
      // ScrollTranslation, we need to compensate the extra ScrollTranslation
      // to get correct contents_rect origin.
      if (box.HasFlippedBlocksWritingMode())
        contents_rect.Move(box.VerticalScrollbarWidth(), 0);

      bool user_scrollable_horizontal =
          scrollable_area->UserInputScrollable(kHorizontalScrollbar);
      bool user_scrollable_vertical =
          scrollable_area->UserInputScrollable(kVerticalScrollbar);

      auto ancestor_reasons =
          context_.current.scroll->GetMainThreadScrollingReasons();
      auto reasons = GetMainThreadScrollingReasons(object_, ancestor_reasons);

      // Main thread scrolling reasons depend on their ancestor's reasons
      // so ensure the entire subtree is updated when reasons change.
      if (auto* existing_scroll = properties_->Scroll()) {
        if (existing_scroll->GetMainThreadScrollingReasons() != reasons)
          full_context_.force_subtree_update = true;
      }

      auto element_id = scrollable_area->GetCompositorElementId();

      // TODO(pdr): Set the correct compositing reasons here.
      auto result = properties_->UpdateScroll(
          context_.current.scroll, container_rect, contents_rect,
          user_scrollable_horizontal, user_scrollable_vertical, reasons,
          element_id);
      full_context_.force_subtree_update |= result.NewNodeCreated();
    } else {
      // Ensure pre-existing properties are cleared.
      full_context_.force_subtree_update |= properties_->ClearScroll();
    }

    // A scroll translation node is created for static offset (e.g., overflow
    // hidden with scroll offset) or cases that scroll and have a scroll node.
    if (NeedsScrollOrScrollTranslation(object_)) {
      const LayoutBox& box = ToLayoutBox(object_);
      IntSize scroll_offset = box.ScrolledContentOffset();
      TransformationMatrix scroll_offset_matrix =
          TransformationMatrix().Translate(-scroll_offset.Width(),
                                           -scroll_offset.Height());
      auto result = properties_->UpdateScrollTranslation(
          context_.current.transform, scroll_offset_matrix, FloatPoint3D(),
          context_.current.should_flatten_inherited_transform,
          context_.current.rendering_context_id, CompositingReason::kNone,
          CompositorElementId(), properties_->Scroll());
      full_context_.force_subtree_update |= result.NewNodeCreated();
    } else {
      // Ensure pre-existing properties are cleared.
      full_context_.force_subtree_update |=
          properties_->ClearScrollTranslation();
    }
  }

  if (properties_->Scroll())
    context_.current.scroll = properties_->Scroll();
  if (properties_->ScrollTranslation()) {
    context_.current.transform = properties_->ScrollTranslation();
    context_.current.should_flatten_inherited_transform = false;
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateOutOfFlowContext() {
  if (!object_.IsBoxModelObject() && !properties_)
    return;

  if (object_.IsLayoutBlock())
    context_.paint_offset_for_float = context_.current.paint_offset;

  if (object_.CanContainAbsolutePositionObjects())
    context_.absolute_position = context_.current;

  if (object_.IsLayoutView()) {
    if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      const auto* initial_fixed_transform = context_.fixed_position.transform;
      const auto* initial_fixed_scroll = context_.fixed_position.scroll;

      context_.fixed_position = context_.current;

      // Fixed position transform and scroll nodes should not be affected.
      context_.fixed_position.transform = initial_fixed_transform;
      context_.fixed_position.scroll = initial_fixed_scroll;
    }
  } else if (object_.CanContainFixedPositionObjects()) {
    context_.fixed_position = context_.current;
    context_.fixed_position.fixed_position_children_fixed_to_root = false;
  } else if (properties_ && properties_->CssClip()) {
    // CSS clip applies to all descendants, even if this object is not a
    // containing block ancestor of the descendant. It is okay for
    // absolute-position descendants because having CSS clip implies being
    // absolute position container. However for fixed-position descendants we
    // need to insert the clip here if we are not a containing block ancestor of
    // them.
    auto* css_clip = properties_->CssClip();

    // Before we actually create anything, check whether in-flow context and
    // fixed-position context has exactly the same clip. Reuse if possible.
    if (context_.fixed_position.clip == css_clip->Parent()) {
      context_.fixed_position.clip = css_clip;
    } else {
      if (object_.NeedsPaintPropertyUpdate() ||
          full_context_.force_subtree_update) {
        auto result = properties_->UpdateCssClipFixedPosition(
            context_.fixed_position.clip,
            const_cast<TransformPaintPropertyNode*>(
                css_clip->LocalTransformSpace()),
            css_clip->ClipRect());
        full_context_.force_subtree_update |= result.NewNodeCreated();
      }
      if (properties_->CssClipFixedPosition())
        context_.fixed_position.clip = properties_->CssClipFixedPosition();
      return;
    }
  }

  if (object_.NeedsPaintPropertyUpdate() ||
      full_context_.force_subtree_update) {
    if (properties_) {
      full_context_.force_subtree_update |=
          properties_->ClearCssClipFixedPosition();
    }
  }
}

static LayoutRect BoundingBoxInPaginationContainer(
    const LayoutObject& object,
    const PaintLayer& enclosing_pagination_layer) {
  // Non-boxes that have no layer paint in the space of their containing block.
  if (!object.IsBox() && !object.HasLayer()) {
    return BoundingBoxInPaginationContainer(*object.ContainingBlock(),
                                            enclosing_pagination_layer);
  }

  LayoutRect object_bounding_box_in_flow_thread;
  if (object.HasLayer()) {
    PaintLayer* paint_layer = ToLayoutBoxModelObject(object).Layer();
    object_bounding_box_in_flow_thread =
        paint_layer->PhysicalBoundingBox(&enclosing_pagination_layer);
  } else {
    // Compute the bounding box without transforms.
    // The object is guaranteed to be a box due to the logic above.
    DCHECK(object.IsBox());
    object_bounding_box_in_flow_thread = ToLayoutBox(object).BorderBoxRect();
    TransformState transform_state(
        TransformState::kApplyTransformDirection,
        FloatPoint(object_bounding_box_in_flow_thread.Location()));
    object.MapLocalToAncestor(&enclosing_pagination_layer.GetLayoutObject(),
                              transform_state, kApplyContainerFlip);
    transform_state.Flatten();
    object_bounding_box_in_flow_thread.SetLocation(
        LayoutPoint(transform_state.LastPlanarPoint()));
  }

  return object_bounding_box_in_flow_thread;
}

void FragmentPaintPropertyTreeBuilder::UpdatePaintOffset() {
  // Paint offsets for fragmented content are computed from scratch.
  if (context_.fragment_clip &&
      // Except if the paint_offset_root is below the pagination container,
      // in which case fragmentation offsets are already baked into the paint
      // offset transform for paint_offset_root.
      !context_.current.paint_offset_root->PaintingLayer()
           ->EnclosingPaginationLayer()) {
    PaintLayer* paint_layer = object_.PaintingLayer();
    PaintLayer* enclosing_pagination_layer =
        paint_layer->EnclosingPaginationLayer();
    DCHECK(enclosing_pagination_layer);

    // Set fragment visual paint offset.
    LayoutPoint paint_offset =
        BoundingBoxInPaginationContainer(object_, *enclosing_pagination_layer)
            .Location();

    paint_offset.MoveBy(fragment_data_.GetRarePaintData()->PaginationOffset());
    paint_offset.MoveBy(
        VisualOffsetFromPaintOffsetRoot(context_, enclosing_pagination_layer));

    // The paint offset root can have a subpixel paint offset adjustment.
    // The paint offset root always has one fragment.
    paint_offset.MoveBy(
        context_.current.paint_offset_root->FirstFragment().PaintOffset());

    context_.current.paint_offset = paint_offset;

    return;
  }

  if (object_.IsFloating())
    context_.current.paint_offset = context_.paint_offset_for_float;

  // Multicolumn spanners are painted starting at the multicolumn container (but
  // still inherit properties in layout-tree order) so reset the paint offset.
  if (object_.IsColumnSpanAll()) {
    context_.current.paint_offset =
        object_.Container()->FirstFragment().PaintOffset();
  }

  if (object_.IsBoxModelObject()) {
    const LayoutBoxModelObject& box_model_object =
        ToLayoutBoxModelObject(object_);
    switch (box_model_object.StyleRef().GetPosition()) {
      case EPosition::kStatic:
        break;
      case EPosition::kRelative:
        context_.current.paint_offset +=
            box_model_object.OffsetForInFlowPosition();
        break;
      case EPosition::kAbsolute: {
        DCHECK(full_context_.container_for_absolute_position ==
               box_model_object.Container());
        context_.current = context_.absolute_position;

        // Absolutely positioned content in an inline should be positioned
        // relative to the inline.
        const auto* container = full_context_.container_for_absolute_position;
        if (container && container->IsInFlowPositioned() &&
            container->IsLayoutInline()) {
          DCHECK(box_model_object.IsBox());
          context_.current.paint_offset +=
              ToLayoutInline(container)->OffsetForInFlowPositionedInline(
                  ToLayoutBox(box_model_object));
        }
        break;
      }
      case EPosition::kSticky:
        context_.current.paint_offset +=
            box_model_object.OffsetForInFlowPosition();
        break;
      case EPosition::kFixed:
        context_.current = context_.fixed_position;
        // Fixed-position elements that are fixed to the vieport have a
        // transform above the scroll of the LayoutView. Child content is
        // relative to that transform, and hence the fixed-position element.
        if (context_.fixed_position.fixed_position_children_fixed_to_root)
          context_.current.paint_offset_root = &box_model_object;
        break;
      default:
        NOTREACHED();
    }
  }

  if (object_.IsBox()) {
    // TODO(pdr): Several calls in this function walk back up the tree to
    // calculate containers (e.g., physicalLocation, offsetForInFlowPosition*).
    // The containing block and other containers can be stored on
    // PaintPropertyTreeBuilderFragmentContext instead of recomputing them.
    context_.current.paint_offset.MoveBy(
        ToLayoutBox(object_).PhysicalLocation());
    // This is a weird quirk that table cells paint as children of table rows,
    // but their location have the row's location baked-in.
    // Similar adjustment is done in LayoutTableCell::offsetFromContainer().
    if (object_.IsTableCell()) {
      LayoutObject* parent_row = object_.Parent();
      DCHECK(parent_row && parent_row->IsTableRow());
      context_.current.paint_offset.MoveBy(
          -ToLayoutBox(parent_row)->PhysicalLocation());
    }
  }
}

static void SetNeedsPaintPropertyUpdateIfNeeded(const LayoutObject& object) {
  if (!object.IsBoxModelObject())
    return;

  const LayoutBoxModelObject& box_model_object = ToLayoutBoxModelObject(object);
  if (box_model_object.Layer() &&
      box_model_object.Layer()->ShouldFragmentCompositedBounds()) {
    // Always force-update properties for fragmented content.
    // TODO(chrishtr): find ways to optimize this in the future.
    // It may suffice to compare previous and current visual overflow,
    // but we do not currenly cache that on the LayoutObject or PaintLayer.
    object.GetMutableForPainting().SetNeedsPaintPropertyUpdate();
    return;
  }

  if (!object.IsBox())
    return;

  const LayoutBox& box = ToLayoutBox(object);

  // Always force-update properties for fragmented content. Boxes with
  // control clip have a fragment-aware offset.
  if (NeedsControlClipFragmentationAdjustment(box)) {
    box.GetMutableForPainting().SetNeedsPaintPropertyUpdate();
    return;
  }

  if (box.Size() == box.PreviousSize())
    return;

  // CSS mask and clip-path comes with an implicit clip to the border box.
  // Currently only SPv2 generate and take advantage of those.
  const bool box_generates_property_nodes_for_mask_and_clip_path =
      RuntimeEnabledFeatures::SlimmingPaintV175Enabled() &&
      (box.HasMask() || box.HasClipPath());
  // The overflow clip paint property depends on the border box rect through
  // overflowClipRect(). The border box rect's size equals the frame rect's
  // size so we trigger a paint property update when the frame rect changes.
  if (NeedsOverflowClip(box) || NeedsInnerBorderRadiusClip(box) ||
      // The used value of CSS clip may depend on size of the box, e.g. for
      // clip: rect(auto auto auto -5px).
      NeedsCssClip(box) ||
      // Relative lengths (e.g., percentage values) in transform, perspective,
      // transform-origin, and perspective-origin can depend on the size of the
      // frame rect, so force a property update if it changes. TODO(pdr): We
      // only need to update properties if there are relative lengths.
      box.StyleRef().HasTransform() || NeedsPerspective(box) ||
      box_generates_property_nodes_for_mask_and_clip_path)
    box.GetMutableForPainting().SetNeedsPaintPropertyUpdate();

  // The filter generated for reflection depends on box size.
  if (box.HasReflection()) {
    DCHECK(box.HasLayer());
    box.Layer()->SetFilterOnEffectNodeDirty();
    box.GetMutableForPainting().SetNeedsPaintPropertyUpdate();
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateForObjectLocationAndSize(
    Optional<IntPoint>& paint_offset_translation) {
#if DCHECK_IS_ON()
  FindPaintOffsetNeedingUpdateScope check_scope(
      object_, fragment_data_, full_context_.is_actually_needed);
#endif

  UpdatePaintOffset();
  UpdateForPaintOffsetTranslation(paint_offset_translation);

  if (fragment_data_.PaintOffset() != context_.current.paint_offset) {
    // Many paint properties depend on paint offset so we force an update of
    // the entire subtree on paint offset changes.
    full_context_.force_subtree_update = true;

    if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
      object_.GetMutableForPainting().SetShouldDoFullPaintInvalidation(
          PaintInvalidationReason::kGeometry);
    }
    fragment_data_.SetPaintOffset(context_.current.paint_offset);
  }

  if (paint_offset_translation)
    context_.current.paint_offset_root = &ToLayoutBoxModelObject(object_);

  SetNeedsPaintPropertyUpdateIfNeeded(object_);
}

void FragmentPaintPropertyTreeBuilder::UpdateForSelf() {
  // This is not in FindObjectPropertiesNeedingUpdateScope because paint offset
  // can change without NeedsPaintPropertyUpdate.
  Optional<IntPoint> paint_offset_translation;
  UpdateForObjectLocationAndSize(paint_offset_translation);

  if (properties_) {
    // TODO(wangxianzhu): Put these in FindObjectPropertiesNeedingUpdateScope.
    UpdateFragmentClip(*full_context_.painting_layer);
    UpdatePaintOffsetTranslation(paint_offset_translation);
  }

#if DCHECK_IS_ON()
  FindObjectPropertiesNeedingUpdateScope check_needs_update_scope(
      object_, fragment_data_, full_context_.force_subtree_update);
#endif

  if (properties_) {
    UpdateTransform();
    UpdateCssClip();
    if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
      UpdateEffect();
    UpdateFilter();
  }
  UpdateLocalBorderBoxContext();
}

void FragmentPaintPropertyTreeBuilder::UpdateForChildren() {
#if DCHECK_IS_ON()
  FindObjectPropertiesNeedingUpdateScope check_needs_update_scope(
      object_, fragment_data_, full_context_.force_subtree_update);
#endif

  if (properties_) {
    UpdateInnerBorderRadiusClip();
    UpdateOverflowClip();
    UpdatePerspective();
    UpdateSvgLocalToBorderBoxTransform();
    UpdateScrollAndScrollTranslation();
  }
  UpdateOutOfFlowContext();
}

}  // namespace

// Match |fragment_clip| against an intersecting one from the parent contexts,
// if any, to allow for correct transform and effect parenting of fragments.
PaintPropertyTreeBuilderFragmentContext ContextForFragment(
    const LayoutRect& fragment_clip,
    const Vector<PaintPropertyTreeBuilderFragmentContext, 1>&
        parent_fragments) {
  // Find a fragment whose clip intersects this one, if any.
  for (auto& fragment_context : parent_fragments) {
    if (!fragment_context.fragment_clip ||
        fragment_clip.Intersects(*fragment_context.fragment_clip)) {
      PaintPropertyTreeBuilderFragmentContext context(fragment_context);
      context.fragment_clip = fragment_clip;
      return context;
    }
  }

  // Otherwise return a new fragment parented at the first parent fragment.
  if (parent_fragments.IsEmpty()) {
    PaintPropertyTreeBuilderFragmentContext context;
    context.fragment_clip = LayoutRect();
    return context;
  }
  PaintPropertyTreeBuilderFragmentContext context(parent_fragments[0]);
  context.fragment_clip = fragment_clip;
  return context;
}

void ObjectPaintPropertyTreeBuilder::InitFragmentPaintProperties(
    FragmentData& fragment,
    bool needs_paint_properties) {
  if (needs_paint_properties) {
    fragment.EnsureRarePaintData().EnsurePaintProperties();
  } else if (fragment.PaintProperties()) {
    context_.force_subtree_update = true;
    fragment.GetRarePaintData()->ClearPaintProperties();
  }
}

void ObjectPaintPropertyTreeBuilder::InitSingleFragmentFromParent(
    bool needs_paint_properties) {
  FragmentData& first_fragment =
      object_.GetMutableForPainting().FirstFragment();
  first_fragment.ClearNextFragment();
  InitFragmentPaintProperties(first_fragment, needs_paint_properties);
  if (context_.fragments.IsEmpty()) {
    context_.fragments.push_back(PaintPropertyTreeBuilderFragmentContext());
  } else {
    context_.fragments.resize(1);
    context_.fragments[0].fragment_clip.reset();
  }
}

// Limit the maximum number of fragments, to avoid pathological situations.
static const int kMaxNumFragments = 2000;

void ObjectPaintPropertyTreeBuilder::UpdateFragments() {
  bool needs_paint_properties =
      NeedsPaintOffsetTranslation(object_) || NeedsTransform(object_) ||
      NeedsEffect(object_) || NeedsTransformForNonRootSVG(object_) ||
      NeedsFilter(object_) || NeedsCssClip(object_) ||
      NeedsInnerBorderRadiusClip(object_) || NeedsOverflowClip(object_) ||
      NeedsPerspective(object_) || NeedsSVGLocalToBorderBoxTransform(object_) ||
      NeedsScrollOrScrollTranslation(object_) ||
      NeedsFragmentationClip(object_, *context_.painting_layer);

  if (!NeedsFragmentation(object_, *context_.painting_layer)) {
    InitSingleFragmentFromParent(needs_paint_properties);
  } else {
    // We need at least the fragments for all fragmented objects, which store
    // their local border box properties and paint invalidation data (such
    // as paint offset and visual rect) on each fragment.
    PaintLayer* paint_layer = object_.PaintingLayer();
    PaintLayer* enclosing_pagination_layer =
        paint_layer->EnclosingPaginationLayer();

    LayoutRect object_bounding_box_in_flow_thread =
        BoundingBoxInPaginationContainer(object_, *enclosing_pagination_layer);
    const LayoutFlowThread& flow_thread =
        ToLayoutFlowThread(enclosing_pagination_layer->GetLayoutObject());
    FragmentainerIterator iterator(flow_thread,
                                   object_bounding_box_in_flow_thread);

    Vector<PaintPropertyTreeBuilderFragmentContext> new_fragment_contexts;
    FragmentData* current_fragment_data = nullptr;

    int fragment_count = 0;
    for (; !iterator.AtEnd() && fragment_count < kMaxNumFragments;
         iterator.Advance(), fragment_count++) {
      if (!current_fragment_data) {
        current_fragment_data =
            &object_.GetMutableForPainting().FirstFragment();
      } else {
        current_fragment_data = &current_fragment_data->EnsureNextFragment();
      }

      InitFragmentPaintProperties(*current_fragment_data,
                                  needs_paint_properties);

      // 1. Compute clip in flow thread space of the containing flow thread.
      LayoutRect fragment_clip(iterator.ClipRectInFlowThread());
      // 2. Convert #1 to visual coordinates in the space of the flow thread.
      fragment_clip.Move(iterator.PaginationOffset());
      // 3. Adust #2 to visual coordinates in the containing "paint offset"
      // space.
      {
        DCHECK(context_.fragments[0].current.paint_offset_root);
        LayoutPoint pagination_visual_offset = VisualOffsetFromPaintOffsetRoot(
            context_.fragments[0], enclosing_pagination_layer);

        // Adjust for paint offset of the root, which may have a subpixel
        // component.
        // The paint offset root never has more than one fragment.
        pagination_visual_offset.MoveBy(
            context_.fragments[0]
                .current.paint_offset_root->FirstFragment()
                .PaintOffset());

        fragment_clip.MoveBy(pagination_visual_offset);
      }
      // 4. Match to parent fragments from the same containing flow thread.
      new_fragment_contexts.push_back(
          ContextForFragment(fragment_clip, context_.fragments));
      // 5. Save off PaginationOffset (which allows us to adjust
      // logical paint offsets into the space of the current fragment later.

      current_fragment_data->EnsureRarePaintData().SetPaginationOffset(
          ToLayoutPoint(iterator.PaginationOffset()));
    }
    if (current_fragment_data) {
      current_fragment_data->ClearNextFragment();
      context_.fragments = new_fragment_contexts;
    } else {
      // This will be an empty fragment - get rid of it?
      InitSingleFragmentFromParent(needs_paint_properties);
    }
  }

  if (object_.IsSVGHiddenContainer()) {
    // SVG resources are painted within one or more other locations in the
    // SVG during paint, and hence have their own independent paint property
    // trees, paint offset, etc.
    context_.fragments.clear();
    context_.fragments.Grow(1);
    PaintPropertyTreeBuilderFragmentContext& fragment_context =
        context_.fragments[0];

    fragment_context.current.paint_offset_root =
        fragment_context.absolute_position.paint_offset_root =
            fragment_context.fixed_position.paint_offset_root = &object_;

    object_.GetMutableForPainting().FirstFragment().ClearNextFragment();
  }
}

static inline bool ObjectTypeMightNeedPaintProperties(
    const LayoutObject& object) {
  return object.IsBoxModelObject() || object.IsSVG() ||
         object.PaintingLayer()->EnclosingPaginationLayer();
}

void ObjectPaintPropertyTreeBuilder::UpdatePaintingLayer() {
  bool changed_painting_layer = false;
  if (object_.HasLayer() &&
      ToLayoutBoxModelObject(object_).HasSelfPaintingLayer()) {
    context_.painting_layer = ToLayoutBoxModelObject(object_).Layer();
    changed_painting_layer = true;
  } else if (object_.IsColumnSpanAll() ||
             object_.IsFloatingWithNonContainingBlockParent()) {
    // See LayoutObject::paintingLayer() for the special-cases of floating under
    // inline and multicolumn.
    context_.painting_layer = object_.PaintingLayer();
    changed_painting_layer = true;
  }
  DCHECK(context_.painting_layer == object_.PaintingLayer());
}

void ObjectPaintPropertyTreeBuilder::UpdateForSelf() {
  UpdatePaintingLayer();

  if (ObjectTypeMightNeedPaintProperties(object_))
    UpdateFragments();
  else
    object_.GetMutableForPainting().FirstFragment().ClearNextFragment();

  auto* fragment_data = &object_.GetMutableForPainting().FirstFragment();
  for (auto& fragment_context : context_.fragments) {
    FragmentPaintPropertyTreeBuilder(object_, context_, fragment_context,
                                     *fragment_data)
        .UpdateForSelf();
    fragment_data = fragment_data->NextFragment();
  }
  DCHECK(!fragment_data);
}

void ObjectPaintPropertyTreeBuilder::UpdateForChildren() {
  if (!ObjectTypeMightNeedPaintProperties(object_))
    return;

  auto* fragment_data = &object_.GetMutableForPainting().FirstFragment();
  for (auto& fragment_context : context_.fragments) {
    FragmentPaintPropertyTreeBuilder(object_, context_, fragment_context,
                                     *fragment_data)
        .UpdateForChildren();
    context_.force_subtree_update |= object_.SubtreeNeedsPaintPropertyUpdate();
    fragment_data = fragment_data->NextFragment();
  }
  DCHECK(!fragment_data);

  if (object_.CanContainAbsolutePositionObjects())
    context_.container_for_absolute_position = &object_;
}

}  // namespace blink
