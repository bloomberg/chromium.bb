// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <set>

#include "base/containers/adapters.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "cc/base/synced_property.h"
#include "cc/input/page_scale_animation.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/layers/effect_tree_layer_list_iterator.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/resources/ui_resource_request.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/traced_value.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {
namespace {
// Small helper class that saves the current viewport location as the user sees
// it and resets to the same location.
class ViewportAnchor {
 public:
  ViewportAnchor(ScrollNode* inner_scroll,
                 ScrollNode* outer_scroll,
                 LayerTreeImpl* tree_impl)
      : inner_(inner_scroll), outer_(outer_scroll), tree_impl_(tree_impl) {
    viewport_in_content_coordinates_ =
        scroll_tree().current_scroll_offset(inner_->element_id);

    if (outer_) {
      viewport_in_content_coordinates_ +=
          scroll_tree().current_scroll_offset(outer_->element_id);
    }
  }

  void ResetViewportToAnchoredPosition() {
    DCHECK(outer_);

    scroll_tree().ClampScrollToMaxScrollOffset(*inner_, tree_impl_);
    scroll_tree().ClampScrollToMaxScrollOffset(*outer_, tree_impl_);

    gfx::ScrollOffset viewport_location =
        scroll_tree().current_scroll_offset(inner_->element_id) +
        scroll_tree().current_scroll_offset(outer_->element_id);

    gfx::Vector2dF delta =
        viewport_in_content_coordinates_.DeltaFrom(viewport_location);

    delta = scroll_tree().ScrollBy(*inner_, delta, tree_impl_);
    scroll_tree().ScrollBy(*outer_, delta, tree_impl_);
  }

 private:
  ScrollTree& scroll_tree() {
    return tree_impl_->property_trees()->scroll_tree;
  }

  ScrollNode* inner_;
  ScrollNode* outer_;
  LayerTreeImpl* tree_impl_;
  gfx::ScrollOffset viewport_in_content_coordinates_;
};

std::pair<gfx::PointF, gfx::PointF> GetVisibleSelectionEndPoints(
    const gfx::RectF& rect,
    const gfx::PointF& top,
    const gfx::PointF& bottom) {
  gfx::PointF start(base::ClampToRange(top.x(), rect.x(), rect.right()),
                    base::ClampToRange(top.y(), rect.y(), rect.bottom()));
  gfx::PointF end =
      start + gfx::Vector2dF(bottom.x() - top.x(), bottom.y() - top.y());
  return {start, end};
}

}  // namespace

void LayerTreeLifecycle::AdvanceTo(LifecycleState next_state) {
  switch (next_state) {
    case (kNotSyncing):
      DCHECK_EQ(state_, kLastSyncState);
      break;
    case (kBeginningSync):
    case (kSyncedPropertyTrees):
    case (kSyncedLayerProperties):
      // Only allow tree synchronization states to be transitioned in order.
      DCHECK_EQ(state_ + 1, next_state);
      break;
  }
  state_ = next_state;
}

LayerTreeImpl::LayerTreeImpl(
    LayerTreeHostImpl* host_impl,
    scoped_refptr<SyncedProperty<ScaleGroup>> page_scale_factor,
    scoped_refptr<SyncedBrowserControls> top_controls_shown_ratio,
    scoped_refptr<SyncedBrowserControls> bottom_controls_shown_ratio,
    scoped_refptr<SyncedElasticOverscroll> elastic_overscroll)
    : host_impl_(host_impl),
      source_frame_number_(-1),
      is_first_frame_after_commit_tracker_(-1),
      hud_layer_(nullptr),
      background_color_(0),
      last_scrolled_scroll_node_index_(ScrollTree::kInvalidNodeId),
      page_scale_factor_(page_scale_factor),
      min_page_scale_factor_(0),
      max_page_scale_factor_(0),
      external_page_scale_factor_(1.f),
      device_scale_factor_(1.f),
      painted_device_scale_factor_(1.f),
      elastic_overscroll_(elastic_overscroll),
      needs_update_draw_properties_(true),
      scrollbar_geometries_need_update_(false),
      needs_full_tree_sync_(true),
      needs_surface_ranges_sync_(false),
      next_activation_forces_redraw_(false),
      has_ever_been_drawn_(false),
      handle_visibility_changed_(false),
      have_scroll_event_handlers_(false),
      event_listener_properties_(),
      top_controls_shown_ratio_(std::move(top_controls_shown_ratio)),
      bottom_controls_shown_ratio_(std::move(bottom_controls_shown_ratio)) {
  property_trees()->is_main_thread = false;
}

LayerTreeImpl::~LayerTreeImpl() {
  // Need to explicitly clear the tree prior to destroying this so that
  // the LayerTreeImpl pointer is still valid in the LayerImpl dtor.
  DCHECK(LayerListIsEmpty());
}

void LayerTreeImpl::Shutdown() {
  DetachLayers();
  BreakSwapPromises(IsActiveTree() ? SwapPromise::SWAP_FAILS
                                   : SwapPromise::ACTIVATION_FAILS);
  DCHECK(LayerListIsEmpty());
}

void LayerTreeImpl::ReleaseResources() {
  for (auto* layer : *this)
    layer->ReleaseResources();
}

void LayerTreeImpl::OnPurgeMemory() {
  for (auto* layer : *this)
    layer->OnPurgeMemory();
}

void LayerTreeImpl::ReleaseTileResources() {
  for (auto* layer : *this)
    layer->ReleaseTileResources();
}

void LayerTreeImpl::RecreateTileResources() {
  for (auto* layer : *this)
    layer->RecreateTileResources();
}

void LayerTreeImpl::DidUpdateScrollOffset(ElementId id) {
  // Scrollbar positions depend on the current scroll offset.
  SetScrollbarGeometriesNeedUpdate();

  DCHECK(lifecycle().AllowsPropertyTreeAccess());
  ScrollTree& scroll_tree = property_trees()->scroll_tree;
  const auto* scroll_node = scroll_tree.FindNodeFromElementId(id);

  if (!scroll_node) {
    // A scroll node should always exist on the active tree but may not exist
    // if we're updating the other trees from the active tree. This can occur
    // when the pending tree represents a different page, for example.
    DCHECK(!IsActiveTree());
    return;
  }

  DCHECK(scroll_node->transform_id != TransformTree::kInvalidNodeId);
  TransformTree& transform_tree = property_trees()->transform_tree;
  auto* transform_node = transform_tree.Node(scroll_node->transform_id);
  if (transform_node->scroll_offset != scroll_tree.current_scroll_offset(id)) {
    transform_node->scroll_offset = scroll_tree.current_scroll_offset(id);
    transform_node->needs_local_transform_update = true;
    transform_tree.set_needs_update(true);
  }
  transform_node->transform_changed = true;
  property_trees()->changed = true;
  set_needs_update_draw_properties();

  if (IsActiveTree()) {
    // Ensure the other trees are kept in sync.
    if (host_impl_->pending_tree())
      host_impl_->pending_tree()->DidUpdateScrollOffset(id);
    if (host_impl_->recycle_tree())
      host_impl_->recycle_tree()->DidUpdateScrollOffset(id);
  }
}

void LayerTreeImpl::UpdateScrollbarGeometries() {
  if (!IsActiveTree())
    return;

  DCHECK(lifecycle().AllowsPropertyTreeAccess());

  // Layer properties such as bounds should be up-to-date.
  DCHECK(lifecycle().AllowsLayerPropertyAccess());

  if (!scrollbar_geometries_need_update_)
    return;

  for (auto& pair : element_id_to_scrollbar_layer_ids_) {
    ElementId scrolling_element_id = pair.first;

    auto& scroll_tree = property_trees()->scroll_tree;
    auto* scroll_node = scroll_tree.FindNodeFromElementId(scrolling_element_id);
    if (!scroll_node)
      continue;
    gfx::ScrollOffset current_offset =
        scroll_tree.current_scroll_offset(scrolling_element_id);
    gfx::SizeF scrolling_size(scroll_node->bounds);
    gfx::Size bounds_size(scroll_tree.container_bounds(scroll_node->id));

    bool is_viewport_scrollbar = scroll_node == InnerViewportScrollNode() ||
                                 scroll_node == OuterViewportScrollNode();
    if (is_viewport_scrollbar) {
      gfx::SizeF viewport_bounds(bounds_size);
      if (scroll_node == InnerViewportScrollNode()) {
        DCHECK_EQ(scroll_node, InnerViewportScrollNode());
        auto* outer_scroll_node = OuterViewportScrollNode();
        DCHECK(outer_scroll_node);

        // Add offset and bounds contribution of outer viewport.
        current_offset +=
            scroll_tree.current_scroll_offset(outer_scroll_node->element_id);
        gfx::SizeF outer_viewport_bounds(
            scroll_tree.container_bounds(outer_scroll_node->id));
        viewport_bounds.SetToMin(outer_viewport_bounds);
        // The scrolling size is only determined by the outer viewport.
        scrolling_size = gfx::SizeF(outer_scroll_node->bounds);
      } else {
        DCHECK_EQ(scroll_node, OuterViewportScrollNode());
        auto* inner_scroll_node = InnerViewportScrollNode();
        DCHECK(inner_scroll_node);
        // Add offset and bounds contribution of inner viewport.
        current_offset +=
            scroll_tree.current_scroll_offset(inner_scroll_node->element_id);
        gfx::SizeF inner_viewport_bounds(
            scroll_tree.container_bounds(inner_scroll_node->id));
        viewport_bounds.SetToMin(inner_viewport_bounds);
      }
      viewport_bounds.Scale(1 / current_page_scale_factor());
      bounds_size = ToCeiledSize(viewport_bounds);
    }

    for (auto* scrollbar : ScrollbarsFor(scrolling_element_id)) {
      if (scrollbar->orientation() == HORIZONTAL) {
        scrollbar->SetCurrentPos(current_offset.x());
        scrollbar->SetClipLayerLength(bounds_size.width());
        scrollbar->SetScrollLayerLength(scrolling_size.width());
      } else {
        scrollbar->SetCurrentPos(current_offset.y());
        scrollbar->SetClipLayerLength(bounds_size.height());
        scrollbar->SetScrollLayerLength(scrolling_size.height());
      }
      if (is_viewport_scrollbar) {
        scrollbar->SetVerticalAdjust(
            property_trees_.inner_viewport_container_bounds_delta().y());
      }
    }
  }

  scrollbar_geometries_need_update_ = false;
}

const RenderSurfaceImpl* LayerTreeImpl::RootRenderSurface() const {
  return property_trees_.effect_tree.GetRenderSurface(
      EffectTree::kContentsRootNodeId);
}

bool LayerTreeImpl::LayerListIsEmpty() const {
  return layer_list_.empty();
}

void LayerTreeImpl::SetRootLayerForTesting(std::unique_ptr<LayerImpl> layer) {
  DetachLayers();
  if (layer)
    AddLayer(std::move(layer));
  host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::OnCanDrawStateChangedForTree() {
  host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::InvalidateRegionForImages(
    const PaintImageIdFlatSet& images_to_invalidate) {
  TRACE_EVENT_BEGIN1("cc", "LayerTreeImpl::InvalidateRegionForImages",
                     "total_layer_count", picture_layers_.size());
  DCHECK(IsSyncTree());

  size_t no_images_count = 0;
  size_t no_invalidation_count = 0;
  size_t invalidated_count = 0;
  if (!images_to_invalidate.empty()) {
    // TODO(khushalsagar): It might be better to keep track of layers with
    // images and only iterate through those here.
    for (auto* picture_layer : picture_layers_) {
      auto result =
          picture_layer->InvalidateRegionForImages(images_to_invalidate);
      switch (result) {
        case PictureLayerImpl::ImageInvalidationResult::kNoImages:
          ++no_images_count;
          break;
        case PictureLayerImpl::ImageInvalidationResult::kNoInvalidation:
          ++no_invalidation_count;
          break;
        case PictureLayerImpl::ImageInvalidationResult::kInvalidated:
          ++invalidated_count;
          break;
      }
    }
  }
  TRACE_EVENT_END1(
      "cc", "LayerTreeImpl::InvalidateRegionForImages", "counts",
      base::StringPrintf("no_images[%zu] no_invalidaton[%zu] invalidated[%zu]",
                         no_images_count, no_invalidation_count,
                         invalidated_count));
}

void LayerTreeImpl::UpdateViewportContainerSizes() {
  if (!InnerViewportScrollNode())
    return;

  DCHECK(OuterViewportScrollNode());
  ViewportAnchor anchor(InnerViewportScrollNode(), OuterViewportScrollNode(),
                        this);

  float top_controls_shown_ratio =
      top_controls_shown_ratio_->Current(IsActiveTree());
  float bottom_controls_shown_ratio =
      bottom_controls_shown_ratio_->Current(IsActiveTree());
  float top_controls_layout_height = browser_controls_shrink_blink_size()
                                         ? top_controls_height()
                                         : top_controls_min_height();
  float top_content_offset =
      top_controls_height() > 0
          ? top_controls_height() * top_controls_shown_ratio
          : 0.f;
  float delta_from_top_controls =
      top_controls_layout_height - top_content_offset;
  float bottom_controls_layout_height = browser_controls_shrink_blink_size()
                                            ? bottom_controls_height()
                                            : bottom_controls_min_height();
  float bottom_content_offset =
      bottom_controls_height() > 0
          ? bottom_controls_height() * bottom_controls_shown_ratio
          : 0.f;
  delta_from_top_controls +=
      bottom_controls_layout_height - bottom_content_offset;

  // Adjust the viewport layers by shrinking/expanding the container to account
  // for changes in the size (e.g. browser controls) since the last resize from
  // Blink.
  auto* property_trees = this->property_trees();
  gfx::Vector2dF bounds_delta(0.f, delta_from_top_controls);
  if (property_trees->inner_viewport_container_bounds_delta() == bounds_delta)
    return;

  property_trees->SetInnerViewportContainerBoundsDelta(bounds_delta);

  // Adjust the outer viewport container as well, since adjusting only the
  // inner may cause its bounds to exceed those of the outer, causing scroll
  // clamping.
  gfx::Vector2dF scaled_bounds_delta =
      gfx::ScaleVector2d(bounds_delta, 1.f / min_page_scale_factor());

  property_trees->SetOuterViewportContainerBoundsDelta(scaled_bounds_delta);
  // outer_viewport_container_bounds_delta and
  // inner_viewport_scroll_bounds_delta are the same thing.
  DCHECK_EQ(scaled_bounds_delta,
            property_trees->inner_viewport_scroll_bounds_delta());

  if (auto* outer_clip_node = OuterViewportClipNode()) {
    float adjusted_container_height =
        OuterViewportScrollNode()->container_bounds.height() +
        scaled_bounds_delta.y();
    outer_clip_node->clip.set_height(adjusted_container_height);
  }

  anchor.ResetViewportToAnchoredPosition();

  property_trees->clip_tree.set_needs_update(true);
  property_trees->full_tree_damaged = true;
  set_needs_update_draw_properties();

  // Viewport scrollbar positions are determined using the viewport bounds
  // delta.
  SetScrollbarGeometriesNeedUpdate();
  set_needs_update_draw_properties();
}

bool LayerTreeImpl::IsRootLayer(const LayerImpl* layer) const {
  return !layer_list_.empty() && layer_list_[0].get() == layer;
}

gfx::ScrollOffset LayerTreeImpl::TotalScrollOffset() const {
  gfx::ScrollOffset offset;
  const auto& scroll_tree = property_trees()->scroll_tree;

  if (auto* inner_scroll = InnerViewportScrollNode()) {
    offset += scroll_tree.current_scroll_offset(inner_scroll->element_id);
    DCHECK(OuterViewportScrollNode());
    offset += scroll_tree.current_scroll_offset(
        OuterViewportScrollNode()->element_id);
  }

  return offset;
}

gfx::ScrollOffset LayerTreeImpl::TotalMaxScrollOffset() const {
  gfx::ScrollOffset offset;
  const auto& scroll_tree = property_trees()->scroll_tree;

  if (viewport_property_ids_.inner_scroll != ScrollTree::kInvalidNodeId)
    offset += scroll_tree.MaxScrollOffset(viewport_property_ids_.inner_scroll);

  if (viewport_property_ids_.outer_scroll != ScrollTree::kInvalidNodeId)
    offset += scroll_tree.MaxScrollOffset(viewport_property_ids_.outer_scroll);

  return offset;
}

OwnedLayerImplList LayerTreeImpl::DetachLayers() {
  render_surface_list_.clear();
  set_needs_update_draw_properties();
  return std::move(layer_list_);
}

OwnedLayerImplList LayerTreeImpl::DetachLayersKeepingRootLayerForTesting() {
  auto layers = DetachLayers();
  SetRootLayerForTesting(std::move(layers[0]));
  return layers;
}

void LayerTreeImpl::SetPropertyTrees(PropertyTrees* property_trees) {
  std::vector<std::unique_ptr<RenderSurfaceImpl>> old_render_surfaces;
  property_trees_.effect_tree.TakeRenderSurfaces(&old_render_surfaces);
  property_trees_ = *property_trees;
  bool render_surfaces_changed =
      property_trees_.effect_tree.CreateOrReuseRenderSurfaces(
          &old_render_surfaces, this);
  if (render_surfaces_changed)
    set_needs_update_draw_properties();
  property_trees->effect_tree.PushCopyRequestsTo(&property_trees_.effect_tree);
  property_trees_.is_main_thread = false;
  property_trees_.is_active = IsActiveTree();
  // The value of some effect node properties (like is_drawn) depends on
  // whether we are on the active tree or not. So, we need to update the
  // effect tree.
  if (IsActiveTree())
    property_trees_.effect_tree.set_needs_update(true);
}

void LayerTreeImpl::PushPropertyTreesTo(LayerTreeImpl* target_tree) {
  TRACE_EVENT0("cc", "LayerTreeImpl::PushPropertyTreesTo");
  // Property trees may store damage status. We preserve the active tree
  // damage status by pushing the damage status from active tree property
  // trees to pending tree property trees or by moving it onto the layers.
  if (target_tree->property_trees()->changed) {
    if (property_trees()->sequence_number ==
        target_tree->property_trees()->sequence_number)
      target_tree->property_trees()->PushChangeTrackingTo(property_trees());
    else
      target_tree->MoveChangeTrackingToLayers();
  }

  // To maintain the current scrolling node we need to use element ids which
  // are stable across the property tree update in SetPropertyTrees.
  ElementId scrolling_element_id;
  if (ScrollNode* scrolling_node = target_tree->CurrentlyScrollingNode())
    scrolling_element_id = scrolling_node->element_id;

  target_tree->SetPropertyTrees(&property_trees_);

  const ScrollNode* scrolling_node = nullptr;
  if (scrolling_element_id) {
    auto& scroll_tree = target_tree->property_trees()->scroll_tree;
    scrolling_node = scroll_tree.FindNodeFromElementId(scrolling_element_id);
  }
  target_tree->SetCurrentlyScrollingNode(scrolling_node);

  std::vector<EventMetrics> events_metrics;
  events_metrics.swap(events_metrics_from_main_thread_);
  target_tree->AppendEventsMetricsFromMainThread(std::move(events_metrics));
}

void LayerTreeImpl::PushSurfaceRangesTo(LayerTreeImpl* target_tree) {
  if (needs_surface_ranges_sync()) {
    target_tree->ClearSurfaceRanges();
    target_tree->SetSurfaceRanges(SurfaceRanges());
    // Reset for next update
    set_needs_surface_ranges_sync(false);
  }
}

void LayerTreeImpl::PushPropertiesTo(LayerTreeImpl* target_tree) {
  TRACE_EVENT0("cc", "LayerTreeImpl::PushPropertiesTo");
  // The request queue should have been processed and does not require a push.
  DCHECK_EQ(ui_resource_request_queue_.size(), 0u);

  PushSurfaceRangesTo(target_tree);
  target_tree->property_trees()->scroll_tree.PushScrollUpdatesFromPendingTree(
      &property_trees_, target_tree);

  if (next_activation_forces_redraw_) {
    target_tree->ForceRedrawNextActivation();
    next_activation_forces_redraw_ = false;
  }

  target_tree->PassSwapPromises(std::move(swap_promise_list_));
  swap_promise_list_.clear();

  // The page scale factor update can affect scrolling which requires that
  // these ids are set, so this must be before PushPageScaleFactorAndLimits.
  // Setting browser controls below also needs viewport scroll properties.
  target_tree->SetViewportPropertyIds(viewport_property_ids_);

  // Active tree already shares the page_scale_factor object with pending
  // tree so only the limits need to be provided.
  target_tree->PushPageScaleFactorAndLimits(nullptr, min_page_scale_factor(),
                                            max_page_scale_factor());
  target_tree->SetExternalPageScaleFactor(external_page_scale_factor_);

  target_tree->SetBrowserControlsParams(browser_controls_params_);
  target_tree->PushBrowserControls(nullptr, nullptr);

  target_tree->set_overscroll_behavior(overscroll_behavior_);

  target_tree->SetRasterColorSpace(raster_color_space_);
  target_tree->elastic_overscroll()->PushPendingToActive();

  target_tree->set_painted_device_scale_factor(painted_device_scale_factor());
  target_tree->SetDeviceScaleFactor(device_scale_factor());
  target_tree->SetDeviceViewportRect(device_viewport_rect_);

  if (TakeNewLocalSurfaceIdRequest())
    target_tree->RequestNewLocalSurfaceId();
  target_tree->SetLocalSurfaceIdAllocationFromParent(
      local_surface_id_allocation_from_parent());

  target_tree->pending_page_scale_animation_ =
      std::move(pending_page_scale_animation_);

  if (TakeForceSendMetadataRequest())
    target_tree->RequestForceSendMetadata();

  target_tree->RegisterSelection(selection_);

  // This should match the property synchronization in
  // LayerTreeHost::finishCommitOnImplThread().
  target_tree->set_source_frame_number(source_frame_number());
  target_tree->set_background_color(background_color());
  target_tree->set_have_scroll_event_handlers(have_scroll_event_handlers());
  target_tree->set_event_listener_properties(
      EventListenerClass::kTouchStartOrMove,
      event_listener_properties(EventListenerClass::kTouchStartOrMove));
  target_tree->set_event_listener_properties(
      EventListenerClass::kMouseWheel,
      event_listener_properties(EventListenerClass::kMouseWheel));
  target_tree->set_event_listener_properties(
      EventListenerClass::kTouchEndOrCancel,
      event_listener_properties(EventListenerClass::kTouchEndOrCancel));

  if (hud_layer())
    target_tree->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(
        target_tree->LayerById(hud_layer()->id())));
  else
    target_tree->set_hud_layer(nullptr);

  target_tree->has_ever_been_drawn_ = false;

  // Note: this needs to happen after SetPropertyTrees.
  target_tree->HandleTickmarksVisibilityChange();
  target_tree->HandleScrollbarShowRequestsFromMain();
  target_tree->AddPresentationCallbacks(std::move(presentation_callbacks_));
  presentation_callbacks_.clear();
}

void LayerTreeImpl::HandleTickmarksVisibilityChange() {
  if (!host_impl_->OuterViewportScrollNode())
    return;

  ScrollbarAnimationController* controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          host_impl_->OuterViewportScrollNode()->element_id);

  if (!controller)
    return;

  for (ScrollbarLayerImplBase* scrollbar : controller->Scrollbars()) {
    if (scrollbar->orientation() != VERTICAL)
      continue;

    // Android Overlay Scrollbar don't have FindInPage Tickmarks.
    if (scrollbar->GetScrollbarAnimator() != LayerTreeSettings::AURA_OVERLAY)
      DCHECK(!scrollbar->HasFindInPageTickmarks());

    controller->UpdateTickmarksVisibility(scrollbar->HasFindInPageTickmarks());
  }
}

void LayerTreeImpl::HandleScrollbarShowRequestsFromMain() {
  for (auto* layer : *this) {
    if (!layer->needs_show_scrollbars())
      continue;
    ScrollbarAnimationController* controller =
        host_impl_->ScrollbarAnimationControllerForElementId(
            layer->element_id());
    if (controller) {
      controller->DidRequestShowFromMainThread();
      layer->set_needs_show_scrollbars(false);
    }
  }
}

void LayerTreeImpl::MoveChangeTrackingToLayers() {
  // We need to update the change tracking on property trees before we move it
  // onto the layers.
  property_trees_.UpdateChangeTracking();
  for (auto* layer : *this) {
    if (layer->LayerPropertyChangedFromPropertyTrees())
      layer->NoteLayerPropertyChangedFromPropertyTrees();
  }
  EffectTree& effect_tree = property_trees_.effect_tree;
  for (int id = EffectTree::kContentsRootNodeId;
       id < static_cast<int>(effect_tree.size()); ++id) {
    RenderSurfaceImpl* render_surface = effect_tree.GetRenderSurface(id);
    if (render_surface && render_surface->AncestorPropertyChanged())
      render_surface->NoteAncestorPropertyChanged();
  }
}

void LayerTreeImpl::ForceRecalculateRasterScales() {
  for (auto* layer : picture_layers_)
    layer->ResetRasterScale();
}

bool LayerTreeImpl::IsElementInPropertyTree(ElementId element_id) const {
  return property_trees()->HasElement(element_id);
}

ElementListType LayerTreeImpl::GetElementTypeForAnimation() const {
  return IsActiveTree() ? ElementListType::ACTIVE : ElementListType::PENDING;
}

void LayerTreeImpl::AddToElementLayerList(ElementId element_id,
                                          LayerImpl* layer) {
  if (!element_id)
    return;

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("layer-element"),
               "LayerTreeImpl::AddToElementLayerList", "element",
               element_id.ToString());

  if (!settings().use_layer_lists) {
    host_impl_->mutator_host()->RegisterElementId(element_id,
                                                  GetElementTypeForAnimation());
  }
}

void LayerTreeImpl::RemoveFromElementLayerList(ElementId element_id) {
  if (!element_id)
    return;

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("layer-element"),
               "LayerTreeImpl::RemoveFromElementLayerList", "element",
               element_id.ToString());

  if (!settings().use_layer_lists) {
    host_impl_->mutator_host()->UnregisterElementId(
        element_id, GetElementTypeForAnimation());
  }
}

void LayerTreeImpl::SetTransformMutated(ElementId element_id,
                                        const gfx::Transform& transform) {
  DCHECK_EQ(1u, property_trees()->element_id_to_transform_node_index.count(
                    element_id));
  if (IsSyncTree() || IsRecycleTree())
    element_id_to_transform_animations_[element_id] = transform;
  if (property_trees()->transform_tree.OnTransformAnimated(element_id,
                                                           transform))
    set_needs_update_draw_properties();
}

void LayerTreeImpl::SetOpacityMutated(ElementId element_id, float opacity) {
  DCHECK_EQ(
      1u, property_trees()->element_id_to_effect_node_index.count(element_id));
  if (IsSyncTree() || IsRecycleTree())
    element_id_to_opacity_animations_[element_id] = opacity;
  if (property_trees()->effect_tree.OnOpacityAnimated(element_id, opacity))
    set_needs_update_draw_properties();
}

void LayerTreeImpl::SetFilterMutated(ElementId element_id,
                                     const FilterOperations& filters) {
  DCHECK_EQ(
      1u, property_trees()->element_id_to_effect_node_index.count(element_id));
  if (IsSyncTree() || IsRecycleTree())
    element_id_to_filter_animations_[element_id] = filters;
  if (property_trees()->effect_tree.OnFilterAnimated(element_id, filters))
    set_needs_update_draw_properties();
}

void LayerTreeImpl::SetBackdropFilterMutated(
    ElementId element_id,
    const FilterOperations& backdrop_filters) {
  DCHECK_EQ(
      1u, property_trees()->element_id_to_effect_node_index.count(element_id));
  if (IsSyncTree() || IsRecycleTree())
    element_id_to_backdrop_filter_animations_[element_id] = backdrop_filters;
  if (property_trees()->effect_tree.OnBackdropFilterAnimated(element_id,
                                                             backdrop_filters))
    set_needs_update_draw_properties();
}

void LayerTreeImpl::AddPresentationCallbacks(
    std::vector<LayerTreeHost::PresentationTimeCallback> callbacks) {
  std::copy(std::make_move_iterator(callbacks.begin()),
            std::make_move_iterator(callbacks.end()),
            std::back_inserter(presentation_callbacks_));
}

std::vector<LayerTreeHost::PresentationTimeCallback>
LayerTreeImpl::TakePresentationCallbacks() {
  std::vector<LayerTreeHost::PresentationTimeCallback> callbacks;
  callbacks.swap(presentation_callbacks_);
  return callbacks;
}

LayerImpl* LayerTreeImpl::InnerViewportScrollLayerForTesting() const {
  if (auto* scroll_node = InnerViewportScrollNode())
    return LayerByElementId(scroll_node->element_id);
  return nullptr;
}

LayerImpl* LayerTreeImpl::OuterViewportScrollLayerForTesting() const {
  if (auto* scroll_node = OuterViewportScrollNode())
    return LayerByElementId(scroll_node->element_id);
  return nullptr;
}

ScrollNode* LayerTreeImpl::CurrentlyScrollingNode() {
  DCHECK(IsActiveTree());
  return property_trees_.scroll_tree.CurrentlyScrollingNode();
}

const ScrollNode* LayerTreeImpl::CurrentlyScrollingNode() const {
  return property_trees_.scroll_tree.CurrentlyScrollingNode();
}

int LayerTreeImpl::LastScrolledScrollNodeIndex() const {
  return last_scrolled_scroll_node_index_;
}

void LayerTreeImpl::SetCurrentlyScrollingNode(const ScrollNode* node) {
  if (node)
    last_scrolled_scroll_node_index_ = node->id;

  ScrollTree& scroll_tree = property_trees()->scroll_tree;
  ScrollNode* old_node = scroll_tree.CurrentlyScrollingNode();

  ElementId old_element_id = old_node ? old_node->element_id : ElementId();
  ElementId new_element_id = node ? node->element_id : ElementId();
  if (old_element_id == new_element_id)
    return;

  scroll_tree.set_currently_scrolling_node(node ? node->id
                                                : ScrollTree::kInvalidNodeId);
}

void LayerTreeImpl::ClearCurrentlyScrollingNode() {
  SetCurrentlyScrollingNode(nullptr);
}

float LayerTreeImpl::ClampPageScaleFactorToLimits(
    float page_scale_factor) const {
  if (min_page_scale_factor_ && page_scale_factor < min_page_scale_factor_)
    page_scale_factor = min_page_scale_factor_;
  else if (max_page_scale_factor_ && page_scale_factor > max_page_scale_factor_)
    page_scale_factor = max_page_scale_factor_;
  return page_scale_factor;
}

void LayerTreeImpl::UpdatePropertyTreeAnimationFromMainThread() {
  // TODO(enne): This should get replaced by pulling out animations into their
  // own trees.  Then animations would have their own ways of synchronizing
  // across commits.  This occurs to push updates from animations that have
  // ticked since begin frame to a newly-committed property tree.
  if (layer_list_.empty())
    return;

  // Note we lazily delete element ids from the |element_id_to_xxx|
  // maps below if we find they have no node present in their
  // respective tree. This can be the case if the layer associated
  // with that element id has been removed.

  // This code is assumed to only run on the sync tree; the node updates are
  // then synced when the tree is activated. See http://crbug.com/916512
  DCHECK(IsSyncTree());

  auto element_id_to_opacity = element_id_to_opacity_animations_.begin();
  while (element_id_to_opacity != element_id_to_opacity_animations_.end()) {
    const ElementId id = element_id_to_opacity->first;
    EffectNode* node = property_trees_.effect_tree.FindNodeFromElementId(id);
    if (!node || !node->is_currently_animating_opacity ||
        node->opacity == element_id_to_opacity->second) {
      element_id_to_opacity_animations_.erase(element_id_to_opacity++);
      continue;
    }
    node->opacity = element_id_to_opacity->second;
    property_trees_.effect_tree.set_needs_update(true);
    ++element_id_to_opacity;
  }

  auto element_id_to_filter = element_id_to_filter_animations_.begin();
  while (element_id_to_filter != element_id_to_filter_animations_.end()) {
    const ElementId id = element_id_to_filter->first;
    EffectNode* node = property_trees_.effect_tree.FindNodeFromElementId(id);
    if (!node || !node->is_currently_animating_filter ||
        node->filters == element_id_to_filter->second) {
      element_id_to_filter_animations_.erase(element_id_to_filter++);
      continue;
    }
    node->filters = element_id_to_filter->second;
    property_trees_.effect_tree.set_needs_update(true);
    ++element_id_to_filter;
  }

  auto element_id_to_backdrop_filter =
      element_id_to_backdrop_filter_animations_.begin();
  while (element_id_to_backdrop_filter !=
         element_id_to_backdrop_filter_animations_.end()) {
    const ElementId id = element_id_to_backdrop_filter->first;
    EffectNode* node = property_trees_.effect_tree.FindNodeFromElementId(id);
    if (!node || !node->is_currently_animating_backdrop_filter ||
        node->backdrop_filters == element_id_to_backdrop_filter->second) {
      element_id_to_backdrop_filter_animations_.erase(
          element_id_to_backdrop_filter++);
      continue;
    }
    node->backdrop_filters = element_id_to_backdrop_filter->second;
    property_trees_.effect_tree.set_needs_update(true);
    ++element_id_to_backdrop_filter;
  }

  auto element_id_to_transform = element_id_to_transform_animations_.begin();
  while (element_id_to_transform != element_id_to_transform_animations_.end()) {
    const ElementId id = element_id_to_transform->first;
    TransformNode* node =
        property_trees_.transform_tree.FindNodeFromElementId(id);
    if (!node || !node->is_currently_animating ||
        node->local == element_id_to_transform->second) {
      element_id_to_transform_animations_.erase(element_id_to_transform++);
      continue;
    }
    node->local = element_id_to_transform->second;
    node->needs_local_transform_update = true;
    property_trees_.transform_tree.set_needs_update(true);
    ++element_id_to_transform;
  }

  for (auto transform_it : property_trees()->element_id_to_transform_node_index)
    UpdateTransformAnimation(transform_it.first, transform_it.second);
}

void LayerTreeImpl::UpdateTransformAnimation(ElementId element_id,
                                             int transform_node_index) {
  // This includes all animations, even those that are finished but
  // haven't yet been deleted.
  if (mutator_host()->HasAnyAnimationTargetingProperty(
          element_id, TargetProperty::TRANSFORM)) {
    TransformTree& transform_tree = property_trees()->transform_tree;
    if (TransformNode* node = transform_tree.Node(transform_node_index)) {
      ElementListType list_type = GetElementTypeForAnimation();
      bool has_potential_animation =
          mutator_host()->HasPotentiallyRunningTransformAnimation(element_id,
                                                                  list_type);
      if (node->has_potential_animation != has_potential_animation) {
        node->has_potential_animation = has_potential_animation;
        mutator_host()->GetAnimationScales(element_id, list_type,
                                           &node->maximum_animation_scale,
                                           &node->starting_animation_scale);
        transform_tree.set_needs_update(true);
        set_needs_update_draw_properties();
      }
    }
  }
}

void LayerTreeImpl::UpdatePageScaleNode() {
  if (!PageScaleTransformNode()) {
    DCHECK(layer_list_.empty() || current_page_scale_factor() == 1);
    return;
  }
  draw_property_utils::UpdatePageScaleFactor(
      property_trees(), PageScaleTransformNode(), current_page_scale_factor());
}

void LayerTreeImpl::SetPageScaleOnActiveTree(float active_page_scale) {
  DCHECK(IsActiveTree());
  DCHECK(lifecycle().AllowsPropertyTreeAccess());
  float clamped_page_scale = ClampPageScaleFactorToLimits(active_page_scale);
  // Temporary crash logging for https://crbug.com/845097.
  static bool has_dumped_without_crashing = false;
  if (host_impl_->settings().is_layer_tree_for_subframe &&
      clamped_page_scale != 1.f && !has_dumped_without_crashing) {
    has_dumped_without_crashing = true;
    static auto* psf_oopif_error = base::debug::AllocateCrashKeyString(
        "psf_oopif_error", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        psf_oopif_error, base::StringPrintf("%f", clamped_page_scale));
    base::debug::DumpWithoutCrashing();
  }
  if (page_scale_factor()->SetCurrent(clamped_page_scale)) {
    DidUpdatePageScale();
    UpdatePageScaleNode();
  }
}

void LayerTreeImpl::PushPageScaleFromMainThread(float page_scale_factor,
                                                float min_page_scale_factor,
                                                float max_page_scale_factor) {
  PushPageScaleFactorAndLimits(&page_scale_factor, min_page_scale_factor,
                               max_page_scale_factor);
}

void LayerTreeImpl::PushPageScaleFactorAndLimits(const float* page_scale_factor,
                                                 float min_page_scale_factor,
                                                 float max_page_scale_factor) {
  DCHECK(page_scale_factor || IsActiveTree());
  bool changed_page_scale = false;

  changed_page_scale |=
      SetPageScaleFactorLimits(min_page_scale_factor, max_page_scale_factor);

  if (page_scale_factor) {
    DCHECK(!IsActiveTree() || !host_impl_->pending_tree());
    changed_page_scale |=
        page_scale_factor_->PushMainToPending(*page_scale_factor);
  }

  if (IsActiveTree()) {
    changed_page_scale |= page_scale_factor_->PushPendingToActive();
  }

  if (changed_page_scale)
    DidUpdatePageScale();

  DCHECK(lifecycle().AllowsPropertyTreeAccess());
  if (page_scale_factor)
    UpdatePageScaleNode();
}

void LayerTreeImpl::SetBrowserControlsParams(
    const BrowserControlsParams& params) {
  if (browser_controls_params_ == params)
    return;

  browser_controls_params_ = params;
  UpdateViewportContainerSizes();

  if (IsActiveTree()) {
    host_impl_->browser_controls_manager()->OnBrowserControlsParamsChanged(
        params.animate_browser_controls_height_changes);
  }
}

void LayerTreeImpl::set_overscroll_behavior(
    const OverscrollBehavior& behavior) {
  overscroll_behavior_ = behavior;
}

bool LayerTreeImpl::ClampTopControlsShownRatio() {
  float ratio = top_controls_shown_ratio_->Current(true);
  auto range = std::make_pair(0.f, 1.f);
  if (IsActiveTree()) {
    // BCOM might need to set ratios outside the [0, 1] range (e.g. animation
    // running). So, use the values it provides instead of clamping to [0, 1].
    range =
        host_impl_->browser_controls_manager()->TopControlsShownRatioRange();
  }
  return top_controls_shown_ratio_->SetCurrent(
      base::ClampToRange(ratio, range.first, range.second));
}

bool LayerTreeImpl::ClampBottomControlsShownRatio() {
  float ratio = bottom_controls_shown_ratio_->Current(true);
  auto range = std::make_pair(0.f, 1.f);
  if (IsActiveTree()) {
    // BCOM might need to set ratios outside the [0, 1] range (e.g. animation
    // running). So, use the values it provides instead of clamping to [0, 1].
    range =
        host_impl_->browser_controls_manager()->BottomControlsShownRatioRange();
  }
  return bottom_controls_shown_ratio_->SetCurrent(
      base::ClampToRange(ratio, range.first, range.second));
}

bool LayerTreeImpl::SetCurrentBrowserControlsShownRatio(float top_ratio,
                                                        float bottom_ratio) {
  TRACE_EVENT2("cc", "LayerTreeImpl::SetCurrentBrowserControlsShownRatio",
               "top_ratio", top_ratio, "bottom_ratio", bottom_ratio);
  bool changed = top_controls_shown_ratio_->SetCurrent(top_ratio);
  changed |= ClampTopControlsShownRatio();
  changed |= bottom_controls_shown_ratio_->SetCurrent(bottom_ratio);
  changed |= ClampBottomControlsShownRatio();
  return changed;
}

void LayerTreeImpl::PushBrowserControlsFromMainThread(
    float top_controls_shown_ratio,
    float bottom_controls_shown_ratio) {
  PushBrowserControls(&top_controls_shown_ratio, &bottom_controls_shown_ratio);
}

void LayerTreeImpl::PushBrowserControls(
    const float* top_controls_shown_ratio,
    const float* bottom_controls_shown_ratio) {
  DCHECK(top_controls_shown_ratio || bottom_controls_shown_ratio ||
         IsActiveTree());
  DCHECK(!top_controls_shown_ratio || bottom_controls_shown_ratio);
  DCHECK(top_controls_shown_ratio || IsActiveTree());

  if (top_controls_shown_ratio) {
    DCHECK(!IsActiveTree() || !host_impl_->pending_tree());
    bool changed_pending =
        top_controls_shown_ratio_->PushMainToPending(*top_controls_shown_ratio);
    changed_pending |= bottom_controls_shown_ratio_->PushMainToPending(
        *bottom_controls_shown_ratio);
    if (!IsActiveTree() && changed_pending)
      UpdateViewportContainerSizes();
  }
  if (IsActiveTree()) {
    bool changed_active = top_controls_shown_ratio_->PushPendingToActive();
    changed_active |= ClampTopControlsShownRatio();
    changed_active |= bottom_controls_shown_ratio_->PushPendingToActive();
    changed_active |= ClampBottomControlsShownRatio();
    if (changed_active)
      host_impl_->DidChangeBrowserControlsPosition();
  }
}

bool LayerTreeImpl::SetPageScaleFactorLimits(float min_page_scale_factor,
                                             float max_page_scale_factor) {
  if (min_page_scale_factor == min_page_scale_factor_ &&
      max_page_scale_factor == max_page_scale_factor_)
    return false;

  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;

  return true;
}

void LayerTreeImpl::DidUpdatePageScale() {
  if (IsActiveTree())
    page_scale_factor()->SetCurrent(
        ClampPageScaleFactorToLimits(current_page_scale_factor()));

  set_needs_update_draw_properties();

  // Viewport scrollbar sizes depend on the page scale factor.
  SetScrollbarGeometriesNeedUpdate();

  if (IsActiveTree()) {
    if (settings().scrollbar_flash_after_any_scroll_update) {
      host_impl_->FlashAllScrollbars(true);
      return;
    }
    if (auto* scroll_node = host_impl_->OuterViewportScrollNode()) {
      if (ScrollbarAnimationController* controller =
              host_impl_->ScrollbarAnimationControllerForElementId(
                  scroll_node->element_id))
        controller->DidScrollUpdate();
    }
  }
}

void LayerTreeImpl::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor == device_scale_factor_)
    return;
  device_scale_factor_ = device_scale_factor;

  set_needs_update_draw_properties();
  if (IsActiveTree())
    host_impl_->SetViewportDamage(GetDeviceViewport());
  host_impl_->SetNeedUpdateGpuRasterizationStatus();
}

void LayerTreeImpl::SetLocalSurfaceIdAllocationFromParent(
    const viz::LocalSurfaceIdAllocation&
        local_surface_id_allocation_from_parent) {
  local_surface_id_allocation_from_parent_ =
      local_surface_id_allocation_from_parent;
}

void LayerTreeImpl::RequestNewLocalSurfaceId() {
  new_local_surface_id_request_ = true;
}

bool LayerTreeImpl::TakeNewLocalSurfaceIdRequest() {
  bool new_local_surface_id_request = new_local_surface_id_request_;
  new_local_surface_id_request_ = false;
  return new_local_surface_id_request;
}

void LayerTreeImpl::SetDeviceViewportRect(
    const gfx::Rect& device_viewport_rect) {
  if (device_viewport_rect == device_viewport_rect_)
    return;
  device_viewport_rect_ = device_viewport_rect;

  set_needs_update_draw_properties();
  if (!IsActiveTree())
    return;

  UpdateViewportContainerSizes();
  host_impl_->OnCanDrawStateChangedForTree();
  host_impl_->SetViewportDamage(GetDeviceViewport());
}

gfx::Rect LayerTreeImpl::GetDeviceViewport() const {
  // TODO(fsamuel): We should plumb |external_viewport| similar to the
  // way we plumb |device_viewport_rect_|.
  const gfx::Rect& external_viewport = host_impl_->external_viewport();
  if (external_viewport.IsEmpty())
    return device_viewport_rect_;
  return external_viewport;
}

void LayerTreeImpl::SetRasterColorSpace(
    const gfx::ColorSpace& raster_color_space) {
  if (raster_color_space == raster_color_space_)
    return;
  raster_color_space_ = raster_color_space;
}

void LayerTreeImpl::SetExternalPageScaleFactor(
    float external_page_scale_factor) {
  if (external_page_scale_factor_ == external_page_scale_factor)
    return;

  external_page_scale_factor_ = external_page_scale_factor;
  DidUpdatePageScale();
}

SyncedProperty<ScaleGroup>* LayerTreeImpl::page_scale_factor() {
  return page_scale_factor_.get();
}

const SyncedProperty<ScaleGroup>* LayerTreeImpl::page_scale_factor() const {
  return page_scale_factor_.get();
}

gfx::SizeF LayerTreeImpl::ScrollableViewportSize() const {
  auto* inner_node = InnerViewportScrollNode();
  if (!inner_node)
    return gfx::SizeF();

  return gfx::ScaleSize(gfx::SizeF(inner_node->container_bounds),
                        1.0f / page_scale_factor_for_scroll());
}

gfx::Rect LayerTreeImpl::RootScrollLayerDeviceViewportBounds() const {
  const ScrollNode* root_scroll_node = OuterViewportScrollNode();
  if (!root_scroll_node) {
    DCHECK(!InnerViewportScrollNode());
    return gfx::Rect();
  }
  return MathUtil::MapEnclosingClippedRect(
      property_trees()->transform_tree.ToScreen(root_scroll_node->transform_id),
      gfx::Rect(root_scroll_node->bounds));
}

void LayerTreeImpl::ApplySentScrollAndScaleDeltasFromAbortedCommit() {
  DCHECK(IsActiveTree());

  page_scale_factor()->AbortCommit();
  top_controls_shown_ratio()->AbortCommit();
  elastic_overscroll()->AbortCommit();

  if (layer_list_.empty())
    return;

  property_trees()->scroll_tree.ApplySentScrollDeltasFromAbortedCommit();
}

void LayerTreeImpl::SetViewportPropertyIds(const ViewportPropertyIds& ids) {
  viewport_property_ids_ = ids;
  // Outer viewport properties exist only if inner viewport property exists.
  DCHECK(ids.inner_scroll != ScrollTree::kInvalidNodeId ||
         (ids.outer_scroll == ScrollTree::kInvalidNodeId &&
          ids.outer_clip == ClipTree::kInvalidNodeId));

  if (auto* inner_scroll = InnerViewportScrollNode()) {
    if (auto* inner_scroll_layer = LayerByElementId(inner_scroll->element_id))
      inner_scroll_layer->set_is_inner_viewport_scroll_layer();
  }
}

const TransformNode* LayerTreeImpl::OverscrollElasticityTransformNode() const {
  return property_trees()->transform_tree.Node(
      viewport_property_ids_.overscroll_elasticity_transform);
}

const TransformNode* LayerTreeImpl::PageScaleTransformNode() const {
  return property_trees()->transform_tree.Node(
      viewport_property_ids_.page_scale_transform);
}

const ScrollNode* LayerTreeImpl::InnerViewportScrollNode() const {
  return property_trees()->scroll_tree.Node(
      viewport_property_ids_.inner_scroll);
}

const ClipNode* LayerTreeImpl::OuterViewportClipNode() const {
  return property_trees()->clip_tree.Node(viewport_property_ids_.outer_clip);
}

const ScrollNode* LayerTreeImpl::OuterViewportScrollNode() const {
  return property_trees()->scroll_tree.Node(
      viewport_property_ids_.outer_scroll);
}

// For unit tests, we use the layer's id as its element id.
void LayerTreeImpl::SetElementIdsForTesting() {
  for (auto* layer : *this) {
    if (!layer->element_id())
      layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
  }
}

bool LayerTreeImpl::UpdateDrawProperties(
    bool update_image_animation_controller,
    LayerImplList* output_update_layer_list_for_testing) {
  if (!needs_update_draw_properties_)
    return true;

  TRACE_EVENT0("cc,benchmark", "LayerTreeImpl::UpdateDrawProperties");

  // Ensure the scrollbar geometries are up-to-date for hit testing and quads
  // generation. This may cause damage on the scrollbar layers which is why
  // it occurs before we reset |needs_update_draw_properties_|.
  UpdateScrollbarGeometries();

  // Calling UpdateDrawProperties must clear this flag, so there can be no
  // early outs before this.
  needs_update_draw_properties_ = false;

  // For max_texture_size. When a new output surface is received the needs
  // update draw properties flag is set again.
  if (!host_impl_->layer_tree_frame_sink())
    return false;

  // Clear this after the renderer early out, as it should still be
  // possible to hit test even without a renderer.
  render_surface_list_.clear();

  if (layer_list_.empty())
    return false;

  {
    base::ElapsedTimer timer;
    TRACE_EVENT2("cc,benchmark",
                 "LayerTreeImpl::UpdateDrawProperties::CalculateDrawProperties",
                 "IsActive", IsActiveTree(), "SourceFrameNumber",
                 source_frame_number_);
    // We verify visible rect calculations whenever we verify clip tree
    // calculations except when this function is explicitly passed a flag asking
    // us to skip it.
    draw_property_utils::CalculateDrawProperties(
        this, &render_surface_list_, output_update_layer_list_for_testing);

    if (const char* client_name = GetClientNameForMetrics()) {
      UMA_HISTOGRAM_COUNTS_1M(
          base::StringPrintf(
              "Compositing.%s.LayerTreeImpl.CalculateDrawPropertiesUs",
              client_name),
          timer.Elapsed().InMicroseconds());
      UMA_HISTOGRAM_COUNTS_100(
          base::StringPrintf("Compositing.%s.NumRenderSurfaces", client_name),
          base::saturated_cast<int>(render_surface_list_.size()));
    }
  }

  if (settings().enable_occlusion) {
    TRACE_EVENT2("cc,benchmark",
                 "LayerTreeImpl::UpdateDrawProperties::Occlusion", "IsActive",
                 IsActiveTree(), "SourceFrameNumber", source_frame_number_);
    OcclusionTracker occlusion_tracker(RootRenderSurface()->content_rect());
    occlusion_tracker.set_minimum_tracking_size(
        settings().minimum_occlusion_tracking_size);

    for (EffectTreeLayerListIterator it(this);
         it.state() != EffectTreeLayerListIterator::State::END; ++it) {
      occlusion_tracker.EnterLayer(it);

      if (it.state() == EffectTreeLayerListIterator::State::LAYER) {
        LayerImpl* layer = it.current_layer();
        layer->draw_properties().occlusion_in_content_space =
            occlusion_tracker.GetCurrentOcclusionForLayer(
                layer->DrawTransform());
      }

      if (it.state() ==
          EffectTreeLayerListIterator::State::CONTRIBUTING_SURFACE) {
        const RenderSurfaceImpl* occlusion_surface =
            occlusion_tracker.OcclusionSurfaceForContributingSurface();
        gfx::Transform draw_transform;
        RenderSurfaceImpl* render_surface = it.current_render_surface();
        if (occlusion_surface) {
          // We are calculating transform between two render surfaces. So, we
          // need to apply the surface contents scale at target and remove the
          // surface contents scale at source.
          property_trees()->GetToTarget(render_surface->TransformTreeIndex(),
                                        occlusion_surface->EffectTreeIndex(),
                                        &draw_transform);
          const EffectNode* effect_node = property_trees()->effect_tree.Node(
              render_surface->EffectTreeIndex());
          draw_property_utils::ConcatInverseSurfaceContentsScale(
              effect_node, &draw_transform);
        }

        Occlusion occlusion =
            occlusion_tracker.GetCurrentOcclusionForContributingSurface(
                draw_transform);
        render_surface->set_occlusion_in_content_space(occlusion);
      }

      occlusion_tracker.LeaveLayer(it);
    }

    unoccluded_screen_space_region_ =
        occlusion_tracker.ComputeVisibleRegionInScreen(this);
  } else {
    // No occlusion, entire root content rect is unoccluded.
    unoccluded_screen_space_region_ =
        Region(RootRenderSurface()->content_rect());
  }

  // Resourceless draw do not need tiles and should not affect existing tile
  // priorities.
  if (!is_in_resourceless_software_draw_mode()) {
    TRACE_EVENT_BEGIN2(
        "cc,benchmark", "LayerTreeImpl::UpdateDrawProperties::UpdateTiles",
        "IsActive", IsActiveTree(), "SourceFrameNumber", source_frame_number_);
    size_t layers_updated_count = 0;
    bool tile_priorities_updated = false;
    for (PictureLayerImpl* layer : picture_layers_) {
      if (!layer->HasValidTilePriorities())
        continue;
      ++layers_updated_count;
      tile_priorities_updated |= layer->UpdateTiles();
    }

    if (tile_priorities_updated)
      DidModifyTilePriorities();

    TRACE_EVENT_END1("cc,benchmark",
                     "LayerTreeImpl::UpdateDrawProperties::UpdateTiles",
                     "layers_updated_count", layers_updated_count);
  }

  if (update_image_animation_controller && image_animation_controller()) {
    image_animation_controller()->UpdateStateFromDrivers();
  }

  DCHECK(!needs_update_draw_properties_)
      << "CalcDrawProperties should not set_needs_update_draw_properties()";
  return true;
}

void LayerTreeImpl::UpdateCanUseLCDText() {
  // If this is not the sync tree, then it is not safe to update lcd text
  // as it causes invalidations and the tiles may be in use.
  DCHECK(IsSyncTree());
  bool tile_priorities_updated = false;
  for (auto* layer : picture_layers_)
    tile_priorities_updated |= layer->UpdateCanUseLCDTextAfterCommit();
  if (tile_priorities_updated)
    DidModifyTilePriorities();
}

const RenderSurfaceList& LayerTreeImpl::GetRenderSurfaceList() const {
  // If this assert triggers, then the list is dirty.
  DCHECK(!needs_update_draw_properties_);
  return render_surface_list_;
}

const Region& LayerTreeImpl::UnoccludedScreenSpaceRegion() const {
  // If this assert triggers, then the render_surface_list_ is dirty, so the
  // unoccluded_screen_space_region_ is not valid anymore.
  DCHECK(!needs_update_draw_properties_);
  return unoccluded_screen_space_region_;
}

gfx::SizeF LayerTreeImpl::ScrollableSize() const {
  auto* scroll_node = OuterViewportScrollNode();
  if (!scroll_node) {
    DCHECK(!InnerViewportScrollNode());
    return gfx::SizeF();
  }
  const auto& scroll_tree = property_trees()->scroll_tree;
  auto size = scroll_tree.scroll_bounds(scroll_node->id);
  size.SetToMax(gfx::SizeF(scroll_tree.container_bounds(scroll_node->id)));
  return size;
}

LayerImpl* LayerTreeImpl::LayerById(int id) const {
  auto iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : nullptr;
}

// TODO(masonfreed): If this shows up on profiles, this could use
// a layer_element_map_ approach similar to LayerById().
LayerImpl* LayerTreeImpl::LayerByElementId(ElementId element_id) const {
  auto it =
      std::find_if(rbegin(), rend(), [&element_id](LayerImpl* layer_impl) {
        return layer_impl->element_id() == element_id;
      });
  if (it == rend())
    return nullptr;
  return *it;
}

void LayerTreeImpl::SetSurfaceRanges(
    const base::flat_set<viz::SurfaceRange> surface_ranges) {
  DCHECK(surface_layer_ranges_.empty());
  surface_layer_ranges_ = std::move(surface_ranges);
  needs_surface_ranges_sync_ = true;
}

const base::flat_set<viz::SurfaceRange>& LayerTreeImpl::SurfaceRanges() const {
  return surface_layer_ranges_;
}

void LayerTreeImpl::ClearSurfaceRanges() {
  surface_layer_ranges_.clear();
  needs_surface_ranges_sync_ = true;
}

void LayerTreeImpl::AddLayerShouldPushProperties(LayerImpl* layer) {
  DCHECK(!IsActiveTree()) << "The active tree does not push layer properties";
  // TODO(crbug.com/303943): PictureLayerImpls always push properties so should
  // not go into this set or we'd push them twice.
  DCHECK(!base::Contains(picture_layers_, layer));
  layers_that_should_push_properties_.insert(layer);
}

void LayerTreeImpl::ClearLayersThatShouldPushProperties() {
  layers_that_should_push_properties_.clear();
}

void LayerTreeImpl::RegisterLayer(LayerImpl* layer) {
  DCHECK(!LayerById(layer->id()));
  layer_id_map_[layer->id()] = layer;
}

void LayerTreeImpl::UnregisterLayer(LayerImpl* layer) {
  DCHECK(LayerById(layer->id()));
  layers_that_should_push_properties_.erase(layer);
  layer_id_map_.erase(layer->id());
}

void LayerTreeImpl::AddLayer(std::unique_ptr<LayerImpl> layer) {
  DCHECK(layer);
  DCHECK(!base::Contains(layer_list_, layer));
  layer_list_.push_back(std::move(layer));
  set_needs_update_draw_properties();
}

size_t LayerTreeImpl::NumLayers() {
  return layer_id_map_.size();
}

void LayerTreeImpl::DidBecomeActive() {
  if (next_activation_forces_redraw_) {
    host_impl_->SetViewportDamage(GetDeviceViewport());
    next_activation_forces_redraw_ = false;
  }

  // Always reset this flag on activation, as we would only have activated
  // if we were in a good state.
  host_impl_->ResetRequiresHighResToDraw();

  for (auto* layer : *this)
    layer->DidBecomeActive();

  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->DidActivate();
  devtools_instrumentation::DidActivateLayerTree(host_impl_->id(),
                                                 source_frame_number_);
}

bool LayerTreeImpl::RequiresHighResToDraw() const {
  return host_impl_->RequiresHighResToDraw();
}

TaskRunnerProvider* LayerTreeImpl::task_runner_provider() const {
  return host_impl_->task_runner_provider();
}

LayerTreeFrameSink* LayerTreeImpl::layer_tree_frame_sink() {
  return host_impl_->layer_tree_frame_sink();
}

int LayerTreeImpl::max_texture_size() const {
  return host_impl_->max_texture_size();
}

const LayerTreeSettings& LayerTreeImpl::settings() const {
  return host_impl_->settings();
}

const LayerTreeDebugState& LayerTreeImpl::debug_state() const {
  return host_impl_->debug_state();
}

viz::ContextProvider* LayerTreeImpl::context_provider() const {
  return host_impl_->layer_tree_frame_sink()->context_provider();
}

viz::ClientResourceProvider* LayerTreeImpl::resource_provider() const {
  return host_impl_->resource_provider();
}

TileManager* LayerTreeImpl::tile_manager() const {
  return host_impl_->tile_manager();
}

ImageDecodeCache* LayerTreeImpl::image_decode_cache() const {
  return host_impl_->image_decode_cache();
}

ImageAnimationController* LayerTreeImpl::image_animation_controller() const {
  return host_impl_->image_animation_controller();
}

FrameRateCounter* LayerTreeImpl::frame_rate_counter() const {
  return host_impl_->fps_counter();
}

base::Optional<int> LayerTreeImpl::current_universal_throughput() {
  return host_impl_->current_universal_throughput();
}

MemoryHistory* LayerTreeImpl::memory_history() const {
  return host_impl_->memory_history();
}

DebugRectHistory* LayerTreeImpl::debug_rect_history() const {
  return host_impl_->debug_rect_history();
}

bool LayerTreeImpl::IsActiveTree() const {
  return host_impl_->active_tree() == this;
}

bool LayerTreeImpl::IsPendingTree() const {
  return host_impl_->pending_tree() == this;
}

bool LayerTreeImpl::IsRecycleTree() const {
  return host_impl_->recycle_tree() == this;
}

bool LayerTreeImpl::IsSyncTree() const {
  return host_impl_->sync_tree() == this;
}

LayerImpl* LayerTreeImpl::FindActiveTreeLayerById(int id) {
  LayerTreeImpl* tree = host_impl_->active_tree();
  if (!tree)
    return nullptr;
  return tree->LayerById(id);
}

LayerImpl* LayerTreeImpl::FindPendingTreeLayerById(int id) {
  LayerTreeImpl* tree = host_impl_->pending_tree();
  if (!tree)
    return nullptr;
  return tree->LayerById(id);
}

bool LayerTreeImpl::PinchGestureActive() const {
  return host_impl_->pinch_gesture_active();
}

const viz::BeginFrameArgs& LayerTreeImpl::CurrentBeginFrameArgs() const {
  return host_impl_->CurrentBeginFrameArgs();
}

base::TimeDelta LayerTreeImpl::CurrentBeginFrameInterval() const {
  return host_impl_->CurrentBeginFrameInterval();
}

const gfx::Rect LayerTreeImpl::ViewportRectForTilePriority() const {
  const gfx::Rect& viewport_rect_for_tile_priority =
      host_impl_->viewport_rect_for_tile_priority();
  return viewport_rect_for_tile_priority.IsEmpty()
             ? GetDeviceViewport()
             : viewport_rect_for_tile_priority;
}

std::unique_ptr<ScrollbarAnimationController>
LayerTreeImpl::CreateScrollbarAnimationController(ElementId scroll_element_id,
                                                  float initial_opacity) {
  DCHECK(!settings().scrollbar_fade_delay.is_zero());
  DCHECK(!settings().scrollbar_fade_duration.is_zero());
  base::TimeDelta fade_delay = settings().scrollbar_fade_delay;
  base::TimeDelta fade_duration = settings().scrollbar_fade_duration;
  switch (settings().scrollbar_animator) {
    case LayerTreeSettings::ANDROID_OVERLAY: {
      return ScrollbarAnimationController::
          CreateScrollbarAnimationControllerAndroid(
              scroll_element_id, host_impl_, fade_delay, fade_duration,
              initial_opacity);
    }
    case LayerTreeSettings::AURA_OVERLAY: {
      base::TimeDelta thinning_duration =
          settings().scrollbar_thinning_duration;
      return ScrollbarAnimationController::
          CreateScrollbarAnimationControllerAuraOverlay(
              scroll_element_id, host_impl_, fade_delay, fade_duration,
              thinning_duration, initial_opacity);
    }
    case LayerTreeSettings::NO_ANIMATOR:
      NOTREACHED();
      break;
  }
  return nullptr;
}

void LayerTreeImpl::DidAnimateScrollOffset() {
  host_impl_->DidAnimateScrollOffset();
}

bool LayerTreeImpl::use_gpu_rasterization() const {
  return host_impl_->use_gpu_rasterization();
}

GpuRasterizationStatus LayerTreeImpl::GetGpuRasterizationStatus() const {
  return host_impl_->gpu_rasterization_status();
}

bool LayerTreeImpl::create_low_res_tiling() const {
  return host_impl_->create_low_res_tiling();
}

void LayerTreeImpl::SetNeedsRedraw() {
  host_impl_->SetNeedsRedraw();
}

void LayerTreeImpl::GetAllPrioritizedTilesForTracing(
    std::vector<PrioritizedTile>* prioritized_tiles) const {
  for (auto* layer : base::Reversed(*this)) {
    if (!layer->contributes_to_drawn_render_surface())
      continue;
    layer->GetAllPrioritizedTilesForTracing(prioritized_tiles);
  }
}

void LayerTreeImpl::AsValueInto(base::trace_event::TracedValue* state) const {
  viz::TracedValue::MakeDictIntoImplicitSnapshot(state, "cc::LayerTreeImpl",
                                                 this);
  state->SetInteger("source_frame_number", source_frame_number_);

  state->BeginArray("render_surface_layer_list");
  for (auto* layer : base::Reversed(*this)) {
    if (layer->contributes_to_drawn_render_surface())
      continue;
    viz::TracedValue::AppendIDRef(layer, state);
  }
  state->EndArray();

  state->BeginArray("swap_promise_trace_ids");
  for (const auto& swap_promise : swap_promise_list_)
    state->AppendDouble(swap_promise->TraceId());
  state->EndArray();

  state->BeginArray("pinned_swap_promise_trace_ids");
  for (const auto& swap_promise : pinned_swap_promise_list_)
    state->AppendDouble(swap_promise->TraceId());
  state->EndArray();

  state->BeginArray("layers");
  for (auto* layer : *this) {
    state->BeginDictionary();
    layer->AsValueInto(state);
    state->EndDictionary();
  }
  state->EndArray();
}

void LayerTreeImpl::QueueSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  DCHECK(swap_promise);
  swap_promise_list_.push_back(std::move(swap_promise));
}

void LayerTreeImpl::QueuePinnedSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  DCHECK(IsActiveTree());
  DCHECK(swap_promise);
  pinned_swap_promise_list_.push_back(std::move(swap_promise));
}

void LayerTreeImpl::PassSwapPromises(
    std::vector<std::unique_ptr<SwapPromise>> new_swap_promises) {
  for (auto& swap_promise : swap_promise_list_) {
    if (swap_promise->DidNotSwap(SwapPromise::SWAP_FAILS) ==
        SwapPromise::DidNotSwapAction::KEEP_ACTIVE) {
      // |swap_promise| must remain active, so place it in |new_swap_promises|
      // in order to keep it alive and active.
      new_swap_promises.push_back(std::move(swap_promise));
    }
  }
  swap_promise_list_.clear();
  swap_promise_list_.swap(new_swap_promises);
}

void LayerTreeImpl::AppendSwapPromises(
    std::vector<std::unique_ptr<SwapPromise>> new_swap_promises) {
  std::move(new_swap_promises.begin(), new_swap_promises.end(),
            std::back_inserter(swap_promise_list_));
  new_swap_promises.clear();
}

void LayerTreeImpl::FinishSwapPromises(viz::CompositorFrameMetadata* metadata) {
  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->WillSwap(metadata);
  for (const auto& swap_promise : pinned_swap_promise_list_)
    swap_promise->WillSwap(metadata);
}

void LayerTreeImpl::ClearSwapPromises() {
  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->DidSwap();
  swap_promise_list_.clear();
  for (const auto& swap_promise : pinned_swap_promise_list_)
    swap_promise->DidSwap();
  pinned_swap_promise_list_.clear();
}

void LayerTreeImpl::BreakSwapPromises(SwapPromise::DidNotSwapReason reason) {
  {
    std::vector<std::unique_ptr<SwapPromise>> persistent_swap_promises;
    for (auto& swap_promise : swap_promise_list_) {
      if (swap_promise->DidNotSwap(reason) ==
          SwapPromise::DidNotSwapAction::KEEP_ACTIVE) {
        persistent_swap_promises.push_back(std::move(swap_promise));
      }
    }
    // |persistent_swap_promises| must remain active even when swap fails.
    swap_promise_list_ = std::move(persistent_swap_promises);
  }

  {
    std::vector<std::unique_ptr<SwapPromise>> persistent_swap_promises;
    for (auto& swap_promise : pinned_swap_promise_list_) {
      if (swap_promise->DidNotSwap(reason) ==
          SwapPromise::DidNotSwapAction::KEEP_ACTIVE) {
        persistent_swap_promises.push_back(std::move(swap_promise));
      }
    }

    // |persistent_swap_promises| must remain active even when swap fails.
    pinned_swap_promise_list_ = std::move(persistent_swap_promises);
  }
}

void LayerTreeImpl::DidModifyTilePriorities() {
  host_impl_->DidModifyTilePriorities();
}

void LayerTreeImpl::set_ui_resource_request_queue(
    UIResourceRequestQueue queue) {
  ui_resource_request_queue_ = std::move(queue);
}

viz::ResourceId LayerTreeImpl::ResourceIdForUIResource(UIResourceId uid) const {
  return host_impl_->ResourceIdForUIResource(uid);
}

bool LayerTreeImpl::IsUIResourceOpaque(UIResourceId uid) const {
  return host_impl_->IsUIResourceOpaque(uid);
}

void LayerTreeImpl::ProcessUIResourceRequestQueue() {
  TRACE_EVENT1("cc", "ProcessUIResourceRequestQueue", "queue_size",
               ui_resource_request_queue_.size());
  for (const auto& req : ui_resource_request_queue_) {
    switch (req.GetType()) {
      case UIResourceRequest::UI_RESOURCE_CREATE:
        host_impl_->CreateUIResource(req.GetId(), req.GetBitmap());
        break;
      case UIResourceRequest::UI_RESOURCE_DELETE:
        host_impl_->DeleteUIResource(req.GetId());
        break;
      case UIResourceRequest::UI_RESOURCE_INVALID_REQUEST:
        NOTREACHED();
        break;
    }
  }
  ui_resource_request_queue_.clear();

  // If all UI resource evictions were not recreated by processing this queue,
  // then another commit is required.
  if (host_impl_->EvictedUIResourcesExist())
    host_impl_->SetNeedsCommit();
}

void LayerTreeImpl::RegisterPictureLayerImpl(PictureLayerImpl* layer) {
  DCHECK(!base::Contains(picture_layers_, layer));
  picture_layers_.push_back(layer);
}

void LayerTreeImpl::UnregisterPictureLayerImpl(PictureLayerImpl* layer) {
  auto it = std::find(picture_layers_.begin(), picture_layers_.end(), layer);
  DCHECK(it != picture_layers_.end());
  picture_layers_.erase(it);

  // Make sure that |picture_layers_with_paint_worklets_| doesn't get left with
  // dead layers. They should already have been removed (via calling
  // NotifyLayerHasPaintWorkletsChanged) before the layer was unregistered.
  DCHECK(!picture_layers_with_paint_worklets_.contains(layer));
}

void LayerTreeImpl::NotifyLayerHasPaintWorkletsChanged(PictureLayerImpl* layer,
                                                       bool has_worklets) {
  if (has_worklets) {
    auto insert_pair = picture_layers_with_paint_worklets_.insert(layer);
    DCHECK(insert_pair.second);
  } else {
    auto it = picture_layers_with_paint_worklets_.find(layer);
    DCHECK(it != picture_layers_with_paint_worklets_.end());
    picture_layers_with_paint_worklets_.erase(it);
  }
}

void LayerTreeImpl::RegisterScrollbar(ScrollbarLayerImplBase* scrollbar_layer) {
  ElementId scroll_element_id = scrollbar_layer->scroll_element_id();
  if (!scroll_element_id)
    return;

  auto* scrollbar_ids = &element_id_to_scrollbar_layer_ids_[scroll_element_id];
  int* scrollbar_layer_id = scrollbar_layer->orientation() == HORIZONTAL
                                ? &scrollbar_ids->horizontal
                                : &scrollbar_ids->vertical;

  // We used to DCHECK this was not the case but this can occur on Android: as
  // the visual viewport supplies scrollbars for the outer viewport, if the
  // outer viewport is changed, we race between updating the visual viewport
  // scrollbars and registering new scrollbars on the old outer viewport. It'd
  // be nice if we could fix this to be cleaner but its harmless to just
  // unregister here.
  if (*scrollbar_layer_id != Layer::INVALID_ID) {
    UnregisterScrollbar(scrollbar_layer);

    // The scrollbar_ids could have been erased above so get it again.
    scrollbar_ids = &element_id_to_scrollbar_layer_ids_[scroll_element_id];
    scrollbar_layer_id = scrollbar_layer->orientation() == HORIZONTAL
                             ? &scrollbar_ids->horizontal
                             : &scrollbar_ids->vertical;
  }

  *scrollbar_layer_id = scrollbar_layer->id();

  if (IsActiveTree() && scrollbar_layer->is_overlay_scrollbar() &&
      scrollbar_layer->GetScrollbarAnimator() !=
          LayerTreeSettings::NO_ANIMATOR) {
    host_impl_->RegisterScrollbarAnimationController(
        scroll_element_id, scrollbar_layer->Opacity());
  }

  // The new scrollbar's geometries need to be initialized.
  SetScrollbarGeometriesNeedUpdate();
}

void LayerTreeImpl::UnregisterScrollbar(
    ScrollbarLayerImplBase* scrollbar_layer) {
  ElementId scroll_element_id = scrollbar_layer->scroll_element_id();
  if (!scroll_element_id)
    return;

  auto& scrollbar_ids = element_id_to_scrollbar_layer_ids_[scroll_element_id];
  if (scrollbar_layer->orientation() == HORIZONTAL)
    scrollbar_ids.horizontal = Layer::INVALID_ID;
  else
    scrollbar_ids.vertical = Layer::INVALID_ID;

  if (scrollbar_ids.horizontal == Layer::INVALID_ID &&
      scrollbar_ids.vertical == Layer::INVALID_ID) {
    element_id_to_scrollbar_layer_ids_.erase(scroll_element_id);
    if (IsActiveTree()) {
      host_impl_->DidUnregisterScrollbarLayer(scroll_element_id);
    }
  }
}

ScrollbarSet LayerTreeImpl::ScrollbarsFor(ElementId scroll_element_id) const {
  ScrollbarSet scrollbars;
  auto it = element_id_to_scrollbar_layer_ids_.find(scroll_element_id);
  if (it != element_id_to_scrollbar_layer_ids_.end()) {
    const ScrollbarLayerIds& layer_ids = it->second;
    if (layer_ids.horizontal != Layer::INVALID_ID)
      scrollbars.insert(ToScrollbarLayer(LayerById(layer_ids.horizontal)));
    if (layer_ids.vertical != Layer::INVALID_ID)
      scrollbars.insert(ToScrollbarLayer(LayerById(layer_ids.vertical)));
  }
  return scrollbars;
}

static bool PointHitsRect(
    const gfx::PointF& screen_space_point,
    const gfx::Transform& local_space_to_screen_space_transform,
    const gfx::Rect& local_space_rect,
    float* distance_to_camera) {
  // If the transform is not invertible, then assume that this point doesn't hit
  // this rect.
  gfx::Transform inverse_local_space_to_screen_space(
      gfx::Transform::kSkipInitialization);
  if (!local_space_to_screen_space_transform.GetInverse(
          &inverse_local_space_to_screen_space))
    return false;

  // Transform the hit test point from screen space to the local space of the
  // given rect.
  bool clipped = false;
  gfx::Point3F planar_point = MathUtil::ProjectPoint3D(
      inverse_local_space_to_screen_space, screen_space_point, &clipped);
  gfx::PointF hit_test_point_in_local_space =
      gfx::PointF(planar_point.x(), planar_point.y());

  // If ProjectPoint could not project to a valid value, then we assume that
  // this point doesn't hit this rect.
  if (clipped)
    return false;

  if (!gfx::RectF(local_space_rect).Contains(hit_test_point_in_local_space))
    return false;

  if (distance_to_camera) {
    // To compute the distance to the camera, we have to take the planar point
    // and pull it back to world space and compute the displacement along the
    // z-axis.
    gfx::Point3F planar_point_in_screen_space(planar_point);
    local_space_to_screen_space_transform.TransformPoint(
        &planar_point_in_screen_space);
    *distance_to_camera = planar_point_in_screen_space.z();
  }

  return true;
}

static bool PointIsClippedByAncestorClipNode(
    const gfx::PointF& screen_space_point,
    const LayerImpl* layer) {
  // We need to visit all ancestor clip nodes to check this. Checking with just
  // the combined clip stored at a clip node is not enough because parent
  // combined clip can sometimes be smaller than current combined clip. This can
  // happen when we have transforms like rotation that inflate the combined
  // clip's bounds. Also, the point can be clipped by the content rect of an
  // ancestor render surface.

  // We first check if the point is clipped by viewport.
  const PropertyTrees* property_trees =
      layer->layer_tree_impl()->property_trees();
  const ClipTree& clip_tree = property_trees->clip_tree;
  const TransformTree& transform_tree = property_trees->transform_tree;
  gfx::Rect clip = gfx::ToEnclosingRect(clip_tree.Node(1)->clip);
  if (!PointHitsRect(screen_space_point, gfx::Transform(), clip, nullptr))
    return true;

  for (const ClipNode* clip_node = clip_tree.Node(layer->clip_tree_index());
       clip_node->id > ClipTree::kViewportNodeId;
       clip_node = clip_tree.parent(clip_node)) {
    if (clip_node->clip_type == ClipNode::ClipType::APPLIES_LOCAL_CLIP) {
      clip = gfx::ToEnclosingRect(clip_node->clip);

      gfx::Transform screen_space_transform =
          transform_tree.ToScreen(clip_node->transform_id);
      if (!PointHitsRect(screen_space_point, screen_space_transform, clip,
                         nullptr)) {
        return true;
      }
    }
  }
  return false;
}

static bool PointIsClippedBySurfaceOrClipRect(
    const gfx::PointF& screen_space_point,
    const LayerImpl* layer) {
  // Walk up the layer tree and hit-test any render_surfaces and any layer
  // clip rects that are active.
  return PointIsClippedByAncestorClipNode(screen_space_point, layer);
}

static bool PointHitsRegion(const gfx::PointF& screen_space_point,
                            const gfx::Transform& screen_space_transform,
                            const Region& layer_space_region,
                            const LayerImpl* layer_impl) {
  if (layer_space_region.IsEmpty())
    return false;

  // If the transform is not invertible, then assume that this point doesn't hit
  // this region.
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  if (!screen_space_transform.GetInverse(&inverse_screen_space_transform))
    return false;

  // Transform the hit test point from screen space to the local space of the
  // given region.
  bool clipped = false;
  gfx::PointF hit_test_point_in_layer_space = MathUtil::ProjectPoint(
      inverse_screen_space_transform, screen_space_point, &clipped);

  // If ProjectPoint could not project to a valid value, then we assume that
  // this point doesn't hit this region.
  if (clipped)
    return false;

  // We need to walk up the parents to ensure that the layer is not clipped in
  // such a way that it is impossible for the point to hit the layer.
  if (layer_impl &&
      PointIsClippedBySurfaceOrClipRect(screen_space_point, layer_impl))
    return false;

  return layer_space_region.Contains(
      gfx::ToRoundedPoint(hit_test_point_in_layer_space));
}

static bool PointHitsLayer(const LayerImpl* layer,
                           const gfx::PointF& screen_space_point,
                           float* distance_to_intersection) {
  gfx::Rect content_rect(layer->bounds());
  if (!PointHitsRect(screen_space_point, layer->ScreenSpaceTransform(),
                     content_rect, distance_to_intersection)) {
    return false;
  }

  // At this point, we think the point does hit the layer, but we need to walk
  // up the parents to ensure that the layer was not clipped in such a way
  // that the hit point actually should not hit the layer.
  if (PointIsClippedBySurfaceOrClipRect(screen_space_point, layer))
    return false;

  // Skip the HUD layer.
  if (layer == layer->layer_tree_impl()->hud_layer())
    return false;

  return true;
}

struct FindClosestMatchingLayerState {
  FindClosestMatchingLayerState()
      : closest_match(nullptr),
        closest_distance(-std::numeric_limits<float>::infinity()) {}
  LayerImpl* closest_match;
  // Note that the positive z-axis points towards the camera, so bigger means
  // closer in this case, counterintuitively.
  float closest_distance;
};

template <typename Functor>
static void FindClosestMatchingLayer(const gfx::PointF& screen_space_point,
                                     LayerImpl* root_layer,
                                     const Functor& func,
                                     FindClosestMatchingLayerState* state) {
  base::ElapsedTimer timer;
  // We want to iterate from front to back when hit testing.
  for (auto* layer : base::Reversed(*root_layer->layer_tree_impl())) {
    if (!func(layer))
      continue;

    float distance_to_intersection = 0.f;
    bool hit = false;
    if (layer->Is3dSorted()) {
      hit =
          PointHitsLayer(layer, screen_space_point, &distance_to_intersection);
    } else {
      hit = PointHitsLayer(layer, screen_space_point, nullptr);
    }

    if (!hit)
      continue;

    bool in_front_of_previous_candidate =
        state->closest_match &&
        layer->GetSortingContextId() ==
            state->closest_match->GetSortingContextId() &&
        distance_to_intersection >
            state->closest_distance + std::numeric_limits<float>::epsilon();

    if (!state->closest_match || in_front_of_previous_candidate) {
      state->closest_distance = distance_to_intersection;
      state->closest_match = layer;
    }
  }
  if (const char* client_name = GetClientNameForMetrics()) {
    UMA_HISTOGRAM_COUNTS_1M(
        base::StringPrintf("Compositing.%s.HitTestTimeToFindClosestLayer",
                           client_name),
        timer.Elapsed().InMicroseconds());
  }
}

LayerImpl* LayerTreeImpl::FindFirstScrollingLayerOrScrollbarThatIsHitByPoint(
    const gfx::PointF& screen_space_point) {
  if (layer_list_.empty())
    return nullptr;

  FindClosestMatchingLayerState state;
  LayerImpl* root_layer = layer_list_[0].get();
  auto HitTestScrollingLayerOrScrollbarFunctor =
      [this](const LayerImpl* layer) {
        return layer->HitTestable() &&
               (layer->IsScrollbarLayer() ||
                (property_trees()->scroll_tree.FindNodeFromElementId(
                    layer->element_id())));
      };
  FindClosestMatchingLayer(screen_space_point, root_layer,
                           HitTestScrollingLayerOrScrollbarFunctor, &state);
  return state.closest_match;
}

struct HitTestVisibleScrollableOrTouchableFunctor {
  bool operator()(LayerImpl* layer) const { return layer->HitTestable(); }
};

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPoint(
    const gfx::PointF& screen_space_point) {
  if (layer_list_.empty())
    return nullptr;
  if (!UpdateDrawProperties())
    return nullptr;
  FindClosestMatchingLayerState state;
  FindClosestMatchingLayer(screen_space_point, layer_list_[0].get(),
                           HitTestVisibleScrollableOrTouchableFunctor(),
                           &state);
  return state.closest_match;
}

struct FindTouchEventLayerFunctor {
  bool operator()(LayerImpl* layer) const {
    if (!layer->has_touch_action_regions())
      return false;
    return PointHitsRegion(screen_space_point, layer->ScreenSpaceTransform(),
                           layer->GetAllTouchActionRegions(), layer);
  }
  const gfx::PointF screen_space_point;
};

struct FindWheelEventHandlerLayerFunctor {
  bool operator()(LayerImpl* layer) const {
    return PointHitsRegion(screen_space_point, layer->ScreenSpaceTransform(),
                           layer->wheel_event_handler_region(), layer);
  }
  const gfx::PointF screen_space_point;
};

template <typename Functor>
LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPointInEventHandlerRegion(
    const gfx::PointF& screen_space_point,
    const Functor& func) {
  if (layer_list_.empty())
    return nullptr;
  if (!UpdateDrawProperties())
    return nullptr;
  FindClosestMatchingLayerState state;
  FindClosestMatchingLayer(screen_space_point, layer_list_[0].get(), func,
                           &state);
  return state.closest_match;
}

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPointInTouchHandlerRegion(
    const gfx::PointF& screen_space_point) {
  FindTouchEventLayerFunctor func = {screen_space_point};
  return FindLayerThatIsHitByPointInEventHandlerRegion(screen_space_point,
                                                       func);
}

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPointInWheelEventHandlerRegion(
    const gfx::PointF& screen_space_point) {
  FindWheelEventHandlerLayerFunctor func = {screen_space_point};
  return FindLayerThatIsHitByPointInEventHandlerRegion(screen_space_point,
                                                       func);
}

std::vector<const LayerImpl*>
LayerTreeImpl::FindLayersHitByPointInNonFastScrollableRegion(
    const gfx::PointF& screen_space_point) {
  std::vector<const LayerImpl*> layers;
  if (layer_list_.empty())
    return layers;
  if (!UpdateDrawProperties())
    return layers;
  for (const auto* layer : *this) {
    if (layer->non_fast_scrollable_region().IsEmpty())
      continue;
    if (!PointHitsLayer(layer, screen_space_point, nullptr))
      continue;
    if (PointHitsRegion(screen_space_point, layer->ScreenSpaceTransform(),
                        layer->non_fast_scrollable_region(), layer)) {
      layers.push_back(layer);
    }
  }

  return layers;
}

struct HitTestFramedVisibleScrollableOrTouchableFunctor {
  bool operator()(LayerImpl* layer) const {
    return layer->HitTestable() && layer->frame_element_id();
  }
};

ElementId LayerTreeImpl::FindFrameElementIdAtPoint(
    const gfx::PointF& screen_space_point) {
  if (layer_list_.empty())
    return {};
  if (!UpdateDrawProperties())
    return {};
  FindClosestMatchingLayerState state;
  FindClosestMatchingLayer(screen_space_point, layer_list_[0].get(),
                           HitTestFramedVisibleScrollableOrTouchableFunctor(),
                           &state);

  if (auto* layer = state.closest_match) {
    // TODO(https://crbug.com/1058870): Permit hit testing only if the framed
    // element hit has a simple mask/clip. We don't have enough information
    // about complex masks/clips on the impl-side to do accurate hit testing.
    bool layer_hit_test_region_is_masked =
        property_trees()->effect_tree.HitTestMayBeAffectedByMask(
            layer->effect_tree_index());

    if (!layer_hit_test_region_is_masked)
      return layer->frame_element_id();
  }
  return {};
}

void LayerTreeImpl::RegisterSelection(const LayerSelection& selection) {
  if (selection_ == selection)
    return;

  handle_visibility_changed_ = true;
  selection_ = selection;
}

void LayerTreeImpl::ResetHandleVisibilityChanged() {
  handle_visibility_changed_ = false;
}

static gfx::SelectionBound ComputeViewportSelectionBound(
    const LayerSelectionBound& layer_bound,
    LayerImpl* layer,
    float device_scale_factor) {
  gfx::SelectionBound viewport_bound;
  viewport_bound.set_type(layer_bound.type);

  if (!layer || layer_bound.type == gfx::SelectionBound::EMPTY)
    return viewport_bound;

  auto layer_start = gfx::PointF(layer_bound.edge_start);
  auto layer_end = gfx::PointF(layer_bound.edge_end);
  gfx::Transform screen_space_transform = layer->ScreenSpaceTransform();

  bool clipped = false;
  gfx::PointF screen_start =
      MathUtil::MapPoint(screen_space_transform, layer_start, &clipped);
  gfx::PointF screen_end =
      MathUtil::MapPoint(screen_space_transform, layer_end, &clipped);

  // MapPoint can produce points with NaN components (even when no inputs are
  // NaN). Since consumers of gfx::SelectionBounds may round |edge_start| or
  // |edge_end| (and since rounding will crash on NaN), we return an empty
  // bound instead.
  if (std::isnan(screen_start.x()) || std::isnan(screen_start.y()) ||
      std::isnan(screen_end.x()) || std::isnan(screen_end.y()))
    return gfx::SelectionBound();

  const float inv_scale = 1.f / device_scale_factor;
  viewport_bound.SetEdgeStart(gfx::ScalePoint(screen_start, inv_scale));
  viewport_bound.SetEdgeEnd(gfx::ScalePoint(screen_end, inv_scale));

  // If |layer_bound| is already hidden due to being occluded by painted content
  // within the layer, it must remain hidden. Otherwise, check whether its
  // position is outside the bounds of the layer.
  if (layer_bound.hidden) {
    viewport_bound.set_visible(false);
  } else {
    // The bottom edge point is used for visibility testing as it is the logical
    // focal point for bound selection handles (this may change in the future).
    // Shifting the visibility point fractionally inward ensures that
    // neighboring or logically coincident layers aligned to integral DPI
    // coordinates will not spuriously occlude the bound.
    gfx::Vector2dF visibility_offset = layer_start - layer_end;
    visibility_offset.Scale(device_scale_factor / visibility_offset.Length());
    gfx::PointF visibility_point = layer_end + visibility_offset;
    if (visibility_point.x() <= 0)
      visibility_point.set_x(visibility_point.x() + device_scale_factor);
    visibility_point =
        MathUtil::MapPoint(screen_space_transform, visibility_point, &clipped);

    float intersect_distance = 0.f;
    viewport_bound.set_visible(
        PointHitsLayer(layer, visibility_point, &intersect_distance));
  }

  if (viewport_bound.visible()) {
    viewport_bound.SetVisibleEdge(viewport_bound.edge_start(),
                                  viewport_bound.edge_end());
  } else {
    // The |layer_start| and |layer_end| might be clipped.
    gfx::RectF visible_layer_rect(layer->visible_layer_rect());
    auto visible_layer_start = layer_start;
    auto visible_layer_end = layer_end;
    if (!visible_layer_rect.Contains(visible_layer_start) &&
        !visible_layer_rect.Contains(visible_layer_end))
      std::tie(visible_layer_start, visible_layer_end) =
          GetVisibleSelectionEndPoints(visible_layer_rect, layer_start,
                                       layer_end);

    gfx::PointF visible_screen_start = MathUtil::MapPoint(
        screen_space_transform, visible_layer_start, &clipped);
    gfx::PointF visible_screen_end =
        MathUtil::MapPoint(screen_space_transform, visible_layer_end, &clipped);

    viewport_bound.SetVisibleEdgeStart(
        gfx::ScalePoint(visible_screen_start, inv_scale));
    viewport_bound.SetVisibleEdgeEnd(
        gfx::ScalePoint(visible_screen_end, inv_scale));
  }

  return viewport_bound;
}

void LayerTreeImpl::GetViewportSelection(
    viz::Selection<gfx::SelectionBound>* selection) {
  DCHECK(selection);

  selection->start = ComputeViewportSelectionBound(
      selection_.start,
      selection_.start.layer_id ? LayerById(selection_.start.layer_id)
                                : nullptr,
      device_scale_factor() * painted_device_scale_factor());
  if (selection->start.type() == gfx::SelectionBound::CENTER ||
      selection->start.type() == gfx::SelectionBound::EMPTY) {
    selection->end = selection->start;
  } else {
    selection->end = ComputeViewportSelectionBound(
        selection_.end,
        selection_.end.layer_id ? LayerById(selection_.end.layer_id) : nullptr,
        device_scale_factor() * painted_device_scale_factor());
  }
}

bool LayerTreeImpl::SmoothnessTakesPriority() const {
  return host_impl_->GetTreePriority() == SMOOTHNESS_TAKES_PRIORITY;
}

VideoFrameControllerClient* LayerTreeImpl::GetVideoFrameControllerClient()
    const {
  return host_impl_;
}

void LayerTreeImpl::UpdateImageDecodingHints(
    base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
        decoding_mode_map) {
  host_impl_->UpdateImageDecodingHints(std::move(decoding_mode_map));
}

int LayerTreeImpl::GetMSAASampleCountForRaster(
    const scoped_refptr<DisplayItemList>& display_list) {
  return host_impl_->GetMSAASampleCountForRaster(display_list);
}

void LayerTreeImpl::SetPendingPageScaleAnimation(
    std::unique_ptr<PendingPageScaleAnimation> pending_animation) {
  pending_page_scale_animation_ = std::move(pending_animation);
}

std::unique_ptr<PendingPageScaleAnimation>
LayerTreeImpl::TakePendingPageScaleAnimation() {
  return std::move(pending_page_scale_animation_);
}

void LayerTreeImpl::AppendEventsMetricsFromMainThread(
    std::vector<EventMetrics> events_metrics) {
  events_metrics_from_main_thread_.reserve(
      events_metrics_from_main_thread_.size() + events_metrics.size());
  events_metrics_from_main_thread_.insert(
      events_metrics_from_main_thread_.end(), events_metrics.begin(),
      events_metrics.end());
}

std::vector<EventMetrics> LayerTreeImpl::TakeEventsMetrics() {
  std::vector<EventMetrics> main_event_metrics_result;
  main_event_metrics_result.swap(events_metrics_from_main_thread_);
  return main_event_metrics_result;
}

bool LayerTreeImpl::TakeForceSendMetadataRequest() {
  bool force_send_metadata_request = force_send_metadata_request_;
  force_send_metadata_request_ = false;
  return force_send_metadata_request;
}

void LayerTreeImpl::ResetAllChangeTracking() {
  layers_that_should_push_properties_.clear();
  // Iterate over all layers, including masks.
  for (auto* layer : *this)
    layer->ResetChangeTracking();
  property_trees_.ResetAllChangeTracking();
}

std::string LayerTreeImpl::LayerListAsJson() const {
  base::trace_event::TracedValueJSON value;
  value.BeginArray("LayerTreeImpl");
  for (auto* layer : *this) {
    value.BeginDictionary();
    layer->AsValueInto(&value);
    value.EndDictionary();
  }
  value.EndArray();
  return value.ToFormattedJSON();
}

}  // namespace cc
