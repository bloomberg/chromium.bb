/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/paint/compositing/PaintLayerCompositor.h"

#include "core/animation/DocumentAnimations.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/ElementAnimations.h"
#include "core/dom/DOMNodeIds.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/fullscreen/Fullscreen.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/media/HTMLVideoElement.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/LayoutVideo.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/paint/FramePainter.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/ScrollableAreaPainter.h"
#include "core/paint/TransformRecorder.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "core/paint/compositing/CompositingInputsUpdater.h"
#include "core/paint/compositing/CompositingLayerAssigner.h"
#include "core/paint/compositing/CompositingRequirementsUpdater.h"
#include "core/paint/compositing/GraphicsLayerTreeBuilder.h"
#include "core/paint/compositing/GraphicsLayerUpdater.h"
#include "core/probe/CoreProbes.h"
#include "platform/Histogram.h"
#include "platform/bindings/ScriptForbiddenScope.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/TransformDisplayItem.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/json/JSONValues.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/Optional.h"

namespace blink {

PaintLayerCompositor::PaintLayerCompositor(LayoutView& layout_view)
    : layout_view_(layout_view),
      compositing_reason_finder_(layout_view),
      pending_update_type_(kCompositingUpdateNone),
      has_accelerated_compositing_(true),
      compositing_(false),
      root_should_always_composite_dirty_(true),
      needs_update_fixed_background_(false),
      in_overlay_fullscreen_video_(false),
      root_layer_attachment_(kRootLayerUnattached) {
  UpdateAcceleratedCompositingSettings();
}

PaintLayerCompositor::~PaintLayerCompositor() {
  DCHECK_EQ(root_layer_attachment_, kRootLayerUnattached);
}

bool PaintLayerCompositor::InCompositingMode() const {
  // FIXME: This should assert that lifecycle is >= CompositingClean since
  // the last step of updateIfNeeded can set this bit to false.
  DCHECK(layout_view_.Layer()->IsAllowedToQueryCompositingState());
  return compositing_;
}

bool PaintLayerCompositor::StaleInCompositingMode() const {
  return compositing_;
}

void PaintLayerCompositor::SetCompositingModeEnabled(bool enable) {
  if (enable == compositing_)
    return;

  compositing_ = enable;

  if (compositing_)
    EnsureRootLayer();
  else
    DestroyRootLayer();

  // Schedule an update in the parent frame so the <iframe>'s layer in the owner
  // document matches the compositing state here.
  if (HTMLFrameOwnerElement* owner_element =
          layout_view_.GetDocument().LocalOwner())
    owner_element->SetNeedsCompositingUpdate();
}

void PaintLayerCompositor::EnableCompositingModeIfNeeded() {
  if (!root_should_always_composite_dirty_)
    return;

  root_should_always_composite_dirty_ = false;
  if (compositing_)
    return;

  if (RootShouldAlwaysComposite()) {
    // FIXME: Is this needed? It was added in
    // https://bugs.webkit.org/show_bug.cgi?id=26651.
    // No tests fail if it's deleted.
    SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
    SetCompositingModeEnabled(true);
  }
}

bool PaintLayerCompositor::RootShouldAlwaysComposite() const {
  if (!has_accelerated_compositing_)
    return false;
  return layout_view_.GetFrame()->IsLocalRoot() ||
         compositing_reason_finder_.RequiresCompositingForScrollableFrame();
}

void PaintLayerCompositor::UpdateAcceleratedCompositingSettings() {
  compositing_reason_finder_.UpdateTriggers();
  has_accelerated_compositing_ = layout_view_.GetDocument()
                                     .GetSettings()
                                     ->GetAcceleratedCompositingEnabled();
  root_should_always_composite_dirty_ = true;
  if (root_layer_attachment_ != kRootLayerUnattached)
    RootLayer()->SetNeedsCompositingInputsUpdate();
}

bool PaintLayerCompositor::PreferCompositingToLCDTextEnabled() const {
  return layout_view_.GetDocument()
      .GetSettings()
      ->GetPreferCompositingToLCDTextEnabled();
}

static LayoutVideo* FindFullscreenVideoLayoutObject(Document& document) {
  // Recursively find the document that is in fullscreen.
  Element* fullscreen_element = Fullscreen::FullscreenElementFrom(document);
  Document* content_document = &document;
  while (fullscreen_element && fullscreen_element->IsFrameOwnerElement()) {
    content_document =
        ToHTMLFrameOwnerElement(fullscreen_element)->contentDocument();
    if (!content_document)
      return nullptr;
    fullscreen_element = Fullscreen::FullscreenElementFrom(*content_document);
  }
  if (!IsHTMLVideoElement(fullscreen_element))
    return nullptr;
  LayoutObject* layout_object = fullscreen_element->GetLayoutObject();
  if (!layout_object)
    return nullptr;
  return ToLayoutVideo(layout_object);
}

void PaintLayerCompositor::UpdateIfNeededRecursive(
    DocumentLifecycle::LifecycleState target_state) {
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.Compositing.UpdateTime");
  CompositingReasonsStats compositing_reasons_stats;
  UpdateIfNeededRecursiveInternal(target_state, compositing_reasons_stats);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.Compositing.LayerPromotionCount.Overlap",
                              compositing_reasons_stats.overlap_layers, 1, 100,
                              5);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Blink.Compositing.LayerPromotionCount.ActiveAnimation",
      compositing_reasons_stats.active_animation_layers, 1, 100, 5);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Blink.Compositing.LayerPromotionCount.AssumedOverlap",
      compositing_reasons_stats.assumed_overlap_layers, 1, 100, 5);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Blink.Compositing.LayerPromotionCount.IndirectComposited",
      compositing_reasons_stats.indirect_composited_layers, 1, 100, 5);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Blink.Compositing.LayerPromotionCount.TotalComposited",
      compositing_reasons_stats.total_composited_layers, 1, 1000, 10);
}

void PaintLayerCompositor::UpdateIfNeededRecursiveInternal(
    DocumentLifecycle::LifecycleState target_state,
    CompositingReasonsStats& compositing_reasons_stats) {
  DCHECK(target_state >= DocumentLifecycle::kCompositingInputsClean);

  LocalFrameView* view = layout_view_.GetFrameView();
  if (view->ShouldThrottleRendering())
    return;
  view->ResetNeedsForcedCompositingUpdate();

  for (Frame* child =
           layout_view_.GetFrameView()->GetFrame().Tree().FirstChild();
       child; child = child->Tree().NextSibling()) {
    if (!child->IsLocalFrame())
      continue;
    LocalFrame* local_frame = ToLocalFrame(child);
    // It's possible for trusted Pepper plugins to force hit testing in
    // situations where the frame tree is in an inconsistent state, such as in
    // the middle of frame detach.
    // TODO(bbudge) Remove this check when trusted Pepper plugins are gone.
    if (local_frame->GetDocument()->IsActive() &&
        !local_frame->ContentLayoutItem().IsNull()) {
      local_frame->ContentLayoutItem()
          .Compositor()
          ->UpdateIfNeededRecursiveInternal(target_state,
                                            compositing_reasons_stats);
    }
  }

  TRACE_EVENT0("blink", "PaintLayerCompositor::updateIfNeededRecursive");

  DCHECK(!layout_view_.NeedsLayout());

  ScriptForbiddenScope forbid_script;

  // FIXME: enableCompositingModeIfNeeded can trigger a
  // CompositingUpdateRebuildTree, which asserts that it's not
  // InCompositingUpdate.
  EnableCompositingModeIfNeeded();

  RootLayer()->UpdateDescendantDependentFlags();

  layout_view_.CommitPendingSelection();

  UpdateIfNeeded(target_state, compositing_reasons_stats);
  DCHECK(Lifecycle().GetState() == DocumentLifecycle::kCompositingInputsClean ||
         Lifecycle().GetState() == DocumentLifecycle::kCompositingClean);
  if (target_state == DocumentLifecycle::kCompositingInputsClean)
    return;

  Optional<CompositorElementIdSet> composited_element_ids;
  DocumentAnimations::UpdateAnimations(layout_view_.GetDocument(),
                                       DocumentLifecycle::kCompositingClean,
                                       composited_element_ids);

  layout_view_.GetFrameView()
      ->GetScrollableArea()
      ->UpdateCompositorScrollAnimations();
  if (const LocalFrameView::ScrollableAreaSet* animating_scrollable_areas =
          layout_view_.GetFrameView()->AnimatingScrollableAreas()) {
    for (ScrollableArea* scrollable_area : *animating_scrollable_areas)
      scrollable_area->UpdateCompositorScrollAnimations();
  }

#if DCHECK_IS_ON()
  DCHECK_EQ(Lifecycle().GetState(), DocumentLifecycle::kCompositingClean);
  AssertNoUnresolvedDirtyBits();
  for (Frame* child =
           layout_view_.GetFrameView()->GetFrame().Tree().FirstChild();
       child; child = child->Tree().NextSibling()) {
    if (!child->IsLocalFrame())
      continue;
    LocalFrame* local_frame = ToLocalFrame(child);
    if (local_frame->ShouldThrottleRendering() ||
        local_frame->ContentLayoutItem().IsNull())
      continue;
    local_frame->ContentLayoutItem()
        .Compositor()
        ->AssertNoUnresolvedDirtyBits();
  }
#endif
}

void PaintLayerCompositor::SetNeedsCompositingUpdate(
    CompositingUpdateType update_type) {
  DCHECK_NE(update_type, kCompositingUpdateNone);
  pending_update_type_ = std::max(pending_update_type_, update_type);
  if (Page* page = GetPage())
    page->Animator().ScheduleVisualUpdate(layout_view_.GetFrame());
  Lifecycle().EnsureStateAtMost(DocumentLifecycle::kLayoutClean);
}

void PaintLayerCompositor::DidLayout() {
  // FIXME: Technically we only need to do this when the LocalFrameView's
  // isScrollable method would return a different value.
  root_should_always_composite_dirty_ = true;
  EnableCompositingModeIfNeeded();

  // FIXME: Rather than marking the entire LayoutView as dirty, we should
  // track which Layers moved during layout and only dirty those
  // specific Layers.
  RootLayer()->SetNeedsCompositingInputsUpdate();
}

#if DCHECK_IS_ON()

void PaintLayerCompositor::AssertNoUnresolvedDirtyBits() {
  DCHECK_EQ(pending_update_type_, kCompositingUpdateNone);
  DCHECK(!root_should_always_composite_dirty_);
}

#endif

void PaintLayerCompositor::ApplyOverlayFullscreenVideoAdjustmentIfNeeded() {
  in_overlay_fullscreen_video_ = false;
  if (!root_content_layer_)
    return;

  bool is_local_root = layout_view_.GetFrame()->IsLocalRoot();
  LayoutVideo* video =
      FindFullscreenVideoLayoutObject(layout_view_.GetDocument());
  if (!video || !video->Layer()->HasCompositedLayerMapping() ||
      !video->VideoElement()->UsesOverlayFullscreenVideo()) {
    if (is_local_root) {
      GraphicsLayer* background_layer = FixedRootBackgroundLayer();
      if (background_layer && !background_layer->Parent())
        RootFixedBackgroundsChanged();
    }
    return;
  }

  GraphicsLayer* video_layer =
      video->Layer()->GetCompositedLayerMapping()->MainGraphicsLayer();

  // The fullscreen video has layer position equal to its enclosing frame's
  // scroll position because fullscreen container is fixed-positioned.
  // We should reset layer position here since we are going to reattach the
  // layer at the very top level.
  video_layer->SetPosition(IntPoint());

  // Only steal fullscreen video layer and clear all other layers if we are the
  // main frame.
  if (!is_local_root)
    return;

  root_content_layer_->RemoveAllChildren();
  overflow_controls_host_layer_->AddChild(video_layer);
  if (GraphicsLayer* background_layer = FixedRootBackgroundLayer())
    background_layer->RemoveFromParent();
  in_overlay_fullscreen_video_ = true;
}

void PaintLayerCompositor::UpdateWithoutAcceleratedCompositing(
    CompositingUpdateType update_type) {
  DCHECK(!HasAcceleratedCompositing());

  if (update_type >= kCompositingUpdateAfterCompositingInputChange)
    CompositingInputsUpdater(RootLayer()).Update();

#if DCHECK_IS_ON()
  CompositingInputsUpdater::AssertNeedsCompositingInputsUpdateBitsCleared(
      RootLayer());
#endif
}

static void ForceRecomputeVisualRectsIncludingNonCompositingDescendants(
    LayoutObject& layout_object) {
  // We clear the previous visual rect as it's wrong (paint invalidation
  // container changed, ...). Forcing a full invalidation will make us recompute
  // it. Also we are not changing the previous position from our paint
  // invalidation container, which is fine as we want a full paint invalidation
  // anyway.
  layout_object.ClearPreviousVisualRects();

  for (LayoutObject* child = layout_object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsPaintInvalidationContainer())
      ForceRecomputeVisualRectsIncludingNonCompositingDescendants(*child);
  }
}

GraphicsLayer* PaintLayerCompositor::ParentForContentLayers() const {
  if (root_content_layer_)
    return root_content_layer_.get();

  DCHECK(RuntimeEnabledFeatures::RootLayerScrollingEnabled());

  // Iframe content layers will be connected by the parent frame using
  // attachFrameContentLayersToIframeLayer.
  if (!IsMainFrame())
    return nullptr;

  // If this is a popup, don't hook into the VisualViewport layers.
  if (!layout_view_.GetDocument()
           .GetPage()
           ->GetChromeClient()
           .IsChromeClientImpl()) {
    return nullptr;
  }

  return GetVisualViewport().ScrollLayer();
}

void PaintLayerCompositor::UpdateIfNeeded(
    DocumentLifecycle::LifecycleState target_state,
    CompositingReasonsStats& compositing_reasons_stats) {
  DCHECK(target_state >= DocumentLifecycle::kCompositingInputsClean);

  Lifecycle().AdvanceTo(DocumentLifecycle::kInCompositingUpdate);

  if (pending_update_type_ < kCompositingUpdateAfterCompositingInputChange &&
      target_state == DocumentLifecycle::kCompositingInputsClean) {
    // The compositing inputs are already clean and that is our target state.
    // Early-exit here without clearing the pending update type since we haven't
    // handled e.g. geometry updates.
    Lifecycle().AdvanceTo(DocumentLifecycle::kCompositingInputsClean);
    return;
  }

  CompositingUpdateType update_type = pending_update_type_;
  pending_update_type_ = kCompositingUpdateNone;

  if (!HasAcceleratedCompositing()) {
    UpdateWithoutAcceleratedCompositing(update_type);
    Lifecycle().AdvanceTo(
        std::min(DocumentLifecycle::kCompositingClean, target_state));
    return;
  }

  if (update_type == kCompositingUpdateNone) {
    Lifecycle().AdvanceTo(
        std::min(DocumentLifecycle::kCompositingClean, target_state));
    return;
  }

  PaintLayer* update_root = RootLayer();

  Vector<PaintLayer*> layers_needing_paint_invalidation;

  if (update_type >= kCompositingUpdateAfterCompositingInputChange) {
    CompositingInputsUpdater(update_root).Update();

#if DCHECK_IS_ON()
    // FIXME: Move this check to the end of the compositing update.
    CompositingInputsUpdater::AssertNeedsCompositingInputsUpdateBitsCleared(
        update_root);
#endif

    // In the case where we only want to make compositing inputs clean, we
    // early-exit here. Because we have not handled the other implications of
    // |pending_update_type_| > kCompositingUpdateNone, we must restore the
    // pending update type for a future call.
    if (target_state == DocumentLifecycle::kCompositingInputsClean) {
      pending_update_type_ = update_type;
      Lifecycle().AdvanceTo(DocumentLifecycle::kCompositingInputsClean);
      return;
    }

    CompositingRequirementsUpdater(layout_view_, compositing_reason_finder_)
        .Update(update_root, compositing_reasons_stats);

    CompositingLayerAssigner layer_assigner(this);
    layer_assigner.Assign(update_root, layers_needing_paint_invalidation);

    bool layers_changed = layer_assigner.LayersChanged();

    {
      TRACE_EVENT0("blink",
                   "PaintLayerCompositor::updateAfterCompositingChange");
      if (const LocalFrameView::ScrollableAreaSet* scrollable_areas =
              layout_view_.GetFrameView()->ScrollableAreas()) {
        for (ScrollableArea* scrollable_area : *scrollable_areas)
          layers_changed |= scrollable_area->UpdateAfterCompositingChange();
      }
      layers_changed |=
          layout_view_.GetFrameView()->UpdateAfterCompositingChange();
    }

    if (layers_changed) {
      update_type = std::max(update_type, kCompositingUpdateRebuildTree);
      if (ScrollingCoordinator* scrolling_coordinator =
              GetScrollingCoordinator()) {
        LocalFrameView* frame_view = layout_view_.GetFrameView();
        scrolling_coordinator->NotifyGeometryChanged(frame_view);
      }
    }
  }

  GraphicsLayerUpdater updater;
  updater.Update(*update_root, layers_needing_paint_invalidation);

  if (updater.NeedsRebuildTree())
    update_type = std::max(update_type, kCompositingUpdateRebuildTree);

#if DCHECK_IS_ON()
  // FIXME: Move this check to the end of the compositing update.
  GraphicsLayerUpdater::AssertNeedsToUpdateGraphicsLayerBitsCleared(
      *update_root);
#endif

  if (update_type >= kCompositingUpdateRebuildTree) {
    GraphicsLayerVector child_list;
    {
      TRACE_EVENT0("blink", "GraphicsLayerTreeBuilder::rebuild");
      GraphicsLayerTreeBuilder().Rebuild(*update_root, child_list);
    }

    if (!child_list.IsEmpty()) {
      CHECK(compositing_);
      if (GraphicsLayer* content_parent = ParentForContentLayers())
        content_parent->SetChildren(child_list);
    }

    ApplyOverlayFullscreenVideoAdjustmentIfNeeded();
  }

  if (needs_update_fixed_background_) {
    RootFixedBackgroundsChanged();
    needs_update_fixed_background_ = false;
  }

  for (unsigned i = 0; i < layers_needing_paint_invalidation.size(); i++) {
    ForceRecomputeVisualRectsIncludingNonCompositingDescendants(
        layers_needing_paint_invalidation[i]->GetLayoutObject());
  }

  if (root_layer_attachment_ == kRootLayerPendingAttachViaChromeClient) {
    if (Page* page = layout_view_.GetFrame()->GetPage()) {
      page->GetChromeClient().AttachRootGraphicsLayer(RootGraphicsLayer(),
                                                      layout_view_.GetFrame());
      root_layer_attachment_ = kRootLayerAttachedViaChromeClient;
    }
  }

  if (!RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    bool is_root_scroller_ancestor = IsRootScrollerAncestor();

    if (scroll_layer_)
      scroll_layer_->SetIsResizedByBrowserControls(is_root_scroller_ancestor);

    // Clip a frame's overflow controls layer only if it's not an ancestor of
    // the root scroller. If it is an ancestor, then it's guaranteed to be
    // viewport sized and so will be appropriately clipped by the visual
    // viewport. We don't want to clip here in this case so that URL bar
    // expansion doesn't need to resize the clip.

    if (overflow_controls_host_layer_) {
      overflow_controls_host_layer_->SetMasksToBounds(
          !is_root_scroller_ancestor);
    }
  }

  // Inform the inspector that the layer tree has changed.
  if (IsMainFrame())
    probe::layerTreeDidChange(layout_view_.GetFrame());

  Lifecycle().AdvanceTo(DocumentLifecycle::kCompositingClean);
}

static void RestartAnimationOnCompositor(const LayoutObject& layout_object) {
  Node* node = layout_object.GetNode();
  ElementAnimations* element_animations =
      (node && node->IsElementNode()) ? ToElement(node)->GetElementAnimations()
                                      : nullptr;
  if (element_animations)
    element_animations->RestartAnimationOnCompositor();
}

bool PaintLayerCompositor::AllocateOrClearCompositedLayerMapping(
    PaintLayer* layer,
    const CompositingStateTransitionType composited_layer_update) {
  bool composited_layer_mapping_changed = false;

  // FIXME: It would be nice to directly use the layer's compositing reason,
  // but allocateOrClearCompositedLayerMapping also gets called without having
  // updated compositing requirements fully.
  switch (composited_layer_update) {
    case kAllocateOwnCompositedLayerMapping:
      DCHECK(!layer->HasCompositedLayerMapping());
      SetCompositingModeEnabled(true);

      // If we need to issue paint invalidations, do so before allocating the
      // compositedLayerMapping and clearing out the groupedMapping.
      PaintInvalidationOnCompositingChange(layer);

      // If this layer was previously squashed, we need to remove its reference
      // to a groupedMapping right away, so that computing paint invalidation
      // rects will know the layer's correct compositingState.
      // FIXME: do we need to also remove the layer from it's location in the
      // squashing list of its groupedMapping?  Need to create a test where a
      // squashed layer pops into compositing. And also to cover all other sorts
      // of compositingState transitions.
      layer->SetLostGroupedMapping(false);
      layer->SetGroupedMapping(
          nullptr, PaintLayer::kInvalidateLayerAndRemoveFromMapping);

      layer->EnsureCompositedLayerMapping();
      composited_layer_mapping_changed = true;

      RestartAnimationOnCompositor(layer->GetLayoutObject());

      // At this time, the ScrollingCoordinator only supports the top-level
      // frame.
      if (layer->IsRootLayer() && layout_view_.GetFrame()->IsLocalRoot()) {
        if (ScrollingCoordinator* scrolling_coordinator =
                GetScrollingCoordinator()) {
          scrolling_coordinator->FrameViewRootLayerDidChange(
              layout_view_.GetFrameView());
        }
      }
      break;
    case kRemoveOwnCompositedLayerMapping:
    // PutInSquashingLayer means you might have to remove the composited layer
    // mapping first.
    case kPutInSquashingLayer:
      if (layer->HasCompositedLayerMapping()) {
        layer->ClearCompositedLayerMapping();
        composited_layer_mapping_changed = true;
      }

      break;
    case kRemoveFromSquashingLayer:
    case kNoCompositingStateChange:
      // Do nothing.
      break;
  }

  if (!composited_layer_mapping_changed)
    return false;

  if (layer->GetLayoutObject().IsLayoutEmbeddedContent()) {
    PaintLayerCompositor* inner_compositor = FrameContentsCompositor(
        ToLayoutEmbeddedContent(layer->GetLayoutObject()));
    if (inner_compositor && inner_compositor->StaleInCompositingMode())
      inner_compositor->EnsureRootLayer();
  }

  layer->ClearClipRects(kPaintingClipRects);

  // If a fixed position layer gained/lost a compositedLayerMapping or the
  // reason not compositing it changed, the scrolling coordinator needs to
  // recalculate whether it can do fast scrolling.
  if (ScrollingCoordinator* scrolling_coordinator = GetScrollingCoordinator()) {
    scrolling_coordinator->FrameViewFixedObjectsDidChange(
        layout_view_.GetFrameView());
  }

  // Compositing state affects whether to create paint offset translation of
  // this layer, and amount of paint offset translation of descendants.
  layer->GetLayoutObject().SetNeedsPaintPropertyUpdate();

  return true;
}

void PaintLayerCompositor::PaintInvalidationOnCompositingChange(
    PaintLayer* layer) {
  // If the layoutObject is not attached yet, no need to issue paint
  // invalidations.
  if (&layer->GetLayoutObject() != &layout_view_ &&
      !layer->GetLayoutObject().Parent())
    return;

  // For querying Layer::compositingState()
  // Eager invalidation here is correct, since we are invalidating with respect
  // to the previous frame's compositing state when changing the compositing
  // backing of the layer.
  DisableCompositingQueryAsserts disabler;
  // We have to do immediate paint invalidation because compositing will change.
  DisablePaintInvalidationStateAsserts paint_invalidation_assertisabler;

  ObjectPaintInvalidator(layer->GetLayoutObject())
      .InvalidatePaintIncludingNonCompositingDescendants();
}

void PaintLayerCompositor::FrameViewDidChangeLocation(
    const IntPoint& contents_offset) {
  if (overflow_controls_host_layer_)
    overflow_controls_host_layer_->SetPosition(contents_offset);
}

void PaintLayerCompositor::UpdateContainerSizes() {
  if (!container_layer_)
    return;

  LocalFrameView* frame_view = layout_view_.GetFrameView();
  container_layer_->SetSize(FloatSize(frame_view->VisibleContentSize()));
  overflow_controls_host_layer_->SetSize(
      FloatSize(frame_view->VisibleContentSize(kIncludeScrollbars)));
}

void PaintLayerCompositor::FrameViewDidChangeSize() {
  if (!container_layer_)
    return;

  UpdateContainerSizes();
  FrameViewDidScroll();
  UpdateOverflowControlsLayers();
}

enum AcceleratedFixedRootBackgroundHistogramBuckets {
  kScrolledMainFrameBucket = 0,
  kScrolledMainFrameWithAcceleratedFixedRootBackground = 1,
  kScrolledMainFrameWithUnacceleratedFixedRootBackground = 2,
  kAcceleratedFixedRootBackgroundHistogramMax = 3
};

void PaintLayerCompositor::FrameViewDidScroll() {
  LocalFrameView* frame_view = layout_view_.GetFrameView();
  IntSize scroll_offset = frame_view->ScrollOffsetInt();

  if (!scroll_layer_)
    return;

  bool scrolling_coordinator_handles_offset = false;
  if (ScrollingCoordinator* scrolling_coordinator = GetScrollingCoordinator()) {
    scrolling_coordinator_handles_offset =
        scrolling_coordinator->ScrollableAreaScrollLayerDidChange(frame_view);
  }

  // Scroll position = scroll origin + scroll offset. Adjust the layer's
  // position to handle whatever the scroll coordinator isn't handling.
  // The scroll origin is non-zero for RTL pages with overflow.
  if (scrolling_coordinator_handles_offset)
    scroll_layer_->SetPosition(frame_view->ScrollOrigin());
  else
    scroll_layer_->SetPosition(IntPoint(-scroll_offset));

  ShowScrollbarLayersIfNeeded();

  DEFINE_STATIC_LOCAL(EnumerationHistogram, accelerated_background_histogram,
                      ("Renderer.AcceleratedFixedRootBackground",
                       kAcceleratedFixedRootBackgroundHistogramMax));
  accelerated_background_histogram.Count(kScrolledMainFrameBucket);
}

void PaintLayerCompositor::FrameViewScrollbarsExistenceDidChange() {
  if (container_layer_)
    UpdateOverflowControlsLayers();
}

void PaintLayerCompositor::RootFixedBackgroundsChanged() {
  if (!container_layer_)
    return;

  // To avoid having to make the fixed root background layer fixed positioned to
  // stay put, we position it in the layer tree as follows:
  //
  // + Overflow controls host
  //   + LocalFrame clip
  //     + (Fixed root background) <-- Here.
  //     + LocalFrame scroll
  //       + Root content layer
  //   + Scrollbars
  //
  // That is, it needs to be the first child of the frame clip, the sibling of
  // the frame scroll layer. The compositor does not own the background layer,
  // it just positions it (like the foreground layer).
  if (GraphicsLayer* background_layer = FixedRootBackgroundLayer())
    container_layer_->AddChildBelow(background_layer, scroll_layer_.get());
}

std::unique_ptr<JSONObject> PaintLayerCompositor::LayerTreeAsJSON(
    LayerTreeFlags flags) const {
  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean ||
         layout_view_.GetFrameView()->ShouldThrottleRendering());

  // We skip dumping the scroll and clip layers to keep layerTreeAsText output
  // similar between platforms (unless we explicitly request dumping from the
  // root.
  const GraphicsLayer* root_layer = root_content_layer_.get();
  if (root_layer && root_layer->Children().size() == 1) {
    // Omit the root_content_layer_ itself, start from the layer for the root
    // LayoutView.
    root_layer = root_layer->Children()[0];
  }

  if (!root_layer)
    root_layer = RootGraphicsLayer();

  if (!root_layer)
    return nullptr;

  if (flags & kLayerTreeIncludesRootLayer) {
    if (IsMainFrame()) {
      while (root_layer->Parent())
        root_layer = root_layer->Parent();
    } else {
      root_layer = RootGraphicsLayer();
    }
  }

  return root_layer->LayerTreeAsJSON(flags);
}

PaintLayerCompositor* PaintLayerCompositor::FrameContentsCompositor(
    LayoutEmbeddedContent& layout_object) {
  if (!layout_object.GetNode()->IsFrameOwnerElement())
    return nullptr;

  HTMLFrameOwnerElement* element =
      ToHTMLFrameOwnerElement(layout_object.GetNode());
  if (Document* content_document = element->contentDocument()) {
    if (LayoutViewItem view = content_document->GetLayoutViewItem())
      return view.Compositor();
  }
  return nullptr;
}

bool PaintLayerCompositor::AttachFrameContentLayersToIframeLayer(
    LayoutEmbeddedContent& layout_object) {
  PaintLayerCompositor* inner_compositor =
      FrameContentsCompositor(layout_object);
  if (!inner_compositor || !inner_compositor->StaleInCompositingMode() ||
      inner_compositor->root_layer_attachment_ !=
          kRootLayerAttachedViaEnclosingFrame)
    return false;

  PaintLayer* layer = layout_object.Layer();
  if (!layer->HasCompositedLayerMapping())
    return false;

  DisableCompositingQueryAsserts disabler;
  layer->GetCompositedLayerMapping()->SetSublayers(
      GraphicsLayerVector(1, inner_compositor->RootGraphicsLayer()));
  return true;
}

static void FullyInvalidatePaintRecursive(PaintLayer* layer) {
  if (layer->GetCompositingState() == kPaintsIntoOwnBacking) {
    layer->GetCompositedLayerMapping()->SetContentsNeedDisplay();
    layer->GetCompositedLayerMapping()->SetSquashingContentsNeedDisplay();
  }

  for (PaintLayer* child = layer->FirstChild(); child;
       child = child->NextSibling())
    FullyInvalidatePaintRecursive(child);
}

void PaintLayerCompositor::FullyInvalidatePaint() {
  // We're walking all compositing layers and invalidating them, so there's
  // no need to have up-to-date compositing state.
  DisableCompositingQueryAsserts disabler;
  FullyInvalidatePaintRecursive(RootLayer());
}

PaintLayer* PaintLayerCompositor::RootLayer() const {
  return layout_view_.Layer();
}

GraphicsLayer* PaintLayerCompositor::RootGraphicsLayer() const {
  if (overflow_controls_host_layer_)
    return overflow_controls_host_layer_.get();
  if (CompositedLayerMapping* clm = RootLayer()->GetCompositedLayerMapping())
    return clm->ChildForSuperlayers();
  return nullptr;
}

GraphicsLayer* PaintLayerCompositor::ScrollLayer() const {
  if (ScrollableArea* scrollable_area =
          layout_view_.GetFrameView()->GetScrollableArea())
    return scrollable_area->LayerForScrolling();
  return nullptr;
}

void PaintLayerCompositor::SetIsInWindow(bool is_in_window) {
  if (!StaleInCompositingMode())
    return;

  if (is_in_window) {
    if (root_layer_attachment_ != kRootLayerUnattached)
      return;

    AttachCompositorTimeline();
    AttachRootLayer();
  } else {
    if (root_layer_attachment_ == kRootLayerUnattached)
      return;

    DetachRootLayer();
    DetachCompositorTimeline();
  }
}

void PaintLayerCompositor::UpdateRootLayerPosition() {
  if (root_content_layer_) {
    IntRect document_rect = layout_view_.DocumentRect();

    // Ensure the root content layer is at least the size of the outer viewport
    // so that we don't end up clipping position: fixed elements if the
    // document is smaller.
    document_rect.Unite(
        IntRect(document_rect.Location(),
                layout_view_.GetFrameView()->VisibleContentSize()));

    root_content_layer_->SetSize(FloatSize(document_rect.Size()));
    root_content_layer_->SetPosition(document_rect.Location());
  }
  if (container_layer_)
    UpdateContainerSizes();
}

void PaintLayerCompositor::UpdatePotentialCompositingReasonsFromStyle(
    PaintLayer* layer) {
  layer->SetPotentialCompositingReasonsFromStyle(
      compositing_reason_finder_.PotentialCompositingReasonsFromStyle(
          layer->GetLayoutObject()));
}

bool PaintLayerCompositor::CanBeComposited(const PaintLayer* layer) const {
  LocalFrameView* frame_view = layer->GetLayoutObject().GetFrameView();
  // Elements within an invisible frame must not be composited because they are
  // not drawn.
  if (frame_view && !frame_view->IsVisible())
    return false;

  const bool has_compositor_animation =
      compositing_reason_finder_.RequiresCompositingForAnimation(
          *layer->GetLayoutObject().Style());
  return has_accelerated_compositing_ &&
         (has_compositor_animation || !layer->SubtreeIsInvisible()) &&
         layer->IsSelfPaintingLayer() &&
         !layer->GetLayoutObject().IsLayoutFlowThread();
}

// Return true if the given layer is a stacking context and has compositing
// child layers that it needs to clip. In this case we insert a clipping
// GraphicsLayer into the hierarchy between this layer and its children in the
// z-order hierarchy.
bool PaintLayerCompositor::ClipsCompositingDescendants(
    const PaintLayer* layer) const {
  if (!layer->HasCompositingDescendant())
    return false;
  if (!layer->GetLayoutObject().IsBox())
    return false;
  const LayoutBox& box = ToLayoutBox(layer->GetLayoutObject());
  return box.ShouldClipOverflow() || box.HasClip();
}

// If an element has composited negative z-index children, those children paint
// in front of the layer background, so we need an extra 'contents' layer for
// the foreground of the layer object.
bool PaintLayerCompositor::NeedsContentsCompositingLayer(
    const PaintLayer* layer) const {
  if (!layer->HasCompositingDescendant())
    return false;
  return layer->StackingNode()->HasNegativeZOrderList();
}

IntRect PaintLayerCompositor::ComputeInterestRect(
    const GraphicsLayer* graphics_layer,
    const IntRect&) const {
  return EnclosingIntRect(FloatRect(FloatPoint(), graphics_layer->Size()));
}

void PaintLayerCompositor::PaintContents(const GraphicsLayer* graphics_layer,
                                         GraphicsContext& context,
                                         GraphicsLayerPaintingPhase,
                                         const IntRect& interest_rect) const {
  // Note the composited scrollable area painted below is always associated with
  // a frame. For painting non-frame ScrollableAreas, see
  // CompositedLayerMapping::paintScrollableArea.

  const Scrollbar* scrollbar = GraphicsLayerToScrollbar(graphics_layer);
  if (!scrollbar && graphics_layer != LayerForScrollCorner())
    return;

  // Map context and cull_rect which are in the local space of the scrollbar
  // to the space of the containing scrollable area in which Scrollbar::Paint()
  // will paint the scrollbar.
  IntSize offset = graphics_layer->OffsetFromLayoutObject();
  IntRect cull_rect = interest_rect;
  cull_rect.Move(offset);
  TransformRecorder transform_recorder(
      context, *graphics_layer,
      AffineTransform::Translation(-offset.Width(), -offset.Height()));

  if (scrollbar) {
    scrollbar->Paint(context, CullRect(cull_rect));
  } else {
    FramePainter(*layout_view_.GetFrameView())
        .PaintScrollCorner(context, cull_rect);
  }
}

Scrollbar* PaintLayerCompositor::GraphicsLayerToScrollbar(
    const GraphicsLayer* graphics_layer) const {
  if (graphics_layer == LayerForHorizontalScrollbar()) {
    return layout_view_.GetFrameView()->HorizontalScrollbar();
  }
  if (graphics_layer == LayerForVerticalScrollbar()) {
    return layout_view_.GetFrameView()->VerticalScrollbar();
  }
  return nullptr;
}

bool PaintLayerCompositor::NeedsFixedRootBackgroundLayer() const {
  return !RuntimeEnabledFeatures::RootLayerScrollingEnabled() &&
         PreferCompositingToLCDTextEnabled() &&
         layout_view_.RootBackgroundIsEntirelyFixed();
}

GraphicsLayer* PaintLayerCompositor::FixedRootBackgroundLayer() const {
  // Get the fixed root background from the LayoutView layer's
  // compositedLayerMapping.
  PaintLayer* view_layer = layout_view_.Layer();
  if (!view_layer)
    return nullptr;

  if (view_layer->GetCompositingState() == kPaintsIntoOwnBacking &&
      view_layer->GetCompositedLayerMapping()
          ->BackgroundLayerPaintsFixedRootBackground())
    return view_layer->GetCompositedLayerMapping()->BackgroundLayer();

  return nullptr;
}

static void UpdateTrackingRasterInvalidationsRecursive(
    GraphicsLayer* graphics_layer) {
  if (!graphics_layer)
    return;

  graphics_layer->UpdateTrackingRasterInvalidations();

  for (size_t i = 0; i < graphics_layer->Children().size(); ++i)
    UpdateTrackingRasterInvalidationsRecursive(graphics_layer->Children()[i]);

  if (GraphicsLayer* mask_layer = graphics_layer->MaskLayer())
    UpdateTrackingRasterInvalidationsRecursive(mask_layer);

  if (GraphicsLayer* clipping_mask_layer =
          graphics_layer->ContentsClippingMaskLayer())
    UpdateTrackingRasterInvalidationsRecursive(clipping_mask_layer);
}

void PaintLayerCompositor::UpdateTrackingRasterInvalidations() {
#if DCHECK_IS_ON()
  LocalFrameView* view = layout_view_.GetFrameView();
  DCHECK(Lifecycle().GetState() == DocumentLifecycle::kPaintClean ||
         (view && view->ShouldThrottleRendering()));
#endif

  if (GraphicsLayer* root_layer = RootGraphicsLayer())
    UpdateTrackingRasterInvalidationsRecursive(root_layer);
}

bool PaintLayerCompositor::IsTrackingRasterInvalidations() const {
  return layout_view_.GetFrameView()->IsTrackingPaintInvalidations();
}

bool PaintLayerCompositor::RequiresHorizontalScrollbarLayer() const {
  return layout_view_.GetFrameView()->HorizontalScrollbar();
}

bool PaintLayerCompositor::RequiresVerticalScrollbarLayer() const {
  return layout_view_.GetFrameView()->VerticalScrollbar();
}

bool PaintLayerCompositor::RequiresScrollCornerLayer() const {
  return layout_view_.GetFrameView()->IsScrollCornerVisible();
}

void PaintLayerCompositor::UpdateOverflowControlsLayers() {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  GraphicsLayer* controls_parent = overflow_controls_host_layer_.get();
  // Main frame scrollbars should always be stuck to the sides of the screen (in
  // overscroll and in pinch-zoom), so make the parent for the scrollbars be the
  // viewport container layer.
  if (IsMainFrame())
    controls_parent = GetVisualViewport().ContainerLayer();

  if (RequiresHorizontalScrollbarLayer()) {
    if (!layer_for_horizontal_scrollbar_) {
      layer_for_horizontal_scrollbar_ = GraphicsLayer::Create(this);
    }

    if (layer_for_horizontal_scrollbar_->Parent() != controls_parent) {
      controls_parent->AddChild(layer_for_horizontal_scrollbar_.get());

      if (ScrollingCoordinator* scrolling_coordinator =
              GetScrollingCoordinator()) {
        scrolling_coordinator->ScrollableAreaScrollbarLayerDidChange(
            layout_view_.GetFrameView(), kHorizontalScrollbar);
      }
    }
  } else if (layer_for_horizontal_scrollbar_) {
    layer_for_horizontal_scrollbar_->RemoveFromParent();
    layer_for_horizontal_scrollbar_ = nullptr;

    if (ScrollingCoordinator* scrolling_coordinator =
            GetScrollingCoordinator()) {
      scrolling_coordinator->ScrollableAreaScrollbarLayerDidChange(
          layout_view_.GetFrameView(), kHorizontalScrollbar);
    }
  }

  if (RequiresVerticalScrollbarLayer()) {
    if (!layer_for_vertical_scrollbar_) {
      layer_for_vertical_scrollbar_ = GraphicsLayer::Create(this);
    }

    if (layer_for_vertical_scrollbar_->Parent() != controls_parent) {
      controls_parent->AddChild(layer_for_vertical_scrollbar_.get());

      if (ScrollingCoordinator* scrolling_coordinator =
              GetScrollingCoordinator()) {
        scrolling_coordinator->ScrollableAreaScrollbarLayerDidChange(
            layout_view_.GetFrameView(), kVerticalScrollbar);
      }
    }
  } else if (layer_for_vertical_scrollbar_) {
    layer_for_vertical_scrollbar_->RemoveFromParent();
    layer_for_vertical_scrollbar_ = nullptr;

    if (ScrollingCoordinator* scrolling_coordinator =
            GetScrollingCoordinator()) {
      scrolling_coordinator->ScrollableAreaScrollbarLayerDidChange(
          layout_view_.GetFrameView(), kVerticalScrollbar);
    }
  }

  if (RequiresScrollCornerLayer()) {
    if (!layer_for_scroll_corner_)
      layer_for_scroll_corner_ = GraphicsLayer::Create(this);

    if (layer_for_scroll_corner_->Parent() != controls_parent)
      controls_parent->AddChild(layer_for_scroll_corner_.get());
  } else if (layer_for_scroll_corner_) {
    layer_for_scroll_corner_->RemoveFromParent();
    layer_for_scroll_corner_ = nullptr;
  }

  layout_view_.GetFrameView()->PositionScrollbarLayers();
  ShowScrollbarLayersIfNeeded();
}

void PaintLayerCompositor::ShowScrollbarLayersIfNeeded() {
  LocalFrameView* frame_view = layout_view_.GetFrameView();
  if (scroll_layer_ && frame_view->NeedsShowScrollbarLayers()) {
    scroll_layer_->PlatformLayer()->ShowScrollbars();
    frame_view->DidShowScrollbarLayers();
  }
}

void PaintLayerCompositor::EnsureRootLayer() {
  if (root_layer_attachment_ != kRootLayerUnattached)
    return;

  if (IsMainFrame())
    GetVisualViewport().CreateLayerTree();

  // When RLS is enabled, none of the PLC GraphicsLayers exist.
  bool should_create_own_layers =
      !RuntimeEnabledFeatures::RootLayerScrollingEnabled();

  if (should_create_own_layers && !root_content_layer_) {
    root_content_layer_ = GraphicsLayer::Create(this);
    IntRect overflow_rect = layout_view_.PixelSnappedLayoutOverflowRect();
    root_content_layer_->SetSize(
        FloatSize(overflow_rect.MaxX(), overflow_rect.MaxY()));
    root_content_layer_->SetPosition(FloatPoint());
    root_content_layer_->SetOwnerNodeId(
        DOMNodeIds::IdForNode(layout_view_.GetNode()));
  }

  if (should_create_own_layers && !overflow_controls_host_layer_) {
    DCHECK(!scroll_layer_);
    DCHECK(!container_layer_);

    // Create a layer to host the clipping layer and the overflow controls
    // layers.
    overflow_controls_host_layer_ = GraphicsLayer::Create(this);

    // Clip iframe's overflow controls layer.
    bool container_masks_to_bounds = !layout_view_.GetFrame()->IsLocalRoot();
    overflow_controls_host_layer_->SetMasksToBounds(container_masks_to_bounds);

    // Create a clipping layer if this is an iframe or settings require to clip.
    container_layer_ = GraphicsLayer::Create(this);
    scroll_layer_ = GraphicsLayer::Create(this);
    if (ScrollingCoordinator* scrolling_coordinator =
            GetScrollingCoordinator()) {
      scrolling_coordinator->SetLayerIsContainerForFixedPositionLayers(
          scroll_layer_.get(), true);
    }

    // In RLS mode, LayoutView scrolling contents layer gets this element ID (in
    // CompositedLayerMapping::UpdateScrollingLayers).
    if (!RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      scroll_layer_->SetElementId(
          layout_view_.GetFrameView()->GetCompositorElementId());
    }

    // Hook them up
    overflow_controls_host_layer_->AddChild(container_layer_.get());
    container_layer_->AddChild(scroll_layer_.get());
    scroll_layer_->AddChild(root_content_layer_.get());

    FrameViewDidChangeSize();
  }

  AttachCompositorTimeline();
  AttachRootLayer();
}

void PaintLayerCompositor::DestroyRootLayer() {
  DetachRootLayer();

  if (layer_for_horizontal_scrollbar_) {
    layer_for_horizontal_scrollbar_->RemoveFromParent();
    layer_for_horizontal_scrollbar_ = nullptr;
    if (ScrollingCoordinator* scrolling_coordinator =
            GetScrollingCoordinator()) {
      scrolling_coordinator->ScrollableAreaScrollbarLayerDidChange(
          layout_view_.GetFrameView(), kHorizontalScrollbar);
    }
    layout_view_.GetFrameView()->SetScrollbarNeedsPaintInvalidation(
        kHorizontalScrollbar);
  }

  if (layer_for_vertical_scrollbar_) {
    layer_for_vertical_scrollbar_->RemoveFromParent();
    layer_for_vertical_scrollbar_ = nullptr;
    if (ScrollingCoordinator* scrolling_coordinator =
            GetScrollingCoordinator()) {
      scrolling_coordinator->ScrollableAreaScrollbarLayerDidChange(
          layout_view_.GetFrameView(), kVerticalScrollbar);
    }
    layout_view_.GetFrameView()->SetScrollbarNeedsPaintInvalidation(
        kVerticalScrollbar);
  }

  if (layer_for_scroll_corner_) {
    layer_for_scroll_corner_ = nullptr;
    layout_view_.GetFrameView()->SetScrollCornerNeedsPaintInvalidation();
  }

  if (overflow_controls_host_layer_) {
    overflow_controls_host_layer_ = nullptr;
    container_layer_ = nullptr;
    scroll_layer_ = nullptr;
  }
  DCHECK(!scroll_layer_);
  root_content_layer_ = nullptr;
}

void PaintLayerCompositor::AttachRootLayer() {
  // In Slimming Paint v2, PaintArtifactCompositor is responsible for the root
  // layer.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  if (layout_view_.GetFrame()->IsLocalRoot()) {
    root_layer_attachment_ = kRootLayerPendingAttachViaChromeClient;
  } else {
    HTMLFrameOwnerElement* owner_element =
        layout_view_.GetDocument().LocalOwner();
    DCHECK(owner_element);
    // The layer will get hooked up via
    // CompositedLayerMapping::updateGraphicsLayerConfiguration() for the
    // frame's layoutObject in the parent document.
    owner_element->SetNeedsCompositingUpdate();
    root_layer_attachment_ = kRootLayerAttachedViaEnclosingFrame;
  }
}

void PaintLayerCompositor::DetachRootLayer() {
  if (root_layer_attachment_ == kRootLayerUnattached)
    return;

  switch (root_layer_attachment_) {
    case kRootLayerAttachedViaEnclosingFrame: {
      // The layer will get unhooked up via
      // CompositedLayerMapping::updateGraphicsLayerConfiguration() for the
      // frame's layoutObject in the parent document.
      if (overflow_controls_host_layer_)
        overflow_controls_host_layer_->RemoveFromParent();

      if (HTMLFrameOwnerElement* owner_element =
              layout_view_.GetDocument().LocalOwner())
        owner_element->SetNeedsCompositingUpdate();
      break;
    }
    case kRootLayerAttachedViaChromeClient: {
      LocalFrame& frame = layout_view_.GetFrameView()->GetFrame();
      Page* page = frame.GetPage();
      if (!page)
        return;
      page->GetChromeClient().AttachRootGraphicsLayer(nullptr, &frame);
      break;
    }
    case kRootLayerPendingAttachViaChromeClient:
    case kRootLayerUnattached:
      break;
  }

  root_layer_attachment_ = kRootLayerUnattached;
}

void PaintLayerCompositor::AttachCompositorTimeline() {
  LocalFrame& frame = layout_view_.GetFrameView()->GetFrame();
  Page* page = frame.GetPage();
  if (!page || !frame.GetDocument())
    return;

  CompositorAnimationTimeline* compositor_timeline =
      frame.GetDocument()->Timeline().CompositorTimeline();
  if (compositor_timeline) {
    page->GetChromeClient().AttachCompositorAnimationTimeline(
        compositor_timeline, &frame);
  }
}

void PaintLayerCompositor::DetachCompositorTimeline() {
  LocalFrame& frame = layout_view_.GetFrameView()->GetFrame();
  Page* page = frame.GetPage();
  if (!page || !frame.GetDocument())
    return;

  CompositorAnimationTimeline* compositor_timeline =
      frame.GetDocument()->Timeline().CompositorTimeline();
  if (compositor_timeline) {
    page->GetChromeClient().DetachCompositorAnimationTimeline(
        compositor_timeline, &frame);
  }
}

ScrollingCoordinator* PaintLayerCompositor::GetScrollingCoordinator() const {
  if (Page* page = GetPage())
    return page->GetScrollingCoordinator();

  return nullptr;
}

Page* PaintLayerCompositor::GetPage() const {
  return layout_view_.GetFrameView()->GetFrame().GetPage();
}

DocumentLifecycle& PaintLayerCompositor::Lifecycle() const {
  return layout_view_.GetDocument().Lifecycle();
}

bool PaintLayerCompositor::IsMainFrame() const {
  return layout_view_.GetFrame()->IsMainFrame();
}

VisualViewport& PaintLayerCompositor::GetVisualViewport() const {
  return layout_view_.GetFrameView()->GetPage()->GetVisualViewport();
}

bool PaintLayerCompositor::IsRootScrollerAncestor() const {
  const TopDocumentRootScrollerController& global_rsc =
      layout_view_.GetDocument().GetPage()->GlobalRootScrollerController();
  PaintLayer* root_scroller_layer = global_rsc.RootScrollerPaintLayer();

  if (root_scroller_layer) {
    Frame* frame = root_scroller_layer->GetLayoutObject().GetFrame();
    while (frame) {
      if (frame->IsLocalFrame()) {
        PaintLayerCompositor* plc =
            ToLocalFrame(frame)->View()->GetLayoutView()->Compositor();
        if (plc == this)
          return true;
      }

      frame = frame->Tree().Parent();
    }
  }

  return false;
}

String PaintLayerCompositor::DebugName(
    const GraphicsLayer* graphics_layer) const {
  String name;
  if (graphics_layer == root_content_layer_.get()) {
    name = "Content Root Layer";
  } else if (graphics_layer == overflow_controls_host_layer_.get()) {
    name = "Frame Overflow Controls Host Layer";
  } else if (graphics_layer == layer_for_horizontal_scrollbar_.get()) {
    name = "Frame Horizontal Scrollbar Layer";
  } else if (graphics_layer == layer_for_vertical_scrollbar_.get()) {
    name = "Frame Vertical Scrollbar Layer";
  } else if (graphics_layer == layer_for_scroll_corner_.get()) {
    name = "Frame Scroll Corner Layer";
  } else if (graphics_layer == container_layer_.get()) {
    name = "Frame Clipping Layer";
  } else if (graphics_layer == scroll_layer_.get()) {
    name = "Frame Scrolling Layer";
  } else {
    NOTREACHED();
  }

  return name;
}

}  // namespace blink
