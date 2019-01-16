/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_geometry_map.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/scroll/main_thread_scrolling_reason.h"
#include "third_party/blink/renderer/platform/transforms/transform_state.h"

namespace blink {

namespace {
inline bool IsOutOfFlowPositionedWithImplicitHeight(
    const LayoutBoxModelObject* child) {
  return child->IsOutOfFlowPositioned() &&
         !child->StyleRef().LogicalTop().IsAuto() &&
         !child->StyleRef().LogicalBottom().IsAuto();
}

// Inclusive of |from|, exclusive of |to|.
PaintLayer* FindFirstStickyBetween(LayoutObject* from, LayoutObject* to) {
  LayoutObject* maybe_sticky_ancestor = from;
  while (maybe_sticky_ancestor && maybe_sticky_ancestor != to) {
    if (maybe_sticky_ancestor->StyleRef().HasStickyConstrainedPosition()) {
      return ToLayoutBoxModelObject(maybe_sticky_ancestor)->Layer();
    }

    maybe_sticky_ancestor =
        maybe_sticky_ancestor->IsLayoutInline()
            ? maybe_sticky_ancestor->Container()
            : ToLayoutBox(maybe_sticky_ancestor)->LocationContainer();
  }
  return nullptr;
}
}  // namespace

// The HashMap for storing continuation pointers.
// The continuation chain is a singly linked list. As such, the HashMap's value
// is the next pointer associated with the key.
typedef HashMap<const LayoutBoxModelObject*, LayoutBoxModelObject*>
    ContinuationMap;
static ContinuationMap* g_continuation_map = nullptr;

void LayoutBoxModelObject::ContentChanged(ContentChangeType change_type) {
  if (!HasLayer())
    return;

  Layer()->ContentChanged(change_type);
}

bool LayoutBoxModelObject::HasAcceleratedCompositing() const {
  return View()->Compositor()->HasAcceleratedCompositing();
}

LayoutBoxModelObject::LayoutBoxModelObject(ContainerNode* node)
    : LayoutObject(node) {}

bool LayoutBoxModelObject::UsesCompositedScrolling() const {
  return HasOverflowClip() && HasLayer() &&
         Layer()->GetScrollableArea()->UsesCompositedScrolling();
}

BackgroundPaintLocation LayoutBoxModelObject::GetBackgroundPaintLocation(
    uint32_t* main_thread_scrolling_reasons) const {
  bool may_have_scrolling_layers_without_scrolling = IsLayoutView();
  const auto* scrollable_area = GetScrollableArea();
  bool scrolls_overflow = scrollable_area && scrollable_area->ScrollsOverflow();
  if (!scrolls_overflow && !may_have_scrolling_layers_without_scrolling)
    return kBackgroundPaintInGraphicsLayer;

  // If we care about LCD text, paint root backgrounds into scrolling contents
  // layer even if style suggests otherwise. (For non-root scrollers, we just
  // avoid compositing - see PLSA::ComputeNeedsCompositedScrolling.)
  if (IsLayoutView()) {
    DCHECK(Layer()->Compositor());
    if (!Layer()->Compositor()->PreferCompositingToLCDTextEnabled())
      return kBackgroundPaintInScrollingContents;
  }

  // TODO(flackr): Detect opaque custom scrollbars which would cover up a
  // border-box background.
  bool has_custom_scrollbars =
      scrollable_area &&
      ((scrollable_area->HorizontalScrollbar() &&
        scrollable_area->HorizontalScrollbar()->IsCustomScrollbar()) ||
       (scrollable_area->VerticalScrollbar() &&
        scrollable_area->VerticalScrollbar()->IsCustomScrollbar()));

  // TODO(flackr): When we correctly clip the scrolling contents layer we can
  // paint locally equivalent backgrounds into it. https://crbug.com/645957
  if (HasClip())
    return kBackgroundPaintInGraphicsLayer;

  // TODO(flackr): Remove this when box shadows are still painted correctly when
  // painting into the composited scrolling contents layer.
  // https://crbug.com/646464
  if (StyleRef().BoxShadow()) {
    if (main_thread_scrolling_reasons) {
      *main_thread_scrolling_reasons |=
          MainThreadScrollingReason::kHasBoxShadowFromNonRootLayer;
    }
    return kBackgroundPaintInGraphicsLayer;
  }

  // Assume optimistically that the background can be painted in the scrolling
  // contents until we find otherwise.
  BackgroundPaintLocation paint_location = kBackgroundPaintInScrollingContents;
  const FillLayer* layer = &(StyleRef().BackgroundLayers());
  for (; layer; layer = layer->Next()) {
    if (layer->Attachment() == EFillAttachment::kLocal)
      continue;

    // Solid color layers with an effective background clip of the padding box
    // can be treated as local.
    if (!layer->GetImage() && !layer->Next() &&
        ResolveColor(GetCSSPropertyBackgroundColor()).Alpha() > 0) {
      EFillBox clip = layer->Clip();
      if (clip == EFillBox::kPadding)
        continue;
      // A border box can be treated as a padding box if the border is opaque or
      // there is no border and we don't have custom scrollbars.
      if (clip == EFillBox::kBorder) {
        if (!has_custom_scrollbars &&
            (StyleRef().BorderTopWidth() == 0 ||
             (!ResolveColor(GetCSSPropertyBorderTopColor()).HasAlpha() &&
              StyleRef().BorderTopStyle() == EBorderStyle::kSolid)) &&
            (StyleRef().BorderLeftWidth() == 0 ||
             (!ResolveColor(GetCSSPropertyBorderLeftColor()).HasAlpha() &&
              StyleRef().BorderLeftStyle() == EBorderStyle::kSolid)) &&
            (StyleRef().BorderRightWidth() == 0 ||
             (!ResolveColor(GetCSSPropertyBorderRightColor()).HasAlpha() &&
              StyleRef().BorderRightStyle() == EBorderStyle::kSolid)) &&
            (StyleRef().BorderBottomWidth() == 0 ||
             (!ResolveColor(GetCSSPropertyBorderBottomColor()).HasAlpha() &&
              StyleRef().BorderBottomStyle() == EBorderStyle::kSolid))) {
          continue;
        }
        // If we have an opaque background color only, we can safely paint it
        // into both the scrolling contents layer and the graphics layer to
        // preserve LCD text.
        if (layer == (&StyleRef().BackgroundLayers()) &&
            ResolveColor(GetCSSPropertyBackgroundColor()).Alpha() < 255)
          return kBackgroundPaintInGraphicsLayer;
        paint_location |= kBackgroundPaintInGraphicsLayer;
        continue;
      }
      // A content fill box can be treated as a padding fill box if there is no
      // padding.
      if (clip == EFillBox::kContent && StyleRef().PaddingTop().IsZero() &&
          StyleRef().PaddingLeft().IsZero() &&
          StyleRef().PaddingRight().IsZero() &&
          StyleRef().PaddingBottom().IsZero()) {
        continue;
      }
    }
    return kBackgroundPaintInGraphicsLayer;
  }
  return paint_location;
}

LayoutBoxModelObject::~LayoutBoxModelObject() {
  // Our layer should have been destroyed and cleared by now
  DCHECK(!HasLayer());
  DCHECK(!Layer());
}

void LayoutBoxModelObject::WillBeDestroyed() {
  // A continuation of this LayoutObject should be destroyed at subclasses.
  DCHECK(!Continuation());

  if (IsPositioned()) {
    // Don't use view() because the document's layoutView has been set to
    // 0 during destruction.
    if (LocalFrame* frame = GetFrame()) {
      if (LocalFrameView* frame_view = frame->View()) {
        if (StyleRef().HasViewportConstrainedPosition() ||
            StyleRef().HasStickyConstrainedPosition())
          frame_view->RemoveViewportConstrainedObject(*this);
      }
    }
  }

  LayoutObject::WillBeDestroyed();

  if (HasLayer())
    DestroyLayer();
}

void LayoutBoxModelObject::StyleWillChange(StyleDifference diff,
                                           const ComputedStyle& new_style) {
  // SPv1:
  // This object's layer may begin or cease to be stacked or stacking context,
  // in which case the paint invalidation container of this object and
  // descendants may change. Thus we need to invalidate paint eagerly for all
  // such children. PaintLayerCompositor::paintInvalidationOnCompositingChange()
  // doesn't work for the case because we can only see the new
  // paintInvalidationContainer during compositing update.
  // SPv1 and v2:
  // Change of stacked/stacking context status may cause change of this or
  // descendant PaintLayer's CompositingContainer, so we need to eagerly
  // invalidate the current compositing container chain which may have painted
  // cached subsequences containing this object or descendant objects.
  if (Style() &&
      (StyleRef().IsStacked() != new_style.IsStacked() ||
       StyleRef().IsStackingContext() != new_style.IsStackingContext()) &&
      // ObjectPaintInvalidator requires this.
      IsRooted()) {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      ObjectPaintInvalidator(*this).SlowSetPaintingLayerNeedsRepaint();
    } else {
      // We need to invalidate based on the current compositing status.
      DisableCompositingQueryAsserts compositing_disabler;
      ObjectPaintInvalidator(*this)
          .InvalidatePaintIncludingNonCompositingDescendants();
    }
  }

  if (HasLayer() && diff.CssClipChanged())
    Layer()->ClearClipRects();

  LayoutObject::StyleWillChange(diff, new_style);
}

DISABLE_CFI_PERF
void LayoutBoxModelObject::StyleDidChange(StyleDifference diff,
                                          const ComputedStyle* old_style) {
  bool had_transform_related_property = HasTransformRelatedProperty();
  bool had_layer = HasLayer();
  bool layer_was_self_painting = had_layer && Layer()->IsSelfPaintingLayer();
  bool was_horizontal_writing_mode = IsHorizontalWritingMode();

  LayoutObject::StyleDidChange(diff, old_style);
  UpdateFromStyle();

  // When an out-of-flow-positioned element changes its display between block
  // and inline-block, then an incremental layout on the element's containing
  // block lays out the element through LayoutPositionedObjects, which skips
  // laying out the element's parent.
  // The element's parent needs to relayout so that it calls LayoutBlockFlow::
  // setStaticInlinePositionForChild with the out-of-flow-positioned child, so
  // that when it's laid out, its LayoutBox::computePositionedLogicalWidth/
  // Height takes into account its new inline/block position rather than its old
  // block/inline position.
  // Position changes and other types of display changes are handled elsewhere.
  if (old_style && IsOutOfFlowPositioned() && Parent() &&
      (Parent() != ContainingBlock()) &&
      (StyleRef().GetPosition() == old_style->GetPosition()) &&
      (StyleRef().OriginalDisplay() != old_style->OriginalDisplay()) &&
      ((StyleRef().OriginalDisplay() == EDisplay::kBlock) ||
       (StyleRef().OriginalDisplay() == EDisplay::kInlineBlock)) &&
      ((old_style->OriginalDisplay() == EDisplay::kBlock) ||
       (old_style->OriginalDisplay() == EDisplay::kInlineBlock)))
    Parent()->SetNeedsLayout(layout_invalidation_reason::kChildChanged,
                             kMarkContainerChain);

  PaintLayerType type = LayerTypeRequired();
  if (type != kNoPaintLayer) {
    if (!Layer()) {
      // In order to update this object properly, we need to lay it out again.
      // However, if we have never laid it out, don't mark it for layout. If
      // this is a new object, it may not yet have been inserted into the tree,
      // and if we mark it for layout then, we risk upsetting the tree
      // insertion machinery.
      if (EverHadLayout())
        SetChildNeedsLayout();

      CreateLayerAfterStyleChange();
    }
  } else if (Layer() && Layer()->Parent()) {
    PaintLayer* parent_layer = Layer()->Parent();
    // Either a transform wasn't specified or the object doesn't support
    // transforms, so just null out the bit.
    SetHasTransformRelatedProperty(false);
    SetHasReflection(false);
    Layer()->UpdateFilters(old_style, StyleRef());
    Layer()->UpdateClipPath(old_style, StyleRef());
    // Calls DestroyLayer() which clears the layer.
    Layer()->RemoveOnlyThisLayerAfterStyleChange(old_style);
    if (EverHadLayout())
      SetChildNeedsLayout();
    if (had_transform_related_property) {
      SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          layout_invalidation_reason::kStyleChange);
    }
    if (!NeedsLayout()) {
      // FIXME: We should call a specialized version of this function.
      parent_layer->UpdateLayerPositionsAfterLayout();
    }
  }

  if (old_style &&
      (old_style->CanContainFixedPositionObjects(IsDocumentElement()) !=
           StyleRef().CanContainFixedPositionObjects(IsDocumentElement()) ||
       old_style->CanContainAbsolutePositionObjects() !=
           StyleRef().CanContainAbsolutePositionObjects())) {
    // If out of flow element containment changed, then we need to force a
    // subtree paint property update, since the children elements may now be
    // referencing a different container.
    AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kContainerChainMayChange);
  } else if (had_layer == HasLayer() &&
             had_transform_related_property != HasTransformRelatedProperty()) {
    // This affects whether to create transform node. Note that if the
    // HasLayer() value changed, then all of this was already set in
    // CreateLayerAfterStyleChange() or DestroyLayer().
    SetNeedsPaintPropertyUpdate();
    if (Layer())
      Layer()->SetNeedsCompositingInputsUpdate();
  }

  if (Layer()) {
    Layer()->StyleDidChange(diff, old_style);
    if (had_layer && Layer()->IsSelfPaintingLayer() != layer_was_self_painting)
      SetChildNeedsLayout();
  }

  if (old_style && was_horizontal_writing_mode != IsHorizontalWritingMode()) {
    // Changing the getWritingMode() may change isOrthogonalWritingModeRoot()
    // of children. Make sure all children are marked/unmarked as orthogonal
    // writing-mode roots.
    bool new_horizontal_writing_mode = IsHorizontalWritingMode();
    for (LayoutObject* child = SlowFirstChild(); child;
         child = child->NextSibling()) {
      if (!child->IsBox())
        continue;
      if (new_horizontal_writing_mode != child->IsHorizontalWritingMode())
        ToLayoutBox(child)->MarkOrthogonalWritingModeRoot();
      else
        ToLayoutBox(child)->UnmarkOrthogonalWritingModeRoot();
    }
  }

  // The used style for body background may change due to computed style change
  // on the document element because of change of BackgroundTransfersToView()
  // which depends on the document element style.
  if (IsDocumentElement()) {
    if (HTMLBodyElement* body = GetDocument().FirstBodyElement()) {
      if (auto* body_object = body->GetLayoutObject()) {
        if (body_object->IsBoxModelObject()) {
          auto* body_box_model = ToLayoutBoxModelObject(body_object);
          bool new_body_background_transfers =
              body_box_model->BackgroundTransfersToView(Style());
          bool old_body_background_transfers =
              old_style && body_box_model->BackgroundTransfersToView(old_style);
          if (new_body_background_transfers != old_body_background_transfers &&
              body_object->Style() && body_object->StyleRef().HasBackground())
            body_object->SetBackgroundNeedsFullPaintInvalidation();
        }
      }
    }
  }

  if (LocalFrameView* frame_view = View()->GetFrameView()) {
    bool new_style_is_viewport_constained =
        StyleRef().GetPosition() == EPosition::kFixed;
    bool old_style_is_viewport_constrained =
        old_style && old_style->GetPosition() == EPosition::kFixed;
    bool new_style_is_sticky = StyleRef().HasStickyConstrainedPosition();
    bool old_style_is_sticky =
        old_style && old_style->HasStickyConstrainedPosition();

    if (new_style_is_sticky != old_style_is_sticky) {
      if (new_style_is_sticky) {
        // During compositing inputs update we'll have the scroll ancestor
        // without having to walk up the tree and can compute the sticky
        // position constraints then.
        if (Layer())
          Layer()->SetNeedsCompositingInputsUpdate();

        // TODO(pdr): When CompositeAfterPaint is enabled, we will need to
        // invalidate the scroll paint property subtree for this so main thread
        // scroll reasons are recomputed.
      } else {
        // This may get re-added to viewport constrained objects if the object
        // went from sticky to fixed.
        frame_view->RemoveViewportConstrainedObject(*this);

        // Remove sticky constraints for this layer.
        if (Layer()) {
          DisableCompositingQueryAsserts disabler;
          if (const PaintLayer* ancestor_overflow_layer =
                  Layer()->AncestorOverflowLayer()) {
            if (PaintLayerScrollableArea* scrollable_area =
                    ancestor_overflow_layer->GetScrollableArea())
              scrollable_area->InvalidateStickyConstraintsFor(Layer());
          }
        }

        // TODO(pdr): When CompositeAfterPaint is enabled, we will need to
        // invalidate the scroll paint property subtree for this so main thread
        // scroll reasons are recomputed.
      }
    }

    if (new_style_is_viewport_constained != old_style_is_viewport_constrained) {
      if (new_style_is_viewport_constained && Layer())
        frame_view->AddViewportConstrainedObject(*this);
      else
        frame_view->RemoveViewportConstrainedObject(*this);
    }
  }

  if (old_style && HasLayer() && !Layer()->NeedsRepaint()) {
    if (old_style->BackfaceVisibility() != StyleRef().BackfaceVisibility()) {
      // We need to repaint the layer to update the backface visibility value of
      // the paint chunk.
      Layer()->SetNeedsRepaint();
    } else if (diff.TransformChanged() &&
               (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
                !Layer()->HasStyleDeterminedDirectCompositingReasons())) {
      // PaintLayerPainter::PaintLayerWithAdjustedRoot skips painting of a layer
      // whose transform is not invertible, so we need to repaint the layer when
      // invertible status changes.
      TransformationMatrix old_transform;
      TransformationMatrix new_transform;
      old_style->ApplyTransform(
          old_transform, LayoutSize(), ComputedStyle::kExcludeTransformOrigin,
          ComputedStyle::kExcludeMotionPath,
          ComputedStyle::kIncludeIndependentTransformProperties);
      StyleRef().ApplyTransform(
          new_transform, LayoutSize(), ComputedStyle::kExcludeTransformOrigin,
          ComputedStyle::kExcludeMotionPath,
          ComputedStyle::kIncludeIndependentTransformProperties);
      if (old_transform.IsInvertible() != new_transform.IsInvertible())
        Layer()->SetNeedsRepaint();
    }
  }
}

void LayoutBoxModelObject::InvalidateStickyConstraints() {
  PaintLayer* enclosing = EnclosingLayer();

  if (PaintLayerScrollableArea* scrollable_area =
          enclosing->GetScrollableArea()) {
    scrollable_area->InvalidateAllStickyConstraints();
    // If this object doesn't have a layer and its enclosing layer is a scroller
    // then we don't need to invalidate the sticky constraints on the ancestor
    // scroller because the enclosing scroller won't have changed size.
    if (!Layer())
      return;
  }

  // This intentionally uses the stale ancestor overflow layer compositing input
  // as if we have saved constraints for this layer they were saved in the
  // previous frame.
  DisableCompositingQueryAsserts disabler;
  if (const PaintLayer* ancestor_overflow_layer =
          enclosing->AncestorOverflowLayer()) {
    if (PaintLayerScrollableArea* ancestor_scrollable_area =
            ancestor_overflow_layer->GetScrollableArea())
      ancestor_scrollable_area->InvalidateAllStickyConstraints();
  }
}

void LayoutBoxModelObject::CreateLayerAfterStyleChange() {
  DCHECK(!HasLayer() && !Layer());
  GetMutableForPainting().FirstFragment().SetLayer(
      std::make_unique<PaintLayer>(*this));
  SetHasLayer(true);
  Layer()->InsertOnlyThisLayerAfterStyleChange();
  // Creating a layer may affect existence of the LocalBorderBoxProperties, so
  // we need to ensure that we update paint properties.
  SetNeedsPaintPropertyUpdate();
}

void LayoutBoxModelObject::DestroyLayer() {
  DCHECK(HasLayer() && Layer());
  SetHasLayer(false);
  GetMutableForPainting().FirstFragment().SetLayer(nullptr);
  // Removing a layer may affect existence of the LocalBorderBoxProperties, so
  // we need to ensure that we update paint properties.
  SetNeedsPaintPropertyUpdate();
}

bool LayoutBoxModelObject::HasSelfPaintingLayer() const {
  return Layer() && Layer()->IsSelfPaintingLayer();
}

PaintLayerScrollableArea* LayoutBoxModelObject::GetScrollableArea() const {
  return Layer() ? Layer()->GetScrollableArea() : nullptr;
}

void LayoutBoxModelObject::AddLayerHitTestRects(
    LayerHitTestRects& rects,
    const PaintLayer* current_layer,
    const LayoutPoint& layer_offset,
    TouchAction supported_fast_actions,
    const LayoutRect& container_rect,
    TouchAction container_whitelisted_touch_action) const {
  if (HasLayer()) {
    if (IsLayoutView()) {
      // LayoutView is handled with a special fast-path, but it needs to know
      // the current layer.
      LayoutObject::AddLayerHitTestRects(rects, Layer(), LayoutPoint(),
                                         supported_fast_actions, LayoutRect(),
                                         TouchAction::kTouchActionAuto);
    } else {
      // Since a LayoutObject never lives outside it's container Layer, we can
      // switch to marking entire layers instead. This may sometimes mark more
      // than necessary (when a layer is made of disjoint objects) but in
      // practice is a significant performance savings.
      Layer()->AddLayerHitTestRects(rects, supported_fast_actions);
    }
  } else {
    LayoutObject::AddLayerHitTestRects(rects, current_layer, layer_offset,
                                       supported_fast_actions, container_rect,
                                       container_whitelisted_touch_action);
  }
}

void LayoutBoxModelObject::AddOutlineRectsForNormalChildren(
    Vector<LayoutRect>& rects,
    const LayoutPoint& additional_offset,
    NGOutlineType include_block_overflows) const {
  for (LayoutObject* child = SlowFirstChild(); child;
       child = child->NextSibling()) {
    // Outlines of out-of-flow positioned descendants are handled in
    // LayoutBlock::addOutlineRects().
    if (child->IsOutOfFlowPositioned())
      continue;

    // Outline of an element continuation or anonymous block continuation is
    // added when we iterate the continuation chain.
    // See LayoutBlock::addOutlineRects() and LayoutInline::addOutlineRects().
    if (child->IsElementContinuation() ||
        (child->IsLayoutBlockFlow() &&
         ToLayoutBlockFlow(child)->IsAnonymousBlockContinuation()))
      continue;

    AddOutlineRectsForDescendant(*child, rects, additional_offset,
                                 include_block_overflows);
  }
}

void LayoutBoxModelObject::AddOutlineRectsForDescendant(
    const LayoutObject& descendant,
    Vector<LayoutRect>& rects,
    const LayoutPoint& additional_offset,
    NGOutlineType include_block_overflows) const {
  if (descendant.IsText() || descendant.IsListMarker())
    return;

  if (descendant.HasLayer()) {
    Vector<LayoutRect> layer_outline_rects;
    descendant.AddOutlineRects(layer_outline_rects, LayoutPoint(),
                               include_block_overflows);
    descendant.LocalToAncestorRects(layer_outline_rects, this, LayoutPoint(),
                                    additional_offset);
    rects.AppendVector(layer_outline_rects);
    return;
  }

  if (descendant.IsBox()) {
    descendant.AddOutlineRects(
        rects, additional_offset + ToLayoutBox(descendant).LocationOffset(),
        include_block_overflows);
    return;
  }

  if (descendant.IsLayoutInline()) {
    // As an optimization, an ancestor has added rects for its line boxes
    // covering descendants' line boxes, so descendants don't need to add line
    // boxes again. For example, if the parent is a LayoutBlock, it adds rects
    // for its RootOutlineBoxes which cover the line boxes of this LayoutInline.
    // So the LayoutInline needs to add rects for children and continuations
    // only.
    ToLayoutInline(descendant)
        .AddOutlineRectsForChildrenAndContinuations(rects, additional_offset,
                                                    include_block_overflows);
    return;
  }

  descendant.AddOutlineRects(rects, additional_offset, include_block_overflows);
}

bool LayoutBoxModelObject::HasNonEmptyLayoutSize() const {
  for (const LayoutBoxModelObject* root = this; root;
       root = root->Continuation()) {
    for (const LayoutObject* object = root; object;
         object = object->NextInPreOrder(root)) {
      if (object->IsBox()) {
        const LayoutBox& box = ToLayoutBox(*object);
        if (box.LogicalHeight() && box.LogicalWidth())
          return true;
      } else if (object->IsLayoutInline()) {
        const LayoutInline& layout_inline = ToLayoutInline(*object);
        if (!layout_inline.LinesBoundingBox().IsEmpty())
          return true;
      } else {
        DCHECK(object->IsText() || object->IsSVG());
      }
    }
  }
  return false;
}

void LayoutBoxModelObject::AbsoluteQuadsForSelf(
    Vector<FloatQuad>& quads,
    MapCoordinatesFlags mode) const {
  NOTREACHED();
}

void LayoutBoxModelObject::AbsoluteQuads(Vector<FloatQuad>& quads,
                                         MapCoordinatesFlags mode) const {
  AbsoluteQuadsForSelf(quads, mode);

  // Iterate over continuations, avoiding recursion in case there are
  // many of them. See crbug.com/653767.
  for (const LayoutBoxModelObject* continuation_object = Continuation();
       continuation_object;
       continuation_object = continuation_object->Continuation()) {
    DCHECK(continuation_object->IsLayoutInline() ||
           (continuation_object->IsLayoutBlockFlow() &&
            ToLayoutBlockFlow(continuation_object)
                ->IsAnonymousBlockContinuation()));
    continuation_object->AbsoluteQuadsForSelf(quads, mode);
  }
}

void LayoutBoxModelObject::UpdateFromStyle() {
  const ComputedStyle& style_to_use = StyleRef();
  SetHasBoxDecorationBackground(style_to_use.HasBoxDecorationBackground());
  SetInline(style_to_use.IsDisplayInlineType());
  SetPositionState(style_to_use.GetPosition());
  SetHorizontalWritingMode(style_to_use.IsHorizontalWritingMode());
}

LayoutBlock* LayoutBoxModelObject::ContainingBlockForAutoHeightDetection(
    const Length& logical_height) const {
  // For percentage heights: The percentage is calculated with respect to the
  // height of the generated box's containing block. If the height of the
  // containing block is not specified explicitly (i.e., it depends on content
  // height), and this element is not absolutely positioned, the used height is
  // calculated as if 'auto' was specified.
  if (!logical_height.IsPercentOrCalc() || IsOutOfFlowPositioned())
    return nullptr;

  // Anonymous block boxes are ignored when resolving percentage values that
  // would refer to it: the closest non-anonymous ancestor box is used instead.
  LayoutBlock* cb = ContainingBlock();
  while (cb->IsAnonymous())
    cb = cb->ContainingBlock();

  // Matching LayoutBox::percentageLogicalHeightIsResolvableFromBlock() by
  // ignoring table cell's attribute value, where it says that table cells
  // violate what the CSS spec says to do with heights. Basically we don't care
  // if the cell specified a height or not.
  if (cb->IsTableCell())
    return nullptr;

  // Match LayoutBox::availableLogicalHeightUsing by special casing the layout
  // view. The available height is taken from the frame.
  if (cb->IsLayoutView())
    return nullptr;

  if (IsOutOfFlowPositionedWithImplicitHeight(cb))
    return nullptr;

  return cb;
}

bool LayoutBoxModelObject::HasAutoHeightOrContainingBlockWithAutoHeight(
    RegisterPercentageDescendant register_percentage_descendant) const {
  // TODO(rego): Check if we can somehow reuse LayoutBlock::
  // availableLogicalHeightForPercentageComputation() (see crbug.com/635655).
  const LayoutBox* this_box = IsBox() ? ToLayoutBox(this) : nullptr;
  const Length& logical_height_length = StyleRef().LogicalHeight();
  LayoutBlock* cb =
      ContainingBlockForAutoHeightDetection(logical_height_length);
  if (register_percentage_descendant == kRegisterPercentageDescendant &&
      logical_height_length.IsPercentOrCalc() && cb && IsBox()) {
    cb->AddPercentHeightDescendant(const_cast<LayoutBox*>(ToLayoutBox(this)));
  }
  if (this_box && this_box->IsFlexItem()) {
    const LayoutFlexibleBox& flex_box = ToLayoutFlexibleBox(*Parent());
    if (flex_box.UseOverrideLogicalHeightForPerentageResolution(*this_box))
      return false;
  }
  if (this_box && this_box->IsGridItem() &&
      this_box->HasOverrideContainingBlockContentLogicalHeight())
    return false;
  if (this_box && this_box->IsCustomItem() &&
      (this_box->HasOverrideContainingBlockContentLogicalHeight() ||
       this_box->HasOverrideContainingBlockPercentageResolutionLogicalHeight()))
    return false;

  if (logical_height_length.IsAuto() &&
      !IsOutOfFlowPositionedWithImplicitHeight(this))
    return true;

  if (GetDocument().InQuirksMode())
    return false;

  if (cb)
    return !cb->HasDefiniteLogicalHeight();

  return false;
}

LayoutSize LayoutBoxModelObject::RelativePositionOffset() const {
  DCHECK(IsRelPositioned());
  LayoutSize offset = AccumulateInFlowPositionOffsets();

  LayoutBlock* containing_block = ContainingBlock();

  // Objects that shrink to avoid floats normally use available line width when
  // computing containing block width. However in the case of relative
  // positioning using percentages, we can't do this. The offset should always
  // be resolved using the available width of the containing block. Therefore we
  // don't use containingBlockLogicalWidthForContent() here, but instead
  // explicitly call availableWidth on our containing block.
  // https://drafts.csswg.org/css-position-3/#rel-pos
  // However for grid items the containing block is the grid area, so offsets
  // should be resolved against that:
  // https://drafts.csswg.org/css-grid/#grid-item-sizing
  base::Optional<LayoutUnit> left;
  base::Optional<LayoutUnit> right;
  if (!StyleRef().Left().IsAuto() || !StyleRef().Right().IsAuto()) {
    LayoutUnit available_width = HasOverrideContainingBlockContentWidth()
                                     ? OverrideContainingBlockContentWidth()
                                     : containing_block->AvailableWidth();
    if (!StyleRef().Left().IsAuto())
      left = ValueForLength(StyleRef().Left(), available_width);
    if (!StyleRef().Right().IsAuto())
      right = ValueForLength(StyleRef().Right(), available_width);
  }
  if (!left && !right) {
    left = LayoutUnit();
    right = LayoutUnit();
  }
  if (!left)
    left = -right.value();
  if (!right)
    right = -left.value();
  bool is_ltr = containing_block->StyleRef().IsLeftToRightDirection();
  WritingMode writing_mode = containing_block->StyleRef().GetWritingMode();
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      if (is_ltr)
        offset.Expand(left.value(), LayoutUnit());
      else
        offset.SetWidth(-right.value());
      break;
    case WritingMode::kVerticalRl:
      offset.SetWidth(-right.value());
      break;
    case WritingMode::kVerticalLr:
      offset.Expand(left.value(), LayoutUnit());
      break;
    // TODO(layout-dev): Sideways-lr and sideways-rl are not yet supported.
    default:
      break;
  }

  // If the containing block of a relatively positioned element does not specify
  // a height, a percentage top or bottom offset should be resolved as auto.
  // An exception to this is if the containing block has the WinIE quirk where
  // <html> and <body> assume the size of the viewport. In this case, calculate
  // the percent offset based on this height.
  // See <https://bugs.webkit.org/show_bug.cgi?id=26396>.
  // Another exception is a grid item, as the containing block is the grid area:
  // https://drafts.csswg.org/css-grid/#grid-item-sizing

  base::Optional<LayoutUnit> top;
  base::Optional<LayoutUnit> bottom;
  bool has_override_containing_block_content_height =
      HasOverrideContainingBlockContentHeight();
  if (!StyleRef().Top().IsAuto() &&
      (!containing_block->HasAutoHeightOrContainingBlockWithAutoHeight() ||
       !StyleRef().Top().IsPercentOrCalc() ||
       containing_block->StretchesToViewport() ||
       has_override_containing_block_content_height)) {
    // TODO(rego): The computation of the available height is repeated later for
    // "bottom". We could refactor this and move it to some common code for both
    // ifs, however moving it outside of the ifs is not possible as it'd cause
    // performance regressions (see crbug.com/893884).
    top = ValueForLength(StyleRef().Top(),
                         has_override_containing_block_content_height
                             ? OverrideContainingBlockContentHeight()
                             : containing_block->AvailableHeight());
  }
  if (!StyleRef().Bottom().IsAuto() &&
      (!containing_block->HasAutoHeightOrContainingBlockWithAutoHeight() ||
       !StyleRef().Bottom().IsPercentOrCalc() ||
       containing_block->StretchesToViewport() ||
       has_override_containing_block_content_height)) {
    // TODO(rego): Check comment above for "top", it applies here too.
    bottom = ValueForLength(StyleRef().Bottom(),
                            has_override_containing_block_content_height
                                ? OverrideContainingBlockContentHeight()
                                : containing_block->AvailableHeight());
  }
  if (!top && !bottom) {
    top = LayoutUnit();
    bottom = LayoutUnit();
  }
  if (!top)
    top = -bottom.value();
  if (!bottom)
    bottom = -top.value();
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      offset.Expand(LayoutUnit(), top.value());
      break;
    case WritingMode::kVerticalRl:
      if (is_ltr)
        offset.Expand(LayoutUnit(), top.value());
      else
        offset.SetHeight(-bottom.value());
      break;
    case WritingMode::kVerticalLr:
      if (is_ltr)
        offset.Expand(LayoutUnit(), top.value());
      else
        offset.SetHeight(-bottom.value());
      break;
    // TODO(layout-dev): Sideways-lr and sideways-rl are not yet supported.
    default:
      break;
  }
  return offset;
}

void LayoutBoxModelObject::UpdateStickyPositionConstraints() const {
  DCHECK(StyleRef().HasStickyConstrainedPosition());

  const FloatSize constraining_size = ComputeStickyConstrainingRect().Size();

  StickyPositionScrollingConstraints constraints;
  FloatSize skipped_containers_offset;
  LayoutBlock* containing_block = ContainingBlock();
  // The location container for boxes is not always the containing block.
  LayoutObject* location_container =
      IsLayoutInline() ? Container() : ToLayoutBox(this)->LocationContainer();
  // Skip anonymous containing blocks.
  while (containing_block->IsAnonymous()) {
    containing_block = containing_block->ContainingBlock();
  }

  // The sticky position constraint rects should be independent of the current
  // scroll position therefore we should ignore the scroll offset when
  // calculating the quad.
  MapCoordinatesFlags flags = kIgnoreScrollOffset | kIgnoreStickyOffset;
  skipped_containers_offset =
      ToFloatSize(location_container
                      ->LocalToAncestorQuadWithoutTransforms(
                          FloatQuad(), containing_block, flags)
                      .BoundingBox()
                      .Location());
  LayoutBox& scroll_ancestor =
      ToLayoutBox(Layer()->AncestorOverflowLayer()->GetLayoutObject());

  LayoutUnit max_container_width =
      containing_block->IsLayoutView()
          ? containing_block->LogicalWidth()
          : containing_block->ContainingBlockLogicalWidthForContent();
  // Sticky positioned element ignore any override logical width on the
  // containing block, as they don't call containingBlockLogicalWidthForContent.
  // It's unclear whether this is totally fine.
  // Compute the container-relative area within which the sticky element is
  // allowed to move.
  LayoutUnit max_width = containing_block->AvailableLogicalWidth();

  // Map the containing block to the inner corner of the scroll ancestor without
  // transforms.
  FloatRect scroll_container_relative_padding_box_rect(
      containing_block->LayoutOverflowRect());
  FloatSize scroll_container_border_offset =
      FloatSize(scroll_ancestor.BorderLeft(), scroll_ancestor.BorderTop());
  if (containing_block != &scroll_ancestor) {
    FloatQuad local_quad(FloatRect(containing_block->PhysicalPaddingBoxRect()));
    scroll_container_relative_padding_box_rect =
        containing_block
            ->LocalToAncestorQuadWithoutTransforms(local_quad, &scroll_ancestor,
                                                   flags)
            .BoundingBox();
  }
  // Remove top-left border offset from overflow scroller.
  scroll_container_relative_padding_box_rect.Move(
      -scroll_container_border_offset);

  LayoutRect scroll_container_relative_containing_block_rect(
      scroll_container_relative_padding_box_rect);
  // This is removing the padding of the containing block's overflow rect to get
  // the flow box rectangle and removing the margin of the sticky element to
  // ensure that space between the sticky element and its containing flow box.
  // It is an open issue whether the margin should collapse.
  // See https://www.w3.org/TR/css-position-3/#sticky-pos
  scroll_container_relative_containing_block_rect.ContractEdges(
      MinimumValueForLength(containing_block->StyleRef().PaddingTop(),
                            max_container_width) +
          MinimumValueForLength(StyleRef().MarginTop(), max_width),
      MinimumValueForLength(containing_block->StyleRef().PaddingRight(),
                            max_container_width) +
          MinimumValueForLength(StyleRef().MarginRight(), max_width),
      MinimumValueForLength(containing_block->StyleRef().PaddingBottom(),
                            max_container_width) +
          MinimumValueForLength(StyleRef().MarginBottom(), max_width),
      MinimumValueForLength(containing_block->StyleRef().PaddingLeft(),
                            max_container_width) +
          MinimumValueForLength(StyleRef().MarginLeft(), max_width));

  constraints.scroll_container_relative_containing_block_rect =
      FloatRect(scroll_container_relative_containing_block_rect);

  FloatRect sticky_box_rect =
      IsLayoutInline() ? FloatRect(ToLayoutInline(this)->LinesBoundingBox())
                       : FloatRect(ToLayoutBox(this)->FrameRect());

  FloatRect flipped_sticky_box_rect = sticky_box_rect;
  containing_block->FlipForWritingMode(flipped_sticky_box_rect);
  FloatPoint sticky_location =
      flipped_sticky_box_rect.Location() + skipped_containers_offset;

  // The scrollContainerRelativePaddingBoxRect's position is the padding box so
  // we need to remove the border when finding the position of the sticky box
  // within the scroll ancestor if the container is not our scroll ancestor. If
  // the container is our scroll ancestor, we also need to remove the border
  // box because we want the position from within the scroller border.
  FloatSize container_border_offset(containing_block->BorderLeft(),
                                    containing_block->BorderTop());
  sticky_location -= container_border_offset;
  constraints.scroll_container_relative_sticky_box_rect =
      FloatRect(scroll_container_relative_padding_box_rect.Location() +
                    ToFloatSize(sticky_location),
                flipped_sticky_box_rect.Size());

  // To correctly compute the offsets, the constraints need to know about any
  // nested position:sticky elements between themselves and their
  // containingBlock, and between the containingBlock and their scrollAncestor.
  //
  // The respective search ranges are [container, containingBlock) and
  // [containingBlock, scrollAncestor).
  constraints.nearest_sticky_layer_shifting_sticky_box =
      FindFirstStickyBetween(location_container, containing_block);
  // We cannot use |scrollAncestor| here as it disregards the root
  // ancestorOverflowLayer(), which we should include.
  constraints.nearest_sticky_layer_shifting_containing_block =
      FindFirstStickyBetween(
          containing_block,
          &Layer()->AncestorOverflowLayer()->GetLayoutObject());

  // We skip the right or top sticky offset if there is not enough space to
  // honor both the left/right or top/bottom offsets.
  LayoutUnit horizontal_offsets =
      MinimumValueForLength(StyleRef().Right(),
                            LayoutUnit(constraining_size.Width())) +
      MinimumValueForLength(StyleRef().Left(),
                            LayoutUnit(constraining_size.Width()));
  bool skip_right = false;
  bool skip_left = false;
  if (!StyleRef().Left().IsAuto() && !StyleRef().Right().IsAuto()) {
    if (horizontal_offsets >
            scroll_container_relative_containing_block_rect.Width() ||
        horizontal_offsets + sticky_box_rect.Width() >
            constraining_size.Width()) {
      skip_right = StyleRef().IsLeftToRightDirection();
      skip_left = !skip_right;
    }
  }

  if (!StyleRef().Left().IsAuto() && !skip_left) {
    constraints.left_offset = MinimumValueForLength(
        StyleRef().Left(), LayoutUnit(constraining_size.Width()));
    constraints.is_anchored_left = true;
  }

  if (!StyleRef().Right().IsAuto() && !skip_right) {
    constraints.right_offset = MinimumValueForLength(
        StyleRef().Right(), LayoutUnit(constraining_size.Width()));
    constraints.is_anchored_right = true;
  }

  bool skip_bottom = false;
  // TODO(flackr): Exclude top or bottom edge offset depending on the writing
  // mode when related sections are fixed in spec.
  // See http://lists.w3.org/Archives/Public/www-style/2014May/0286.html
  LayoutUnit vertical_offsets =
      MinimumValueForLength(StyleRef().Top(),
                            LayoutUnit(constraining_size.Height())) +
      MinimumValueForLength(StyleRef().Bottom(),
                            LayoutUnit(constraining_size.Height()));
  if (!StyleRef().Top().IsAuto() && !StyleRef().Bottom().IsAuto()) {
    if (vertical_offsets >
            scroll_container_relative_containing_block_rect.Height() ||
        vertical_offsets + sticky_box_rect.Height() >
            constraining_size.Height()) {
      skip_bottom = true;
    }
  }

  if (!StyleRef().Top().IsAuto()) {
    constraints.top_offset = MinimumValueForLength(
        StyleRef().Top(), LayoutUnit(constraining_size.Height()));
    constraints.is_anchored_top = true;
  }

  if (!StyleRef().Bottom().IsAuto() && !skip_bottom) {
    constraints.bottom_offset = MinimumValueForLength(
        StyleRef().Bottom(), LayoutUnit(constraining_size.Height()));
    constraints.is_anchored_bottom = true;
  }
  PaintLayerScrollableArea* scrollable_area =
      Layer()->AncestorOverflowLayer()->GetScrollableArea();
  scrollable_area->GetStickyConstraintsMap().Set(Layer(), constraints);
}

bool LayoutBoxModelObject::IsSlowRepaintConstrainedObject() const {
  if (!HasLayer() || (StyleRef().GetPosition() != EPosition::kFixed &&
                      StyleRef().GetPosition() != EPosition::kSticky)) {
    return false;
  }

  PaintLayer* layer = Layer();

  // Whether the Layer sticks to the viewport is a tree-depenent
  // property and our viewportConstrainedObjects collection is maintained
  // with only LayoutObject-level information.
  if (!layer->FixedToViewport() && !layer->SticksToScroller())
    return false;

  // If the whole subtree is invisible, there's no reason to scroll on
  // the main thread because we don't need to generate invalidations
  // for invisible content.
  if (layer->SubtreeIsInvisible())
    return false;

  // We're only smart enough to scroll viewport-constrainted objects
  // in the compositor if they have their own backing or they paint
  // into a grouped back (which necessarily all have the same viewport
  // constraints).
  return (layer->GetCompositingState() == kNotComposited);
}

FloatRect LayoutBoxModelObject::ComputeStickyConstrainingRect() const {
  LayoutBox* enclosing_clipping_box =
      Layer()->AncestorOverflowLayer()->GetLayoutBox();
  DCHECK(enclosing_clipping_box);
  FloatRect constraining_rect;
  constraining_rect =
      FloatRect(enclosing_clipping_box->OverflowClipRect(LayoutPoint()));
  constraining_rect.Move(-enclosing_clipping_box->BorderLeft() +
                             enclosing_clipping_box->PaddingLeft(),
                         -enclosing_clipping_box->BorderTop() +
                             enclosing_clipping_box->PaddingTop());
  constraining_rect.Contract(
      FloatSize(enclosing_clipping_box->PaddingLeft() +
                    enclosing_clipping_box->PaddingRight(),
                enclosing_clipping_box->PaddingTop() +
                    enclosing_clipping_box->PaddingBottom()));
  return constraining_rect;
}

LayoutSize LayoutBoxModelObject::StickyPositionOffset() const {
  const PaintLayer* ancestor_overflow_layer = Layer()->AncestorOverflowLayer();
  // TODO: Force compositing input update if we ask for offset before
  // compositing inputs have been computed?
  if (!ancestor_overflow_layer || !ancestor_overflow_layer->GetScrollableArea())
    return LayoutSize();

  StickyConstraintsMap& constraints_map =
      ancestor_overflow_layer->GetScrollableArea()->GetStickyConstraintsMap();
  auto it = constraints_map.find(Layer());
  if (it == constraints_map.end())
    return LayoutSize();
  StickyPositionScrollingConstraints* constraints = &it->value;

  // The sticky offset is physical, so we can just return the delta computed in
  // absolute coords (though it may be wrong with transforms).
  FloatRect constraining_rect = ComputeStickyConstrainingRect();
  constraining_rect.MoveBy(
      ancestor_overflow_layer->GetScrollableArea()->ScrollPosition());
  return LayoutSize(
      constraints->ComputeStickyOffset(constraining_rect, constraints_map));
}

LayoutPoint LayoutBoxModelObject::AdjustedPositionRelativeTo(
    const LayoutPoint& start_point,
    const Element* offset_parent) const {
  // If the element is the HTML body element or doesn't have a parent
  // return 0 and stop this algorithm.
  if (IsBody() || !Parent())
    return LayoutPoint();

  LayoutPoint reference_point = start_point;

  // If the offsetParent is null, return the distance between the canvas origin
  // and the left/top border edge of the element and stop this algorithm.
  if (!offset_parent)
    return reference_point;

  if (const LayoutBoxModelObject* offset_parent_object =
          offset_parent->GetLayoutBoxModelObject()) {
    if (!IsOutOfFlowPositioned()) {
      if (IsInFlowPositioned())
        reference_point.Move(OffsetForInFlowPosition());

      // Note that we may fail to find |offsetParent| while walking the
      // container chain, if |offsetParent| is an inline split into
      // continuations: <body style="display:inline;" id="offsetParent">
      // <div id="this">
      // This is why we have to do a nullptr check here.
      for (const LayoutObject* current = Container();
           current && current->GetNode() != offset_parent;
           current = current->Container()) {
        // FIXME: What are we supposed to do inside SVG content?
        reference_point.Move(current->ColumnOffset(reference_point));
        if (current->IsBox() && !current->IsTableRow())
          reference_point.MoveBy(ToLayoutBox(current)->PhysicalLocation());
      }

      if (offset_parent_object->IsBox() && offset_parent_object->IsBody() &&
          !offset_parent_object->IsPositioned()) {
        reference_point.MoveBy(
            ToLayoutBox(offset_parent_object)->PhysicalLocation());
      }
    }

    if (offset_parent_object->IsLayoutInline()) {
      const LayoutInline* inline_parent = ToLayoutInline(offset_parent_object);

      if (IsBox() && IsOutOfFlowPositioned() &&
          inline_parent->CanContainOutOfFlowPositionedElement(
              StyleRef().GetPosition())) {
        // Offset for out of flow positioned elements with inline containers is
        // a special case in the CSS spec
        reference_point +=
            inline_parent->OffsetForInFlowPositionedInline(*ToLayoutBox(this));
      }

      reference_point -= inline_parent->FirstLineBoxTopLeft();
    }

    if (offset_parent_object->IsBox() && !offset_parent_object->IsBody()) {
      reference_point.Move(-ToLayoutBox(offset_parent_object)->BorderLeft(),
                           -ToLayoutBox(offset_parent_object)->BorderTop());
    }
  }

  return reference_point;
}

LayoutSize LayoutBoxModelObject::OffsetForInFlowPosition() const {
  if (IsRelPositioned())
    return RelativePositionOffset();

  if (IsStickyPositioned())
    return StickyPositionOffset();

  return LayoutSize();
}

LayoutUnit LayoutBoxModelObject::OffsetLeft(const Element* parent) const {
  // Note that LayoutInline and LayoutBox override this to pass a different
  // startPoint to adjustedPositionRelativeTo.
  return AdjustedPositionRelativeTo(LayoutPoint(), parent).X();
}

LayoutUnit LayoutBoxModelObject::OffsetTop(const Element* parent) const {
  // Note that LayoutInline and LayoutBox override this to pass a different
  // startPoint to adjustedPositionRelativeTo.
  return AdjustedPositionRelativeTo(LayoutPoint(), parent).Y();
}

int LayoutBoxModelObject::PixelSnappedOffsetWidth(const Element* parent) const {
  return SnapSizeToPixel(OffsetWidth(), OffsetLeft(parent));
}

int LayoutBoxModelObject::PixelSnappedOffsetHeight(
    const Element* parent) const {
  return SnapSizeToPixel(OffsetHeight(), OffsetTop(parent));
}

LayoutUnit LayoutBoxModelObject::ComputedCSSPadding(
    const Length& padding) const {
  LayoutUnit w;
  if (padding.IsPercentOrCalc())
    w = ContainingBlockLogicalWidthForContent();
  return MinimumValueForLength(padding, w);
}

LayoutUnit LayoutBoxModelObject::ContainingBlockLogicalWidthForContent() const {
  return ContainingBlock()->AvailableLogicalWidth();
}

LayoutBoxModelObject* LayoutBoxModelObject::Continuation() const {
  return (!g_continuation_map) ? nullptr : g_continuation_map->at(this);
}

void LayoutBoxModelObject::SetContinuation(LayoutBoxModelObject* continuation) {
  if (continuation) {
    DCHECK(continuation->IsLayoutInline() || continuation->IsLayoutBlockFlow());
    if (!g_continuation_map)
      g_continuation_map = new ContinuationMap;
    g_continuation_map->Set(this, continuation);
  } else {
    if (g_continuation_map)
      g_continuation_map->erase(this);
  }
}

void LayoutBoxModelObject::ComputeLayerHitTestRects(
    LayerHitTestRects& rects,
    TouchAction supported_fast_actions) const {
  LayoutObject::ComputeLayerHitTestRects(rects, supported_fast_actions);

  // If there is a continuation then we need to consult it here, since this is
  // the root of the tree walk and it wouldn't otherwise get picked up.
  // Continuations should always be siblings in the tree, so any others should
  // get picked up already by the tree walk.
  if (Continuation())
    Continuation()->ComputeLayerHitTestRects(rects, supported_fast_actions);
}

LayoutRect LayoutBoxModelObject::LocalCaretRectForEmptyElement(
    LayoutUnit width,
    LayoutUnit text_indent_offset) const {
  DCHECK(!SlowFirstChild() || SlowFirstChild()->IsPseudoElement());

  // FIXME: This does not take into account either :first-line or :first-letter
  // However, as soon as some content is entered, the line boxes will be
  // constructed and this kludge is not called any more. So only the caret size
  // of an empty :first-line'd block is wrong. I think we can live with that.
  const ComputedStyle& current_style = FirstLineStyleRef();

  enum CaretAlignment { kAlignLeft, kAlignRight, kAlignCenter };

  CaretAlignment alignment = kAlignLeft;

  switch (current_style.GetTextAlign()) {
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
      break;
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter:
      alignment = kAlignCenter;
      break;
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
      alignment = kAlignRight;
      break;
    case ETextAlign::kJustify:
    case ETextAlign::kStart:
      if (!current_style.IsLeftToRightDirection())
        alignment = kAlignRight;
      break;
    case ETextAlign::kEnd:
      if (current_style.IsLeftToRightDirection())
        alignment = kAlignRight;
      break;
  }

  LayoutUnit x = BorderLeft() + PaddingLeft();
  LayoutUnit max_x = width - BorderRight() - PaddingRight();
  LayoutUnit caret_width = GetFrameView()->CaretWidth();

  switch (alignment) {
    case kAlignLeft:
      if (current_style.IsLeftToRightDirection())
        x += text_indent_offset;
      break;
    case kAlignCenter:
      x = (x + max_x) / 2;
      if (current_style.IsLeftToRightDirection())
        x += text_indent_offset / 2;
      else
        x -= text_indent_offset / 2;
      break;
    case kAlignRight:
      x = max_x - caret_width;
      if (!current_style.IsLeftToRightDirection())
        x -= text_indent_offset;
      break;
  }
  x = std::min(x, (max_x - caret_width).ClampNegativeToZero());

  const Font& font = StyleRef().GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  LayoutUnit height;
  // crbug.com/595692 This check should not be needed but sometimes
  // primaryFont is null.
  if (font_data)
    height = LayoutUnit(font_data->GetFontMetrics().Height());
  LayoutUnit vertical_space =
      LineHeight(true,
                 current_style.IsHorizontalWritingMode() ? kHorizontalLine
                                                         : kVerticalLine,
                 kPositionOfInteriorLineBoxes) -
      height;
  LayoutUnit y = PaddingTop() + BorderTop() + (vertical_space / 2);
  return current_style.IsHorizontalWritingMode()
             ? LayoutRect(x, y, caret_width, height)
             : LayoutRect(y, x, height, caret_width);
}

const LayoutObject* LayoutBoxModelObject::PushMappingToContainer(
    const LayoutBoxModelObject* ancestor_to_stop_at,
    LayoutGeometryMap& geometry_map) const {
  DCHECK_NE(ancestor_to_stop_at, this);

  AncestorSkipInfo skip_info(ancestor_to_stop_at);
  LayoutObject* container = Container(&skip_info);
  if (!container)
    return nullptr;

  bool is_inline = IsLayoutInline();
  bool is_fixed_pos =
      !is_inline && StyleRef().GetPosition() == EPosition::kFixed;
  bool contains_fixed_position = CanContainFixedPositionObjects();

  TransformationMatrix adjustment_for_skipped_ancestor;
  bool adjustment_for_skipped_ancestor_is_translate2D = true;
  if (skip_info.AncestorSkipped()) {
    // There can't be a transform between container and ancestor_to_stop_at,
    // because transforms create containers, so it should be safe to just
    // subtract the delta between the container and ancestor_to_stop_at.
    LayoutSize ancestor_offset =
        ancestor_to_stop_at->OffsetFromAncestor(container);
    adjustment_for_skipped_ancestor.Translate(
        -ancestor_offset.Width().ToFloat(),
        -ancestor_offset.Height().ToFloat());
  }

  LayoutSize container_offset = OffsetFromContainer(container);
  bool offset_depends_on_point;
  if (IsLayoutFlowThread()) {
    container_offset += ColumnOffset(LayoutPoint());
    offset_depends_on_point = true;
  } else {
    offset_depends_on_point =
        container->StyleRef().IsFlippedBlocksWritingMode() &&
        container->IsBox();
  }

  bool preserve3d =
      container->StyleRef().Preserves3D() || StyleRef().Preserves3D();
  GeometryInfoFlags flags = 0;
  if (preserve3d)
    flags |= kAccumulatingTransform;
  if (offset_depends_on_point)
    flags |= kIsNonUniform;
  if (is_fixed_pos)
    flags |= kIsFixedPosition;
  if (contains_fixed_position)
    flags |= kContainsFixedPosition;
  if (ShouldUseTransformFromContainer(container)) {
    TransformationMatrix t;
    GetTransformFromContainer(container, container_offset, t);
    adjustment_for_skipped_ancestor.Multiply(t);
    geometry_map.Push(this, adjustment_for_skipped_ancestor, flags,
                      LayoutSize());
  } else if (adjustment_for_skipped_ancestor_is_translate2D) {
    container_offset.SetWidth(
        container_offset.Width() +
        LayoutUnit(adjustment_for_skipped_ancestor.M41()));
    container_offset.SetHeight(
        container_offset.Height() +
        LayoutUnit(adjustment_for_skipped_ancestor.M42()));
    geometry_map.Push(this, container_offset, flags, LayoutSize());
  } else {
    adjustment_for_skipped_ancestor.Translate(container_offset.Width(),
                                              container_offset.Height());
    geometry_map.Push(this, adjustment_for_skipped_ancestor, flags,
                      LayoutSize());
  }

  return skip_info.AncestorSkipped() ? ancestor_to_stop_at : container;
}

void LayoutBoxModelObject::MoveChildTo(
    LayoutBoxModelObject* to_box_model_object,
    LayoutObject* child,
    LayoutObject* before_child,
    bool full_remove_insert) {
  // We assume that callers have cleared their positioned objects list for child
  // moves (!fullRemoveInsert) so the positioned layoutObject maps don't become
  // stale. It would be too slow to do the map lookup on each call.
  DCHECK(!full_remove_insert || !IsLayoutBlock() ||
         !ToLayoutBlock(this)->HasPositionedObjects());

  DCHECK_EQ(this, child->Parent());
  DCHECK(!before_child || to_box_model_object == before_child->Parent());

  // If a child is moving from a block-flow to an inline-flow parent then any
  // floats currently intruding into the child can no longer do so. This can
  // happen if a block becomes floating or out-of-flow and is moved to an
  // anonymous block. Remove all floats from their float-lists immediately as
  // markAllDescendantsWithFloatsForLayout won't attempt to remove floats from
  // parents that have inline-flow if we try later.
  if (child->IsLayoutBlockFlow() && to_box_model_object->ChildrenInline() &&
      !ChildrenInline()) {
    ToLayoutBlockFlow(child)->RemoveFloatingObjectsFromDescendants();
    DCHECK(!ToLayoutBlockFlow(child)->ContainsFloats());
  }

  if (full_remove_insert && IsLayoutBlock() && child->IsBox())
    ToLayoutBox(child)->RemoveFromPercentHeightContainer();

  if (full_remove_insert && (to_box_model_object->IsLayoutBlock() ||
                             to_box_model_object->IsLayoutInline())) {
    // Takes care of adding the new child correctly if toBlock and fromBlock
    // have different kind of children (block vs inline).
    to_box_model_object->AddChild(
        VirtualChildren()->RemoveChildNode(this, child), before_child);
  } else {
    to_box_model_object->VirtualChildren()->InsertChildNode(
        to_box_model_object,
        VirtualChildren()->RemoveChildNode(this, child, full_remove_insert),
        before_child, full_remove_insert);
  }
}

void LayoutBoxModelObject::MoveChildrenTo(
    LayoutBoxModelObject* to_box_model_object,
    LayoutObject* start_child,
    LayoutObject* end_child,
    LayoutObject* before_child,
    bool full_remove_insert) {
  // This condition is rarely hit since this function is usually called on
  // anonymous blocks which can no longer carry positioned objects (see r120761)
  // or when fullRemoveInsert is false.
  if (full_remove_insert && IsLayoutBlock()) {
    LayoutBlock* block = ToLayoutBlock(this);
    block->RemovePositionedObjects(nullptr);
    block->RemoveFromPercentHeightContainer();
    if (block->IsLayoutBlockFlow())
      ToLayoutBlockFlow(block)->RemoveFloatingObjects();
  }

  DCHECK(!before_child || to_box_model_object == before_child->Parent());
  for (LayoutObject* child = start_child; child && child != end_child;) {
    // Save our next sibling as moveChildTo will clear it.
    LayoutObject* next_sibling = child->NextSibling();
    MoveChildTo(to_box_model_object, child, before_child, full_remove_insert);
    child = next_sibling;
  }
}

bool LayoutBoxModelObject::BackgroundTransfersToView(
    const ComputedStyle* document_element_style) const {
  // In our painter implementation, ViewPainter instead of the painter of the
  // layout object of the document element paints the view background.
  if (IsDocumentElement())
    return true;

  // http://www.w3.org/TR/css3-background/#body-background
  // If the document element is <html> with no background, and a <body> child
  // element exists, the <body> element's background transfers to the document
  // element which in turn transfers to the view in our painter implementation.
  if (!IsBody())
    return false;

  Element* document_element = GetDocument().documentElement();
  if (!IsHTMLHtmlElement(document_element))
    return false;

  if (!document_element_style)
    document_element_style = document_element->GetComputedStyle();
  DCHECK(document_element_style);
  if (document_element_style->HasBackground())
    return false;

  if (GetNode() != GetDocument().FirstBodyElement())
    return false;

  return true;
}

}  // namespace blink
