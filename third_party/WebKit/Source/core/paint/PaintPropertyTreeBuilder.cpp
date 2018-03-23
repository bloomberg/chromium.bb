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
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutTableSection.h"
#include "core/layout/LayoutView.h"
#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/LayoutSVGViewportContainer.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/paint/CSSMaskPainter.h"
#include "core/paint/ClipPathClipper.h"
#include "core/paint/FindPaintOffsetAndVisualRectNeedingUpdate.h"
#include "core/paint/FindPropertiesNeedingUpdate.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintPropertyTreePrinter.h"
#include "core/paint/SVGRootPainter.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "core/paint/compositing/CompositingReasonFinder.h"
#include "platform/geometry/TransformState.h"
#include "platform/transforms/TransformationMatrix.h"

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

// Returns true if we are printing which was initiated by the frame. We should
// ignore clipping and scroll transform on contents. WebLocalFrameImpl will
// issue artificial page clip for each page, and always print from the origin
// of the contents for which no scroll offset should be applied.
static bool IsPrintingRootFrame(const LocalFrame& frame) {
  if (!frame.GetDocument()->Printing())
    return false;

  const auto* parent_frame = frame.Tree().Parent();
  if (!parent_frame)
    return true;
  // TODO(crbug.com/455764): The local frame may be not the root frame of
  // printing when it's printing under a remote frame.
  if (!parent_frame->IsLocalFrame())
    return true;

  // If the parent frame is printing, this frame should clip normally.
  return !ToLocalFrame(parent_frame)->GetDocument()->Printing();
}

static bool IsPrintingRootLayoutView(const LayoutObject& object) {
  return object.IsLayoutView() && IsPrintingRootFrame(*object.GetFrame());
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

  context.current.containing_block_changed_under_filter = false;
  context.absolute_position.containing_block_changed_under_filter = false;
  context.fixed_position.containing_block_changed_under_filter = false;

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
    full_context.container_for_fixed_position = nullptr;
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
    bool property_added_or_removed = UpdatePreTranslation(
        frame_view, context.current.transform, frame_translate, FloatPoint3D());

    bool is_printing_root = IsPrintingRootFrame(frame_view.GetFrame());

    FloatRoundedRect content_clip(
        is_printing_root
            ? LayoutRect::InfiniteIntRect()
            : IntRect(IntPoint(), frame_view.VisibleContentSize()));
    property_added_or_removed |= UpdateContentClip(
        frame_view, context.current.clip, frame_view.PreTranslation(),
        content_clip, full_context.clip_changed);

    if (!is_printing_root && frame_view.IsScrollable()) {
      property_added_or_removed |= UpdateScroll(frame_view, context);
    } else if (frame_view.ScrollNode()) {
      // Ensure pre-existing properties are cleared if there is no scrolling.
      frame_view.SetScrollNode(nullptr);
      property_added_or_removed = true;
    }

    // A scroll translation node is created for static offset (e.g., overflow
    // hidden with scroll offset) or cases that scroll and have a scroll node.
    ScrollOffset scroll_offset = frame_view.GetScrollOffset();
    if (!is_printing_root &&
        (frame_view.IsScrollable() || !scroll_offset.IsZero())) {
      TransformationMatrix frame_scroll;
      frame_scroll.Translate(-scroll_offset.Width(), -scroll_offset.Height());
      property_added_or_removed |=
          UpdateScrollTranslation(frame_view, frame_view.PreTranslation(),
                                  frame_scroll, frame_view.ScrollNode());
    } else if (frame_view.ScrollTranslation()) {
      // Ensure pre-existing properties are cleared if there is no scrolling.
      frame_view.SetScrollTranslation(nullptr);
      property_added_or_removed = true;
    }
    full_context.painting_layer = frame_view.GetLayoutView()->Layer();

    if (property_added_or_removed) {
      full_context.force_subtree_update = true;
      // We need to update property tree states of paint chunks.
      if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
        frame_view.GetLayoutView()->Layer()->SetNeedsRepaint();
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
  full_context.container_for_fixed_position = nullptr;
  context.fixed_position = context.current;
  context.fixed_position.transform = fixed_transform_node;
  context.fixed_position.scroll = fixed_scroll_node;
  context.fixed_position.fixed_position_children_fixed_to_root = true;

  std::unique_ptr<PropertyTreeState> contents_state(new PropertyTreeState(
      context.current.transform, context.current.clip, context.current_effect));
  frame_view.SetTotalPropertyTreeStateForContents(std::move(contents_state));

#if DCHECK_IS_ON()
  PaintPropertyTreePrinter::UpdateDebugNames(frame_view);
#endif
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
        properties_(fragment_data.PaintProperties()) {}

  ~FragmentPaintPropertyTreeBuilder() {
    full_context_.force_subtree_update |= property_added_or_removed_;
#if DCHECK_IS_ON()
    if (properties_)
      PaintPropertyTreePrinter::UpdateDebugNames(object_, *properties_);
#endif
  }

  ALWAYS_INLINE void UpdateForSelf();
  ALWAYS_INLINE void UpdateForChildren();

  bool PropertyChanged() const { return property_changed_; }
  bool PropertyAddedOrRemoved() const { return property_added_or_removed_; }

 private:
  ALWAYS_INLINE void UpdatePaintOffset();
  ALWAYS_INLINE void UpdateForPaintOffsetTranslation(Optional<IntPoint>&);
  ALWAYS_INLINE void UpdatePaintOffsetTranslation(const Optional<IntPoint>&);
  ALWAYS_INLINE void SetNeedsPaintPropertyUpdateIfNeeded();
  ALWAYS_INLINE void UpdateForObjectLocationAndSize(
      Optional<IntPoint>& paint_offset_translation);
  ALWAYS_INLINE void UpdateClipPathCache();
  ALWAYS_INLINE void UpdateTransform();
  ALWAYS_INLINE void UpdateTransformForNonRootSVG();
  ALWAYS_INLINE void UpdateEffect();
  ALWAYS_INLINE void UpdateFilter();
  ALWAYS_INLINE void UpdateFragmentClip();
  ALWAYS_INLINE void UpdateCssClip();
  ALWAYS_INLINE void UpdateClipPathClip(bool spv1_compositing_specific_pass);
  ALWAYS_INLINE void UpdateLocalBorderBoxContext();
  ALWAYS_INLINE bool NeedsOverflowControlsClip() const;
  ALWAYS_INLINE void UpdateOverflowControlsClip();
  ALWAYS_INLINE void UpdateInnerBorderRadiusClip();
  ALWAYS_INLINE void UpdateOverflowClip();
  ALWAYS_INLINE void UpdatePerspective();
  ALWAYS_INLINE void UpdateSvgLocalToBorderBoxTransform();
  ALWAYS_INLINE void UpdateScrollAndScrollTranslation();
  ALWAYS_INLINE void UpdateOutOfFlowContext();

  bool NeedsPaintPropertyUpdate() const {
    return object_.NeedsPaintPropertyUpdate() ||
           full_context_.force_subtree_update;
  }

  void OnUpdate(const ObjectPaintProperties::UpdateResult& result) {
    property_added_or_removed_ |= result.NewNodeCreated();
    property_changed_ |= !result.Unchanged();
  }
  void OnUpdateClip(const ObjectPaintProperties::UpdateResult& result) {
    OnUpdate(result);
    full_context_.clip_changed |= !result.Unchanged();
  }
  void OnClear(bool cleared) {
    property_added_or_removed_ |= cleared;
    property_changed_ |= cleared;
  }
  void OnClearClip(bool cleared) {
    OnClear(cleared);
    full_context_.clip_changed |= cleared;
  }

  const LayoutObject& object_;
  // The tree builder context for the whole object.
  PaintPropertyTreeBuilderContext& full_context_;
  // The tree builder context for the current fragment, which is one of the
  // entries in |full_context.fragments|.
  PaintPropertyTreeBuilderFragmentContext& context_;
  FragmentData& fragment_data_;
  ObjectPaintProperties* properties_;
  bool property_changed_ = false;
  bool property_added_or_removed_ = false;
};

static bool NeedsScrollNode(const LayoutObject& object) {
  if (!object.HasOverflowClip())
    return false;
  return ToLayoutBox(object).GetScrollableArea()->ScrollsOverflow();
}

static CompositingReasons CompositingReasonsForScroll(const LayoutBox& box) {
  CompositingReasons compositing_reasons = CompositingReason::kNone;
  if (auto* scrollable_area = box.GetScrollableArea()) {
    if (auto* layer = scrollable_area->Layer()) {
      if (CompositingReasonFinder::RequiresCompositingForRootScroller(*layer))
        compositing_reasons |= CompositingReason::kRootScroller;
    }
  }
  // TODO(pdr): Set other compositing reasons for scroll here, see:
  // PaintLayerScrollableArea::ComputeNeedsCompositedScrolling.
  return compositing_reasons;
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

  // <foreignObject> inherits no paint offset, because there is no such
  // concept within SVG. However, the foreign object can have its own paint
  // offset due to the x and y parameters of the element. This affects the
  // offset of painting of the <foreignObject> element and its children.
  // However, <foreignObject> otherwise behaves like other SVG elements, in
  // that the x and y offset is applied *after* any transform, instead of
  // before. Therefore there is no paint offset translation needed.
  if (object.IsSVGForeignObject())
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
    OnUpdate(properties_->UpdatePaintOffsetTranslation(
        context_.current.transform,
        TransformationMatrix().Translate(paint_offset_translation->X(),
                                         paint_offset_translation->Y()),
        FloatPoint3D(), context_.current.should_flatten_inherited_transform,
        context_.current.rendering_context_id));
    context_.current.transform = properties_->PaintOffsetTranslation();
    if (RuntimeEnabledFeatures::RootLayerScrollingEnabled() &&
        object_.IsLayoutView()) {
      context_.absolute_position.transform =
          properties_->PaintOffsetTranslation();
      context_.fixed_position.transform = properties_->PaintOffsetTranslation();
    }
  } else {
    OnClear(properties_->ClearPaintOffsetTranslation());
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

  if (NeedsPaintPropertyUpdate()) {
    AffineTransform transform = object_.LocalToSVGParentTransform();
    if (NeedsTransformForNonRootSVG(object_)) {
      // The origin is included in the local transform, so leave origin empty.
      OnUpdate(properties_->UpdateTransform(context_.current.transform,
                                            TransformationMatrix(transform),
                                            FloatPoint3D()));
    } else {
      OnClear(properties_->ClearTransform());
    }
  }

  if (properties_->Transform()) {
    context_.current.transform = properties_->Transform();
    context_.current.should_flatten_inherited_transform = false;
    context_.current.rendering_context_id = 0;
  }
}

static CompositingReasons CompositingReasonsForTransform(const LayoutBox& box) {
  if (!box.HasLayer())
    return CompositingReason::kNone;

  const ComputedStyle& style = box.StyleRef();
  CompositingReasons compositing_reasons = CompositingReason::kNone;
  if (CompositingReasonFinder::RequiresCompositingForTransform(box))
    compositing_reasons |= CompositingReason::k3DTransform;

  if (CompositingReasonFinder::RequiresCompositingForTransformAnimation(style))
    compositing_reasons |= CompositingReason::kActiveTransformAnimation;

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

  if (NeedsPaintPropertyUpdate()) {
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

      OnUpdate(properties_->UpdateTransform(
          context_.current.transform, matrix, TransformOrigin(box),
          context_.current.should_flatten_inherited_transform,
          rendering_context_id, compositing_reasons,
          CompositorElementIdFromUniqueObjectId(
              object_.UniqueId(), CompositorElementIdNamespace::kPrimary)));
    } else {
      OnClear(properties_->ClearTransform());
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

static bool NeedsClipPathClip(const LayoutObject& object) {
  if (!object.StyleRef().ClipPath())
    return false;

  return object.FirstFragment().ClipPathPath();
}

static bool NeedsEffect(const LayoutObject& object) {
  const ComputedStyle& style = object.StyleRef();

  const bool is_css_isolated_group =
      object.IsBoxModelObject() && style.IsStackingContext();

  if (!is_css_isolated_group && !object.IsSVG())
    return false;

  if (object.IsSVG()) {
    if (object.IsSVGRoot() && is_css_isolated_group &&
        object.HasNonIsolatedBlendingDescendants())
      return true;
    if (SVGLayoutSupport::IsIsolationRequired(&object))
      return true;
    if (SVGResources* resources =
            SVGResourcesCache::CachedResourcesForLayoutObject(object)) {
      if (resources->Masker()) {
        return true;
      }
    }
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

  if (object.StyleRef().HasMask())
    return true;

  if (object.HasLayer() &&
      ToLayoutBoxModelObject(object).Layer()->GetCompositedLayerMapping() &&
      ToLayoutBoxModelObject(object)
          .Layer()
          ->GetCompositedLayerMapping()
          ->MaskLayer()) {
    // In SPv1* a mask layer can be created for clip-path in absence of mask,
    // and a mask effect node must be created whether the clip-path is
    // path-based or not.
    return true;
  }

  if (object.StyleRef().ClipPath() &&
      object.FirstFragment().ClipPathBoundingBox() &&
      !object.FirstFragment().ClipPathPath()) {
    // If the object has a valid clip-path but can't use path-based clip-path,
    // a clip-path effect node must be created.
    return true;
  }

  return false;
}

void FragmentPaintPropertyTreeBuilder::UpdateEffect() {
  DCHECK(properties_);
  const ComputedStyle& style = object_.StyleRef();

  // TODO(trchen): Can't omit effect node if we have 3D children.
  if (NeedsPaintPropertyUpdate()) {
    // Use the current clip as output_clip for SVG children because their
    // effects never interleave with clips.
    const ClipPaintPropertyNode* output_clip =
        object_.IsSVGChild() ? context_.current.clip : nullptr;
    if (NeedsEffect(object_)) {
      // We may begin to composite our subtree prior to an animation starts,
      // but a compositor element ID is only needed when an animation is
      // current.
      CompositingReasons compositing_reasons = CompositingReason::kNone;
      if (CompositingReasonFinder::RequiresCompositingForOpacityAnimation(
              style)) {
        compositing_reasons = CompositingReason::kActiveOpacityAnimation;
      }

      Optional<IntRect> mask_clip = CSSMaskPainter::MaskBoundingBox(
          object_, context_.current.paint_offset);
      bool has_clip_path =
          style.ClipPath() && fragment_data_.ClipPathBoundingBox();
      bool has_spv1_composited_clip_path =
          has_clip_path && object_.HasLayer() &&
          ToLayoutBoxModelObject(object_).Layer()->GetCompositedLayerMapping();
      bool has_mask_based_clip_path =
          has_clip_path && !fragment_data_.ClipPathPath();
      Optional<IntRect> clip_path_clip;
      if (has_spv1_composited_clip_path || has_mask_based_clip_path) {
        clip_path_clip = fragment_data_.ClipPathBoundingBox();
      }
      if ((mask_clip || clip_path_clip) &&
          // TODO(crbug.com/768691): Remove the following condition after mask
          // clip doesn't fail fast/borders/inline-mask-overlay-image-outset-
          // vertical-rl.html.
          RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
        IntRect combined_clip = mask_clip ? *mask_clip : *clip_path_clip;
        if (mask_clip && clip_path_clip)
          combined_clip.Intersect(*clip_path_clip);

        OnUpdateClip(properties_->UpdateMaskClip(
            context_.current.clip, context_.current.transform,
            FloatRoundedRect(combined_clip)));
        output_clip = properties_->MaskClip();
      } else {
        OnClearClip(properties_->ClearMaskClip());
      }

      SkBlendMode blend_mode =
          object_.IsBlendingAllowed()
              ? WebCoreCompositeToSkiaComposite(kCompositeSourceOver,
                                                style.BlendMode())
              : SkBlendMode::kSrcOver;

      OnUpdate(properties_->UpdateEffect(
          context_.current_effect, context_.current.transform, output_clip,
          kColorFilterNone, CompositorFilterOperations(), style.Opacity(),
          blend_mode, compositing_reasons,
          CompositorElementIdFromUniqueObjectId(
              object_.UniqueId(), CompositorElementIdNamespace::kPrimary)));
      if (mask_clip || has_spv1_composited_clip_path) {
        OnUpdate(properties_->UpdateMask(
            properties_->Effect(), context_.current.transform, output_clip,
            CSSMaskPainter::MaskColorFilter(object_),
            CompositorFilterOperations(), 1.f, SkBlendMode::kDstIn,
            CompositingReason::kNone,
            CompositorElementIdFromUniqueObjectId(
                object_.UniqueId(),
                CompositorElementIdNamespace::kEffectMask)));
      } else {
        OnClear(properties_->ClearMask());
      }
      if (has_mask_based_clip_path) {
        const EffectPaintPropertyNode* parent = has_spv1_composited_clip_path
                                                    ? properties_->Mask()
                                                    : properties_->Effect();
        OnUpdate(properties_->UpdateClipPath(
            parent, context_.current.transform, output_clip, kColorFilterNone,
            CompositorFilterOperations(), 1.f, SkBlendMode::kDstIn,
            CompositingReason::kNone,
            CompositorElementIdFromUniqueObjectId(
                object_.UniqueId(),
                CompositorElementIdNamespace::kEffectClipPath)));
      } else {
        OnClear(properties_->ClearClipPath());
      }
    } else {
      OnClear(properties_->ClearEffect());
      OnClear(properties_->ClearMask());
      OnClear(properties_->ClearClipPath());
      OnClearClip(properties_->ClearMaskClip());
    }
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

  if (NeedsPaintPropertyUpdate()) {
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
              ? CompositingReason::kActiveFilterAnimation
              : CompositingReason::kNone;
      DCHECK(!style.HasCurrentFilterAnimation() ||
             compositing_reasons != CompositingReason::kNone);

      OnUpdate(properties_->UpdateFilter(
          context_.current_effect, context_.current.transform, output_clip,
          kColorFilterNone, std::move(filter), 1.f, SkBlendMode::kSrcOver,
          compositing_reasons,
          CompositorElementIdFromUniqueObjectId(
              object_.UniqueId(), CompositorElementIdNamespace::kEffectFilter),
          FloatPoint(context_.current.paint_offset)));
    } else {
      OnClear(properties_->ClearFilter());
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

static FloatRoundedRect ToClipRect(const LayoutRect& rect) {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return FloatRoundedRect(FloatRect(PixelSnappedIntRect(rect)));
  return FloatRoundedRect(FloatRect(rect));
}

void FragmentPaintPropertyTreeBuilder::UpdateFragmentClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (context_.fragment_clip) {
      OnUpdateClip(properties_->UpdateFragmentClip(
          context_.current.clip, context_.current.transform,
          ToClipRect(*context_.fragment_clip)));
    } else {
      OnClearClip(properties_->ClearFragmentClip());
    }
  }

  if (properties_->FragmentClip())
    context_.current.clip = properties_->FragmentClip();
}

static bool NeedsCssClip(const LayoutObject& object) {
  return object.HasClip();
}

void FragmentPaintPropertyTreeBuilder::UpdateCssClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsCssClip(object_)) {
      // Create clip node for descendants that are not fixed position.
      // We don't have to setup context.absolutePosition.clip here because this
      // object must be a container for absolute position descendants, and will
      // copy from in-flow context later at updateOutOfFlowContext() step.
      DCHECK(object_.CanContainAbsolutePositionObjects());
      OnUpdateClip(properties_->UpdateCssClip(
          context_.current.clip, context_.current.transform,
          ToClipRect(
              ToLayoutBox(object_).ClipRect(context_.current.paint_offset))));
    } else {
      OnClearClip(properties_->ClearCssClip());
    }
  }

  if (properties_->CssClip())
    context_.current.clip = properties_->CssClip();
}

void FragmentPaintPropertyTreeBuilder::UpdateClipPathClip(
    bool spv1_compositing_specific_pass) {
  if (!NeedsPaintPropertyUpdate())
    return;

  // In SPv1*, composited path-based clip-path applies to a mask paint chunk
  // instead of actual contents. We have to delay until mask clip node has been
  // created first so we can parent under it.
  bool is_spv1_composited =
      object_.HasLayer() &&
      ToLayoutBoxModelObject(object_).Layer()->GetCompositedLayerMapping();
  if (is_spv1_composited != spv1_compositing_specific_pass)
    return;

  if (!NeedsClipPathClip(object_)) {
    OnClearClip(properties_->ClearClipPathClip());
  } else {
    OnUpdateClip(properties_->UpdateClipPathClip(
        context_.current.clip, context_.current.transform,
        FloatRoundedRect(FloatRect(*fragment_data_.ClipPathBoundingBox())),
        nullptr, fragment_data_.ClipPathPath()));
  }

  if (properties_->ClipPathClip() && !spv1_compositing_specific_pass) {
    context_.current.clip = context_.absolute_position.clip =
        context_.fixed_position.clip = properties_->ClipPathClip();
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateLocalBorderBoxContext() {
  if (!NeedsPaintPropertyUpdate())
    return;

  if (!object_.HasLayer() && !NeedsPaintOffsetTranslation(object_)) {
    fragment_data_.ClearLocalBorderBoxProperties();
  } else {
    PropertyTreeState local_border_box =
        PropertyTreeState(context_.current.transform, context_.current.clip,
                          context_.current_effect);

    if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled() &&
        (!fragment_data_.HasLocalBorderBoxProperties() ||
         local_border_box != fragment_data_.LocalBorderBoxProperties()))
      property_added_or_removed_ = true;

    fragment_data_.SetLocalBorderBoxProperties(std::move(local_border_box));
  }
}

static bool NeedsOverflowClip(const LayoutObject& object) {
  // Though a SVGForeignObject is a LayoutBox, its overflow clip logic is
  // special because it doesn't create a PaintLayer.
  // See LayoutSVGBlock::AllowsOverflowClip().
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled() &&
      object.IsSVGViewportContainer() &&
      SVGLayoutSupport::IsOverflowHidden(object))
    return true;

  return object.IsBox() && ToLayoutBox(object).ShouldClipOverflow() &&
         !IsPrintingRootLayoutView(object);
}

bool FragmentPaintPropertyTreeBuilder::NeedsOverflowControlsClip() const {
  if (!object_.HasOverflowClip())
    return false;

  const auto& box = ToLayoutBox(object_);
  const auto* scrollable_area = box.GetScrollableArea();
  IntRect scroll_controls_bounds =
      scrollable_area->ScrollCornerAndResizerRect();
  if (const auto* scrollbar = scrollable_area->HorizontalScrollbar())
    scroll_controls_bounds.Unite(scrollbar->FrameRect());
  if (const auto* scrollbar = scrollable_area->VerticalScrollbar())
    scroll_controls_bounds.Unite(scrollbar->FrameRect());
  auto pixel_snapped_border_box_rect = box.PixelSnappedBorderBoxRect(
      ToLayoutSize(context_.current.paint_offset));
  pixel_snapped_border_box_rect.SetLocation(IntPoint());
  return !pixel_snapped_border_box_rect.Contains(scroll_controls_bounds);
}

static bool NeedsInnerBorderRadiusClip(const LayoutObject& object) {
  if (!object.StyleRef().HasBorderRadius())
    return false;
  if (object.IsBox() && NeedsOverflowClip(object))
    return true;
  // LayoutReplaced applies inner border-radius clip on the foreground. This
  // doesn't apply to SVGRoot which uses the NeedsOverflowClip() rule above.
  // This includes iframes which applies border-radius clip on the subdocument.
  if (object.IsLayoutReplaced() && !object.IsSVGRoot())
    return true;
  return false;
}

static LayoutPoint VisualOffsetFromPaintOffsetRoot(
    const PaintPropertyTreeBuilderFragmentContext& context,
    const PaintLayer* child) {
  const LayoutObject* paint_offset_root = context.current.paint_offset_root;
  PaintLayer* painting_layer = paint_offset_root->PaintingLayer();
  LayoutPoint result = child->VisualOffsetFromAncestor(painting_layer);
  if (!paint_offset_root->HasLayer() ||
      ToLayoutBoxModelObject(paint_offset_root)->Layer() != painting_layer) {
    result.Move(-paint_offset_root->OffsetFromAncestorContainer(
        &painting_layer->GetLayoutObject()));
  }

  // Don't include scroll offset of paint_offset_root. Any scroll is
  // already included in a separate transform node.
  if (paint_offset_root->HasOverflowClip())
    result += ToLayoutBox(paint_offset_root)->ScrolledContentOffset();
  return result;
}

void FragmentPaintPropertyTreeBuilder::UpdateOverflowControlsClip() {
  DCHECK(properties_);

  if (!NeedsPaintPropertyUpdate())
    return;

  if (NeedsOverflowControlsClip()) {
    // Clip overflow controls to the border box rect.
    properties_->UpdateOverflowControlsClip(
        context_.current.clip, context_.current.transform,
        ToClipRect(LayoutRect(context_.current.paint_offset,
                              ToLayoutBox(object_).Size())));
  } else {
    properties_->ClearOverflowControlsClip();
  }

  // No need to set force_subtree_update and clip_changed because
  // OverflowControlsClip applies to overflow controls only, not descendants.
  // We also don't walk into custom scrollbars in PrePaintTreeWalk and
  // LayoutObjects under custom scrollbars don't support paint properties.
}

void FragmentPaintPropertyTreeBuilder::UpdateInnerBorderRadiusClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsInnerBorderRadiusClip(object_)) {
      const LayoutBox& box = ToLayoutBox(object_);
      FloatRoundedRect inner_border;
      if (box.IsLayoutReplaced()) {
        // LayoutReplaced clips the foreground by rounded inner content box.
        inner_border = box.StyleRef().GetRoundedInnerBorderFor(
            LayoutRect(context_.current.paint_offset, box.Size()),
            LayoutRectOutsets(-(box.PaddingTop() + box.BorderTop()),
                              -(box.PaddingRight() + box.BorderRight()),
                              -(box.PaddingBottom() + box.BorderBottom()),
                              -(box.PaddingLeft() + box.BorderLeft())));
      } else {
        inner_border = box.StyleRef().GetRoundedInnerBorderFor(
            LayoutRect(context_.current.paint_offset, box.Size()));
      }
      OnUpdateClip(properties_->UpdateInnerBorderRadiusClip(
          context_.current.clip, context_.current.transform, inner_border));
    } else {
      OnClearClip(properties_->ClearInnerBorderRadiusClip());
    }
  }

  if (auto* border_radius_clip = properties_->InnerBorderRadiusClip())
    context_.current.clip = border_radius_clip;
}

void FragmentPaintPropertyTreeBuilder::UpdateOverflowClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsOverflowClip(object_)) {
      FloatRoundedRect clip_rect;
      FloatRoundedRect clip_rect_excluding_overlay_scrollbars;
      if (!RuntimeEnabledFeatures::SlimmingPaintV175Enabled() &&
          object_.IsSVGForeignObject()) {
        clip_rect = ToClipRect(ToLayoutBox(object_).FrameRect());
        clip_rect_excluding_overlay_scrollbars = clip_rect;
      } else if (object_.IsBox()) {
        clip_rect = ToClipRect(ToLayoutBox(object_).OverflowClipRect(
            context_.current.paint_offset));
        clip_rect_excluding_overlay_scrollbars =
            ToClipRect(ToLayoutBox(object_).OverflowClipRect(
                context_.current.paint_offset,
                kExcludeOverlayScrollbarSizeForHitTesting));
      } else {
        DCHECK(object_.IsSVGViewportContainer());
        const auto& viewport_container = ToLayoutSVGViewportContainer(object_);
        clip_rect = FloatRoundedRect(
            viewport_container.LocalToSVGParentTransform().Inverse().MapRect(
                viewport_container.Viewport()));
        clip_rect_excluding_overlay_scrollbars = clip_rect;
      }

      bool should_create_overflow_clip = true;
      if (auto* border_radius_clip = properties_->InnerBorderRadiusClip()) {
        if (border_radius_clip->ClipRect().Rect() == clip_rect.Rect() &&
            clip_rect == clip_rect_excluding_overlay_scrollbars)
          should_create_overflow_clip = false;
      }
      if (should_create_overflow_clip) {
        OnUpdateClip(properties_->UpdateOverflowClip(
            context_.current.clip, context_.current.transform, clip_rect,
            &clip_rect_excluding_overlay_scrollbars));
      } else {
        OnClearClip(properties_->ClearOverflowClip());
      }
    } else {
      OnClearClip(properties_->ClearOverflowClip());
    }
  }

  if (auto* overflow_clip = OverflowClip(*properties_))
    context_.current.clip = overflow_clip;
}

static FloatPoint PerspectiveOrigin(const LayoutBox& box) {
  const ComputedStyle& style = box.StyleRef();
  // Perspective origin has no effect without perspective.
  DCHECK(style.HasPerspective());
  FloatSize border_box_size(box.Size());
  return FloatPointForLengthPoint(style.PerspectiveOrigin(), border_box_size);
}

static bool NeedsPerspective(const LayoutObject& object) {
  return object.IsBox() && object.StyleRef().HasPerspective();
}

void FragmentPaintPropertyTreeBuilder::UpdatePerspective() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsPerspective(object_)) {
      const ComputedStyle& style = object_.StyleRef();
      // The perspective node must not flatten (else nothing will get
      // perspective), but it should still extend the rendering context as
      // most transform nodes do.
      TransformationMatrix matrix =
          TransformationMatrix().ApplyPerspective(style.Perspective());
      FloatPoint3D origin = PerspectiveOrigin(ToLayoutBox(object_)) +
                            ToLayoutSize(context_.current.paint_offset);
      OnUpdate(properties_->UpdatePerspective(
          context_.current.transform, matrix, origin,
          context_.current.should_flatten_inherited_transform,
          context_.current.rendering_context_id));
    } else {
      OnClear(properties_->ClearPerspective());
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

  if (NeedsPaintPropertyUpdate()) {
    AffineTransform transform_to_border_box =
        SVGRootPainter(ToLayoutSVGRoot(object_))
            .TransformToPixelSnappedBorderBox(context_.current.paint_offset);
    if (!transform_to_border_box.IsIdentity() &&
        NeedsSVGLocalToBorderBoxTransform(object_)) {
      OnUpdate(properties_->UpdateSvgLocalToBorderBoxTransform(
          context_.current.transform, transform_to_border_box, FloatPoint3D()));
    } else {
      OnClear(properties_->ClearSvgLocalToBorderBoxTransform());
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

  if (NeedsPaintPropertyUpdate()) {
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

      OnUpdate(properties_->UpdateScroll(
          context_.current.scroll, container_rect, contents_rect,
          user_scrollable_horizontal, user_scrollable_vertical, reasons,
          element_id));
    } else {
      OnClear(properties_->ClearScroll());
    }

    // A scroll translation node is created for static offset (e.g., overflow
    // hidden with scroll offset) or cases that scroll and have a scroll node.
    if (NeedsScrollOrScrollTranslation(object_)) {
      const LayoutBox& box = ToLayoutBox(object_);
      IntSize scroll_offset = box.ScrolledContentOffset();
      TransformationMatrix scroll_offset_matrix =
          TransformationMatrix().Translate(-scroll_offset.Width(),
                                           -scroll_offset.Height());
      CompositingReasons compositing_reasons = CompositingReasonsForScroll(box);
      OnUpdate(properties_->UpdateScrollTranslation(
          context_.current.transform, scroll_offset_matrix, FloatPoint3D(),
          context_.current.should_flatten_inherited_transform,
          context_.current.rendering_context_id, compositing_reasons,
          CompositorElementId(), properties_->Scroll()));
    } else {
      OnClear(properties_->ClearScrollTranslation());
    }
  }

  if (properties_->Scroll())
    context_.current.scroll = properties_->Scroll();
  if (properties_->ScrollTranslation()) {
    context_.current.transform = properties_->ScrollTranslation();
    context_.current.should_flatten_inherited_transform = false;
  }
}

static inline bool ContextsDiffer(
    const PaintPropertyTreeBuilderFragmentContext::ContainingBlockContext& a,
    const PaintPropertyTreeBuilderFragmentContext::ContainingBlockContext& b) {
  if (a.clip != b.clip)
    return true;
  if (a.transform != b.transform)
    return true;
  if (a.paint_offset != b.paint_offset)
    return true;
  if (a.scroll != b.scroll)
    return true;
  return false;
}

void FragmentPaintPropertyTreeBuilder::UpdateOutOfFlowContext() {
  if (!object_.IsBoxModelObject() && !properties_)
    return;

  if (object_.IsLayoutBlock())
    context_.paint_offset_for_float = context_.current.paint_offset;

  if (object_.CanContainAbsolutePositionObjects()) {
    context_.absolute_position = context_.current;
  }

  if (object_.IsLayoutView()) {
    if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      const auto* initial_fixed_transform = context_.fixed_position.transform;
      const auto* initial_fixed_scroll = context_.fixed_position.scroll;

      context_.fixed_position = context_.current;
      context_.fixed_position.containing_block_changed_under_filter = false;
      context_.fixed_position.fixed_position_children_fixed_to_root = true;

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
      if (NeedsPaintPropertyUpdate()) {
        OnUpdate(properties_->UpdateCssClipFixedPosition(
            context_.fixed_position.clip,
            const_cast<TransformPaintPropertyNode*>(
                css_clip->LocalTransformSpace()),
            css_clip->ClipRect()));
      }
      if (properties_->CssClipFixedPosition())
        context_.fixed_position.clip = properties_->CssClipFixedPosition();
      return;
    }
  }

  if (NeedsFilter(object_)) {
    if (ContextsDiffer(context_.current, context_.absolute_position))
      context_.absolute_position.containing_block_changed_under_filter = true;

    if (ContextsDiffer(context_.current, context_.fixed_position))
      context_.fixed_position.containing_block_changed_under_filter = true;
  }

  if (NeedsPaintPropertyUpdate() && properties_)
    OnClear(properties_->ClearCssClipFixedPosition());
}

static LayoutRect BorderBoxRectInPaginationContainer(
    const LayoutBox& box,
    const PaintLayer& enclosing_pagination_layer) {
  auto rect = box.BorderBoxRect();
  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 FloatPoint(rect.Location()));
  box.MapLocalToAncestor(&enclosing_pagination_layer.GetLayoutObject(),
                         transform_state, kApplyContainerFlip);
  transform_state.Flatten();
  return LayoutRect(LayoutPoint(transform_state.LastPlanarPoint()),
                    rect.Size());
}

static LayoutRect BoundingBoxInPaginationContainer(
    const LayoutObject& object,
    const PaintLayer& enclosing_pagination_layer,
    bool& should_repeat_in_fragments) {
  // Non-boxes that have no layer paint in the space of their containing block.
  if (!object.IsBox() && !object.HasLayer()) {
    return BoundingBoxInPaginationContainer(*object.ContainingBlock(),
                                            enclosing_pagination_layer,
                                            should_repeat_in_fragments);
  }

  should_repeat_in_fragments = false;

  // The special path for layers ensures that the bounding box also covers
  // contents visual overflow, so that the fragments will cover all fragments of
  // contents except for self-painting layers, because we initiate fragment
  // painting of contents from the layer.
  // Table section may repeat, and doesn't need the special layer path because
  // it doesn't have contents visual overflow.
  if (object.HasLayer() && !object.IsTableSection()) {
    return ToLayoutBoxModelObject(object).Layer()->PhysicalBoundingBox(
        &enclosing_pagination_layer);
  }

  // Compute the bounding box without transforms.
  // The object is guaranteed to be a box due to the logic above.
  auto bounding_box = BorderBoxRectInPaginationContainer(
      ToLayoutBox(object), enclosing_pagination_layer);

  if (!object.IsTableSection())
    return bounding_box;
  const auto& section = ToLayoutTableSection(object);
  if (!section.IsRepeatingHeaderGroup() && !section.IsRepeatingFooterGroup())
    return bounding_box;

  const auto& table = *section.Table();
  should_repeat_in_fragments = true;

  if (section.IsRepeatingHeaderGroup()) {
    // Now bounding_box covers the original header. Expand it to intersect
    // with all fragments containing the original and repeatings, i.e. to
    // intersect any fragment containing any row.
    if (const auto* bottom_section = table.BottomNonEmptySection()) {
      bounding_box.Unite(BorderBoxRectInPaginationContainer(
          *bottom_section, enclosing_pagination_layer));
    }
    return bounding_box;
  }

  DCHECK(section.IsRepeatingFooterGroup());
  // Similar to repeating header, expand bounding_box to intersect any
  // fragment containing any row first.
  const auto* top_section = table.TopNonEmptySection();
  if (top_section) {
    bounding_box.Unite(BorderBoxRectInPaginationContainer(
        *top_section, enclosing_pagination_layer));
    // However, the first fragment intersecting the expanded bounding_box may
    // not have enough space to contain the repeating footer. Exclude the
    // total height of the first row and repeating footers from the top of
    // bounding_box to exclude the first fragment without enough space.
    auto top_exclusion = table.RowOffsetFromRepeatingFooter();
    if (const auto* top_section = table.TopNonEmptySection()) {
      // Otherwise the footer should not repeating.
      DCHECK(top_section != section);
      top_exclusion +=
          top_section->FirstRow()->LogicalHeight() + table.VBorderSpacing();
    }
    // Subtract 1 to ensure overlap of 1 px for a fragment that has exactly
    // one row plus space for the footer.
    if (top_exclusion)
      top_exclusion -= 1;
    bounding_box.ShiftYEdgeTo(bounding_box.Y() + top_exclusion);
  }
  return bounding_box;
}

static LayoutPoint PaintOffsetInPaginationContainer(
    const LayoutObject& object,
    const PaintLayer& enclosing_pagination_layer) {
  // Non-boxes use their containing blocks' paint offset.
  if (!object.IsBox() && !object.HasLayer()) {
    return PaintOffsetInPaginationContainer(*object.ContainingBlock(),
                                            enclosing_pagination_layer);
  }

  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 FloatPoint());
  object.MapLocalToAncestor(&enclosing_pagination_layer.GetLayoutObject(),
                            transform_state, kApplyContainerFlip);
  transform_state.Flatten();
  return LayoutPoint(transform_state.LastPlanarPoint());
}

void FragmentPaintPropertyTreeBuilder::UpdatePaintOffset() {
  // Paint offsets for fragmented content are computed from scratch.
  const auto* enclosing_pagination_layer =
      full_context_.painting_layer->EnclosingPaginationLayer();
  if (enclosing_pagination_layer &&
      // Except if the paint_offset_root is below the pagination container,
      // in which case fragmentation offsets are already baked into the paint
      // offset transform for paint_offset_root.
      !context_.current.paint_offset_root->PaintingLayer()
           ->EnclosingPaginationLayer()) {
    // Set fragment visual paint offset.
    LayoutPoint paint_offset =
        PaintOffsetInPaginationContainer(object_, *enclosing_pagination_layer);

    paint_offset.MoveBy(fragment_data_.PaginationOffset());
    paint_offset.Move(context_.repeating_paint_offset_adjustment);
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
        if (container && container->IsLayoutInline()) {
          DCHECK(container->CanContainAbsolutePositionObjects());
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
      case EPosition::kFixed: {
        DCHECK(full_context_.container_for_fixed_position ==
               box_model_object.Container());
        context_.current = context_.fixed_position;
        // Fixed-position elements that are fixed to the vieport have a
        // transform above the scroll of the LayoutView. Child content is
        // relative to that transform, and hence the fixed-position element.
        if (context_.fixed_position.fixed_position_children_fixed_to_root)
          context_.current.paint_offset_root = &box_model_object;

        const auto* container = full_context_.container_for_fixed_position;
        if (container && container->IsLayoutInline()) {
          DCHECK(container->CanContainFixedPositionObjects());
          DCHECK(box_model_object.IsBox());
          context_.current.paint_offset +=
              ToLayoutInline(container)->OffsetForInFlowPositionedInline(
                  ToLayoutBox(box_model_object));
        }
        break;
      }
      default:
        NOTREACHED();
    }
  }

  if (context_.current.containing_block_changed_under_filter) {
    UseCounter::Count(object_.GetDocument(),
                      WebFeature::kFilterAsContainingBlockMayChangeOutput);
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

void FragmentPaintPropertyTreeBuilder::SetNeedsPaintPropertyUpdateIfNeeded() {
  if (!object_.IsBox())
    return;

  const LayoutBox& box = ToLayoutBox(object_);
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
      box_generates_property_nodes_for_mask_and_clip_path) {
    box.GetMutableForPainting().SetNeedsPaintPropertyUpdate();
  }

  if (box.HasClipPath())
    box.GetMutableForPainting().InvalidateClipPathCache();

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
    fragment_data_.InvalidateClipPathCache();
  }

  if (paint_offset_translation)
    context_.current.paint_offset_root = &ToLayoutBoxModelObject(object_);
}

void FragmentPaintPropertyTreeBuilder::UpdateClipPathCache() {
  if (fragment_data_.IsClipPathCacheValid())
    return;

  if (!object_.StyleRef().ClipPath())
    return;

  Optional<FloatRect> bounding_box =
      ClipPathClipper::LocalClipPathBoundingBox(object_);
  if (!bounding_box) {
    fragment_data_.SetClipPathCache(WTF::nullopt, nullptr);
    return;
  }
  bounding_box->MoveBy(FloatPoint(fragment_data_.PaintOffset()));

  bool is_valid = false;
  Optional<Path> path = ClipPathClipper::PathBasedClip(
      object_, object_.IsSVGChild(),
      ClipPathClipper::LocalReferenceBox(object_), is_valid);
  DCHECK(is_valid);
  if (path)
    path->Translate(ToFloatSize(FloatPoint(fragment_data_.PaintOffset())));
  fragment_data_.SetClipPathCache(
      EnclosingIntRect(*bounding_box),
      path ? AdoptRef(new RefCountedPath(std::move(*path))) : nullptr);
}

void FragmentPaintPropertyTreeBuilder::UpdateForSelf() {
  // This is not in FindObjectPropertiesNeedingUpdateScope because paint offset
  // can change without NeedsPaintPropertyUpdate.
  Optional<IntPoint> paint_offset_translation;
  UpdateForObjectLocationAndSize(paint_offset_translation);
  if (&fragment_data_ == &object_.FirstFragment())
    SetNeedsPaintPropertyUpdateIfNeeded();
  UpdateClipPathCache();

  if (properties_) {
    // TODO(wangxianzhu): Put these in FindObjectPropertiesNeedingUpdateScope.
    UpdateFragmentClip();
    UpdatePaintOffsetTranslation(paint_offset_translation);
  }

#if DCHECK_IS_ON()
  FindObjectPropertiesNeedingUpdateScope check_needs_update_scope(
      object_, fragment_data_, full_context_.force_subtree_update);
#endif

  if (properties_) {
    UpdateTransform();
    UpdateClipPathClip(false);
    if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
      UpdateEffect();
    UpdateClipPathClip(true);  // Special pass for SPv1 composited clip-path.
    UpdateCssClip();
    UpdateFilter();
    UpdateOverflowControlsClip();
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

void ObjectPaintPropertyTreeBuilder::InitFragmentPaintProperties(
    FragmentData& fragment,
    bool needs_paint_properties,
    const LayoutPoint& pagination_offset,
    LayoutUnit logical_top_in_flow_thread) {
  if (needs_paint_properties) {
    fragment.EnsurePaintProperties();
  } else if (fragment.PaintProperties()) {
    context_.force_subtree_update = true;
    fragment.ClearPaintProperties();
  }
  fragment.SetPaginationOffset(pagination_offset);
  fragment.SetLogicalTopInFlowThread(logical_top_in_flow_thread);
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
    context_.fragments[0].logical_top_in_flow_thread = LayoutUnit();
  }

  if (object_.IsColumnSpanAll()) {
    // Column-span:all skips pagination container in the tree hierarchy, so
    // it should also skip any fragment clip created by the skipped pagination
    // container.
    if (const auto* pagination_layer_in_tree_hierarchy =
            object_.Parent()->EnclosingLayer()->EnclosingPaginationLayer()) {
      const auto* properties =
          pagination_layer_in_tree_hierarchy->GetLayoutObject()
              .FirstFragment()
              .PaintProperties();
      if (properties && properties->FragmentClip()) {
        context_.fragments[0].current.clip =
            properties->FragmentClip()->Parent();
      }
    }
  }
}

void ObjectPaintPropertyTreeBuilder::UpdateCompositedLayerPaginationOffset() {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  // TODO(crbug.com/797779): Implement fragments across frame boundaries.

  const auto* enclosing_pagination_layer =
      context_.painting_layer->EnclosingPaginationLayer();
  if (!enclosing_pagination_layer)
    return;

  // We reach here because context_.painting_layer is in a composited layer
  // under the pagination layer. SPv1* doesn't fragment composited layers,
  // but we still need to set correct pagination offset for correct paint
  // offset calculation.
  FragmentData& first_fragment =
      object_.GetMutableForPainting().FirstFragment();
  bool is_paint_invalidation_container = object_.IsPaintInvalidationContainer();
  const auto* parent_composited_layer =
      context_.painting_layer->EnclosingLayerWithCompositedLayerMapping(
          is_paint_invalidation_container ? kExcludeSelf : kIncludeSelf);
  if (is_paint_invalidation_container &&
      (!parent_composited_layer ||
       !parent_composited_layer->EnclosingPaginationLayer())) {
    // |object_| establishes the top level composited layer under the
    // pagination layer.
    bool should_repeat_in_fragments = false;
    FragmentainerIterator iterator(
        ToLayoutFlowThread(enclosing_pagination_layer->GetLayoutObject()),
        BoundingBoxInPaginationContainer(object_, *enclosing_pagination_layer,
                                         should_repeat_in_fragments));
    DCHECK(!should_repeat_in_fragments);
    if (!iterator.AtEnd()) {
      first_fragment.SetPaginationOffset(
          ToLayoutPoint(iterator.PaginationOffset()));
      first_fragment.SetLogicalTopInFlowThread(
          iterator.FragmentainerLogicalTopInFlowThread());
    }
  } else if (parent_composited_layer) {
    // All objects under the composited layer use the same pagination offset.
    const auto& fragment =
        parent_composited_layer->GetLayoutObject().FirstFragment();
    first_fragment.SetPaginationOffset(fragment.PaginationOffset());
    first_fragment.SetLogicalTopInFlowThread(fragment.LogicalTopInFlowThread());
  }
}

void ObjectPaintPropertyTreeBuilder::UpdateRepeatingPaintOffsetAdjustment() {
  if (!context_.is_repeating_in_fragments)
    return;

  if (object_.IsTableSection()) {
    if (ToLayoutTableSection(object_).IsRepeatingHeaderGroup())
      UpdateRepeatingTableHeaderPaintOffsetAdjustment();
    else if (ToLayoutTableSection(object_).IsRepeatingFooterGroup())
      UpdateRepeatingTableFooterPaintOffsetAdjustment();
  }

  // Otherwise the object is a descendant of the object which initiated the
  // repeating. It just uses repeating_paint_offset_adjustment in its fragment
  // contexts inherited from the initiating object.
}

// TODO(wangxianzhu): For now this works for horizontal-bt writing mode only.
// Need to support vertical writing modes.
void ObjectPaintPropertyTreeBuilder::
    UpdateRepeatingTableHeaderPaintOffsetAdjustment() {
  const auto& section = ToLayoutTableSection(object_);
  DCHECK(section.IsRepeatingHeaderGroup());
  auto& flow_thread = ToLayoutFlowThread(
      context_.painting_layer->EnclosingPaginationLayer()->GetLayoutObject());
  // TODO(crbug.com/757947): This shouldn't be possible but happens to
  // column-spanners in nested multi-col contexts.
  if (!flow_thread.IsPageLogicalHeightKnown())
    return;

  auto original_offset_in_flow_thread =
      context_.repeating_bounding_box_in_flow_thread.Y();
  auto fragment_height =
      flow_thread.PageLogicalHeightForOffset(original_offset_in_flow_thread);
  auto original_offset_in_fragment =
      fragment_height -
      flow_thread.PageRemainingLogicalHeightForOffset(
          original_offset_in_flow_thread, LayoutBox::kAssociateWithLatterPage);
  // This is total height of repeating headers seen by the table - height of
  // this header (which is the lowest repeating header seen by this table.
  auto repeating_offset_in_fragment =
      section.Table()->RowOffsetFromRepeatingHeader() - section.LogicalHeight();

  // For a repeating table header, the original location (which may be in the
  // middle of the fragment) and repeated locations (which should be always,
  // together with repeating headers of outer tables, aligned to the top of
  // the fragments) may be different. Therefore, for fragments other than the
  // first, adjust by |alignment_offset|.
  auto adjustment = repeating_offset_in_fragment - original_offset_in_fragment;

  auto fragment_offset_in_flow_thread =
      original_offset_in_flow_thread - original_offset_in_fragment;
  for (size_t i = 0; i < context_.fragments.size(); ++i) {
    auto& fragment_context = context_.fragments[i];
    fragment_context.repeating_paint_offset_adjustment = LayoutSize();
    // Adjust paint offsets of repeatings (not including the original).
    if (i)
      fragment_context.repeating_paint_offset_adjustment.SetHeight(adjustment);

    // Calculate the adjustment for the repeating which will appear in the next
    // fragment.
    adjustment += fragment_height;
    // Calculate the offset of the next fragment in flow thread. It's used to
    // get the height of that fragment.
    fragment_offset_in_flow_thread += fragment_height;
    fragment_height =
        flow_thread.PageLogicalHeightForOffset(fragment_offset_in_flow_thread);
  }
}

void ObjectPaintPropertyTreeBuilder::
    UpdateRepeatingTableFooterPaintOffsetAdjustment() {
  const auto& section = ToLayoutTableSection(object_);
  DCHECK(section.IsRepeatingFooterGroup());
  auto& flow_thread = ToLayoutFlowThread(
      context_.painting_layer->EnclosingPaginationLayer()->GetLayoutObject());
  // TODO(crbug.com/757947): This shouldn't be possible but happens to
  // column-spanners in nested multi-col contexts.
  if (!flow_thread.IsPageLogicalHeightKnown())
    return;

  auto original_offset_in_flow_thread =
      context_.repeating_bounding_box_in_flow_thread.MaxY() -
      section.LogicalHeight();
  auto fragment_height =
      flow_thread.PageLogicalHeightForOffset(original_offset_in_flow_thread);
  auto original_offset_in_fragment =
      fragment_height -
      flow_thread.PageRemainingLogicalHeightForOffset(
          original_offset_in_flow_thread, LayoutBox::kAssociateWithLatterPage);

  const auto& table = *section.Table();
  // TODO(crbug.com/798153): This keeps the existing behavior of repeating
  // footer painting in TableSectionPainter. Should change both places when
  // tweaking border-spacing for repeating footers.
  auto repeating_offset_in_fragment = fragment_height -
                                      table.RowOffsetFromRepeatingFooter() -
                                      table.VBorderSpacing();
  // We should show the whole bottom border instead of half if the table
  // collapses borders.
  if (table.ShouldCollapseBorders())
    repeating_offset_in_fragment -= table.BorderBottom();

  // Similar to repeating header, this is to adjust the repeating footer from
  // its original location to the repeating location.
  auto adjustment = repeating_offset_in_fragment - original_offset_in_fragment;

  auto fragment_offset_in_flow_thread =
      original_offset_in_flow_thread - original_offset_in_fragment;
  for (auto i = context_.fragments.size(); i > 0; --i) {
    auto& fragment_context = context_.fragments[i - 1];
    fragment_context.repeating_paint_offset_adjustment = LayoutSize();
    // Adjust paint offsets of repeatings.
    if (i != context_.fragments.size())
      fragment_context.repeating_paint_offset_adjustment.SetHeight(adjustment);

    // Calculate the adjustment for the repeating which will appear in the
    // previous fragment.
    adjustment -= fragment_height;
    // Calculate the offset of the previous fragment in flow thread. It's used
    // to get the height of that fragment.
    fragment_offset_in_flow_thread -= fragment_height;
    fragment_height =
        flow_thread.PageLogicalHeightForOffset(fragment_offset_in_flow_thread);
  }
}

bool ObjectPaintPropertyTreeBuilder::NeedsFragmentation() const {
  return context_.painting_layer->ShouldFragmentCompositedBounds();
}

static LayoutUnit FragmentLogicalTopInParentFlowThread(
    const LayoutFlowThread& flow_thread,
    LayoutUnit logical_top_in_current_flow_thread) {
  const auto* parent_pagination_layer =
      flow_thread.Layer()->Parent()->EnclosingPaginationLayer();
  if (!parent_pagination_layer)
    return LayoutUnit();

  const auto* parent_flow_thread =
      &ToLayoutFlowThread(parent_pagination_layer->GetLayoutObject());

  LayoutPoint location(LayoutUnit(), logical_top_in_current_flow_thread);
  // TODO(crbug.com/467477): Should we flip for writing-mode? For now regardless
  // of flipping, fast/multicol/vertical-rl/nested-columns.html fails.
  if (!flow_thread.IsHorizontalWritingMode())
    location = location.TransposedPoint();

  // Convert into parent_flow_thread's coordinates.
  location = LayoutPoint(flow_thread.LocalToAncestorPoint(FloatPoint(location),
                                                          parent_flow_thread));
  if (!parent_flow_thread->IsHorizontalWritingMode())
    location = location.TransposedPoint();

  if (location.X() >= parent_flow_thread->LogicalWidth()) {
    // TODO(crbug.com/803649): We hit this condition for
    // external/wpt/css/css-multicol/multicol-height-block-child-001.xht.
    // The normal path would cause wrong FragmentClip hierarchy.
    // Return -1 to force standalone FragmentClip in the case.
    return LayoutUnit(-1);
  }

  // Return the logical top of the containing fragment in parent_flow_thread.
  return location.Y() +
         parent_flow_thread->PageRemainingLogicalHeightForOffset(
             location.Y(), LayoutBox::kAssociateWithLatterPage) -
         parent_flow_thread->PageLogicalHeightForOffset(location.Y());
}

// Find from parent contexts with matching |logical_top_in_flow_thread|, if any,
// to allow for correct property tree parenting of fragments.
PaintPropertyTreeBuilderFragmentContext
ObjectPaintPropertyTreeBuilder::ContextForFragment(
    const Optional<LayoutRect>& fragment_clip,
    LayoutUnit logical_top_in_flow_thread) const {
  const auto& parent_fragments = context_.fragments;
  if (parent_fragments.IsEmpty())
    return PaintPropertyTreeBuilderFragmentContext();

  // This will be used in the loop finding matching fragment from ancestor flow
  // threads after no matching from parent_fragments.
  LayoutUnit logical_top_in_containing_flow_thread;

  if (object_.IsLayoutFlowThread()) {
    const auto& flow_thread = ToLayoutFlowThread(object_);
    // If this flow thread is under another flow thread, find the fragment in
    // the parent flow thread containing this fragment. Otherwise, the following
    // code will just match parent_contexts[0].
    logical_top_in_containing_flow_thread =
        FragmentLogicalTopInParentFlowThread(flow_thread,
                                             logical_top_in_flow_thread);
    for (const auto& parent_context : parent_fragments) {
      if (logical_top_in_containing_flow_thread ==
          parent_context.logical_top_in_flow_thread) {
        auto context = parent_context;
        context.fragment_clip = fragment_clip;
        context.logical_top_in_flow_thread = logical_top_in_flow_thread;
        return context;
      }
    }
  } else {
    bool parent_is_under_same_flow_thread;
    auto pagination_layer = context_.painting_layer->EnclosingPaginationLayer();
    if (object_.IsColumnSpanAll()) {
      parent_is_under_same_flow_thread = false;
    } else if (object_.IsOutOfFlowPositioned()) {
      parent_is_under_same_flow_thread =
          (object_.Parent()->PaintingLayer()->EnclosingPaginationLayer() ==
           pagination_layer);
    } else {
      parent_is_under_same_flow_thread = true;
    }

    // Match against parent_fragments if the fragment and parent_fragments are
    // under the same flow thread.
    if (parent_is_under_same_flow_thread) {
      DCHECK(object_.Parent()->PaintingLayer()->EnclosingPaginationLayer() ==
             pagination_layer);
      for (const auto& parent_context : parent_fragments) {
        if (logical_top_in_flow_thread ==
            parent_context.logical_top_in_flow_thread) {
          auto context = parent_context;
          // The context inherits fragment clip from parent so we don't need
          // to issue fragment clip again.
          context.fragment_clip = WTF::nullopt;
          return context;
        }
      }
    }

    logical_top_in_containing_flow_thread = logical_top_in_flow_thread;
  }

  // Found no matching parent fragment. Use parent_fragments[0] to inherit
  // transforms and effects from ancestors, and adjust the clip state.
  // TODO(crbug.com/803649): parent_fragments[0] is not always correct because
  // some ancestor transform/effect may be missing in the fragment if the
  // ancestor doesn't intersect with the first fragment of the flow thread.
  auto context = parent_fragments[0];
  context.logical_top_in_flow_thread = logical_top_in_flow_thread;
  context.fragment_clip = fragment_clip;

  // We reach here because of the following reasons:
  // 1. the parent doesn't have the corresponding fragment because the fragment
  //    overflows the parent;
  // 2. the fragment and parent_fragments are not under the same flow thread
  //    (e.g. column-span:all or some out-of-flow positioned).
  // For each case, we need to adjust context.current.clip. For now it's the
  // first parent fragment's FragmentClip which is not the correct clip for
  // object_.
  for (const auto* container = object_.Container(); container;
       container = container->Container()) {
    if (!container->FirstFragment().HasLocalBorderBoxProperties())
      continue;

    for (const auto* fragment = &container->FirstFragment(); fragment;
         fragment = fragment->NextFragment()) {
      if (fragment->LogicalTopInFlowThread() ==
          logical_top_in_containing_flow_thread) {
        // Found a matching fragment in an ancestor container. Use the
        // container's content clip as the clip state.
        DCHECK(fragment->PostOverflowClip());
        context.current.clip = fragment->PostOverflowClip();
        return context;
      }
    }

    if (container->IsLayoutFlowThread()) {
      logical_top_in_containing_flow_thread =
          FragmentLogicalTopInParentFlowThread(
              ToLayoutFlowThread(*container),
              logical_top_in_containing_flow_thread);
    }
  }

  // We should always find a matching ancestor fragment in the above loop
  // because logical_top_in_containing_flow_thread will be zero when we traverse
  // across the top-level flow thread and it should match the first fragment of
  // a non-fragmented ancestor container.
  NOTREACHED();
  return context;
}

void ObjectPaintPropertyTreeBuilder::CreateFragmentContexts(
    bool needs_paint_properties) {
  // We need at least the fragments for all fragmented objects, which store
  // their local border box properties and paint invalidation data (such
  // as paint offset and visual rect) on each fragment.
  PaintLayer* paint_layer = context_.painting_layer;
  PaintLayer* enclosing_pagination_layer =
      paint_layer->EnclosingPaginationLayer();

  const auto& flow_thread =
      ToLayoutFlowThread(enclosing_pagination_layer->GetLayoutObject());
  LayoutRect object_bounding_box_in_flow_thread;
  if (context_.is_repeating_in_fragments) {
    // The object is a descendant of a repeating object. It should use the
    // repeating bounding box to repeat in the same fragments as its
    // repeating ancestor.
    object_bounding_box_in_flow_thread =
        context_.repeating_bounding_box_in_flow_thread;
  } else {
    bool should_repeat_in_fragments = false;
    object_bounding_box_in_flow_thread = BoundingBoxInPaginationContainer(
        object_, *enclosing_pagination_layer, should_repeat_in_fragments);
    if (should_repeat_in_fragments) {
      context_.is_repeating_in_fragments = true;
      context_.repeating_bounding_box_in_flow_thread =
          object_bounding_box_in_flow_thread;
    }
  }

  FragmentData* current_fragment_data = nullptr;
  FragmentainerIterator iterator(flow_thread,
                                 object_bounding_box_in_flow_thread);
  bool fragments_changed = false;
  Vector<PaintPropertyTreeBuilderFragmentContext, 1> new_fragment_contexts;
  for (; !iterator.AtEnd(); iterator.Advance()) {
    auto pagination_offset = ToLayoutPoint(iterator.PaginationOffset());
    auto logical_top_in_flow_thread =
        iterator.FragmentainerLogicalTopInFlowThread();
    Optional<LayoutRect> fragment_clip;

    if (object_.HasLayer()) {
      // 1. Compute clip in flow thread space.
      fragment_clip = iterator.ClipRectInFlowThread();
      // 2. Convert #1 to visual coordinates in the space of the flow thread.
      fragment_clip->MoveBy(pagination_offset);
      // 3. Adjust #2 to visual coordinates in the containing "paint offset"
      // space.
      {
        DCHECK(context_.fragments[0].current.paint_offset_root);
        LayoutPoint pagination_visual_offset = VisualOffsetFromPaintOffsetRoot(
            context_.fragments[0], enclosing_pagination_layer);
        // Adjust for paint offset of the root, which may have a subpixel
        // component. The paint offset root never has more than one fragment.
        pagination_visual_offset.MoveBy(
            context_.fragments[0]
                .current.paint_offset_root->FirstFragment()
                .PaintOffset());
        fragment_clip->MoveBy(pagination_visual_offset);
      }
    }

    // Match to parent fragments from the same containing flow thread.
    new_fragment_contexts.push_back(
        ContextForFragment(fragment_clip, logical_top_in_flow_thread));

    Optional<LayoutUnit> old_logical_top_in_flow_thread;
    if (current_fragment_data) {
      if (const auto* old_fragment = current_fragment_data->NextFragment())
        old_logical_top_in_flow_thread = old_fragment->LogicalTopInFlowThread();
      current_fragment_data = &current_fragment_data->EnsureNextFragment();
    } else {
      current_fragment_data = &object_.GetMutableForPainting().FirstFragment();
      old_logical_top_in_flow_thread =
          current_fragment_data->LogicalTopInFlowThread();
    }

    if (!old_logical_top_in_flow_thread ||
        *old_logical_top_in_flow_thread != logical_top_in_flow_thread)
      fragments_changed = true;

    InitFragmentPaintProperties(
        *current_fragment_data,
        needs_paint_properties || new_fragment_contexts.back().fragment_clip,
        pagination_offset, logical_top_in_flow_thread);
  }

  if (!current_fragment_data) {
    // This will be an empty fragment - get rid of it?
    InitSingleFragmentFromParent(needs_paint_properties);
  } else {
    if (current_fragment_data->NextFragment())
      fragments_changed = true;
    current_fragment_data->ClearNextFragment();
    context_.fragments = std::move(new_fragment_contexts);
  }

  // Need to update subtree paint properties for the changed fragments.
  if (fragments_changed)
    object_.GetMutableForPainting().SetSubtreeNeedsPaintPropertyUpdate();
}

bool ObjectPaintPropertyTreeBuilder::UpdateFragments() {
  bool had_paint_properties = object_.FirstFragment().PaintProperties();
  // Note: It is important to short-circuit on object_.StyleRef().ClipPath()
  // because NeedsClipPathClip() and NeedsEffect() requires the clip path
  // cache to be resolved, but the clip path cache invalidation must delayed
  // until the paint offset and border box has been computed.
  bool needs_paint_properties =
      object_.StyleRef().ClipPath() || NeedsPaintOffsetTranslation(object_) ||
      NeedsTransform(object_) || NeedsClipPathClip(object_) ||
      NeedsEffect(object_) || NeedsTransformForNonRootSVG(object_) ||
      NeedsFilter(object_) || NeedsCssClip(object_) ||
      NeedsInnerBorderRadiusClip(object_) || NeedsOverflowClip(object_) ||
      NeedsPerspective(object_) || NeedsSVGLocalToBorderBoxTransform(object_) ||
      NeedsScrollOrScrollTranslation(object_);
  // Need of fragmentation clip will be determined in CreateFragmentContexts().

  if (!NeedsFragmentation()) {
    InitSingleFragmentFromParent(needs_paint_properties);
    UpdateCompositedLayerPaginationOffset();
    context_.is_repeating_in_fragments = false;
  } else {
    CreateFragmentContexts(needs_paint_properties);
  }

  if (object_.IsSVGHiddenContainer()) {
    // SVG resources are painted within one or more other locations in the
    // SVG during paint, and hence have their own independent paint property
    // trees, paint offset, etc.
    context_.fragments.clear();
    context_.fragments.Grow(1);
    context_.has_svg_hidden_container_ancestor = true;
    PaintPropertyTreeBuilderFragmentContext& fragment_context =
        context_.fragments[0];

    fragment_context.current.paint_offset_root =
        fragment_context.absolute_position.paint_offset_root =
            fragment_context.fixed_position.paint_offset_root = &object_;

    object_.GetMutableForPainting().FirstFragment().ClearNextFragment();
  }

  if (object_.HasLayer()) {
    ToLayoutBoxModelObject(object_).Layer()->SetIsUnderSVGHiddenContainer(
        context_.has_svg_hidden_container_ancestor);
  }

  UpdateRepeatingPaintOffsetAdjustment();

  return needs_paint_properties != had_paint_properties;
}

bool ObjectPaintPropertyTreeBuilder::ObjectTypeMightNeedPaintProperties()
    const {
  return object_.IsBoxModelObject() || object_.IsSVG() ||
         context_.painting_layer->EnclosingPaginationLayer();
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

bool ObjectPaintPropertyTreeBuilder::UpdateForSelf() {
  UpdatePaintingLayer();

  bool property_added_or_removed = false;
  if (ObjectTypeMightNeedPaintProperties())
    property_added_or_removed = UpdateFragments();
  else
    object_.GetMutableForPainting().FirstFragment().ClearNextFragment();

  bool property_changed = false;
  auto* fragment_data = &object_.GetMutableForPainting().FirstFragment();
  for (auto& fragment_context : context_.fragments) {
    FragmentPaintPropertyTreeBuilder builder(object_, context_,
                                             fragment_context, *fragment_data);
    builder.UpdateForSelf();
    property_changed |= builder.PropertyChanged();
    property_added_or_removed |= builder.PropertyAddedOrRemoved();
    fragment_data = fragment_data->NextFragment();
  }
  DCHECK(!fragment_data);

  // We need to update property tree states of paint chunks.
  if (property_added_or_removed &&
      RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    context_.painting_layer->SetNeedsRepaint();

  return property_changed;
}

bool ObjectPaintPropertyTreeBuilder::UpdateForChildren() {
  if (!ObjectTypeMightNeedPaintProperties())
    return false;

  bool property_changed = false;
  bool property_added_or_removed = false;
  auto* fragment_data = &object_.GetMutableForPainting().FirstFragment();
  for (auto& fragment_context : context_.fragments) {
    FragmentPaintPropertyTreeBuilder builder(object_, context_,
                                             fragment_context, *fragment_data);
    builder.UpdateForChildren();
    property_changed |= builder.PropertyChanged();
    property_added_or_removed |= builder.PropertyAddedOrRemoved();
    context_.force_subtree_update |= object_.SubtreeNeedsPaintPropertyUpdate();
    fragment_data = fragment_data->NextFragment();
  }
  DCHECK(!fragment_data);

  if (object_.CanContainAbsolutePositionObjects())
    context_.container_for_absolute_position = &object_;
  if (object_.CanContainFixedPositionObjects())
    context_.container_for_fixed_position = &object_;

  // We need to update property tree states of paint chunks.
  if (property_added_or_removed &&
      RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    context_.painting_layer->SetNeedsRepaint();

  return property_changed;
}

}  // namespace blink
