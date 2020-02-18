/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/visual_viewport.h"

#include <memory>

#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/scrollbar_layer_interface.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/platform/geometry/double_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

VisualViewport::VisualViewport(Page& owner)
    : page_(&owner),
      scale_(1),
      is_pinch_gesture_active_(false),
      browser_controls_adjustment_(0),
      max_page_scale_(-1),
      track_pinch_zoom_stats_for_page_(false),
      needs_paint_property_update_(true) {
  UniqueObjectId unique_id = NewUniqueObjectId();
  element_id_ = CompositorElementIdFromUniqueObjectId(
      unique_id, CompositorElementIdNamespace::kPrimary);
  scroll_element_id_ = CompositorElementIdFromUniqueObjectId(
      unique_id, CompositorElementIdNamespace::kScroll);
  overscroll_elasticity_element_id_ = CompositorElementIdFromUniqueObjectId(
      unique_id, CompositorElementIdNamespace::kOverscrollElasticity);
  Reset();
}

TransformPaintPropertyNode* VisualViewport::GetDeviceEmulationTransformNode()
    const {
  return device_emulation_transform_node_.get();
}

TransformPaintPropertyNode*
VisualViewport::GetOverscrollElasticityTransformNode() const {
  return overscroll_elasticity_transform_node_.get();
}

TransformPaintPropertyNode* VisualViewport::GetPageScaleNode() const {
  return scale_transform_node_.get();
}

TransformPaintPropertyNode* VisualViewport::GetScrollTranslationNode() const {
  return translation_transform_node_.get();
}

ScrollPaintPropertyNode* VisualViewport::GetScrollNode() const {
  return scroll_node_.get();
}

PaintPropertyChangeType VisualViewport::UpdatePaintPropertyNodesIfNeeded(
    PaintPropertyTreeBuilderFragmentContext& context) {
  PaintPropertyChangeType change = PaintPropertyChangeType::kUnchanged;
  if (!needs_paint_property_update_)
    return change;

  needs_paint_property_update_ = false;

  auto* transform_parent = context.current.transform;
  auto* scroll_parent = context.current.scroll;
  auto* clip_parent = context.current.clip;
  auto* effect_parent = context.current_effect;

  DCHECK(transform_parent);
  DCHECK(scroll_parent);
  DCHECK(clip_parent);
  DCHECK(effect_parent);

  {
    const auto& device_emulation_transform =
        GetChromeClient()->GetDeviceEmulationTransform();
    if (!device_emulation_transform.IsIdentity()) {
      TransformPaintPropertyNode::State state{device_emulation_transform};
      state.in_subtree_of_page_scale = false;
      if (!device_emulation_transform_node_) {
        device_emulation_transform_node_ = TransformPaintPropertyNode::Create(
            *transform_parent, std::move(state));
        change = PaintPropertyChangeType::kNodeAddedOrRemoved;
      } else {
        change = std::max(change, device_emulation_transform_node_->Update(
                                      *transform_parent, std::move(state)));
      }
      transform_parent = device_emulation_transform_node_.get();
    } else if (device_emulation_transform_node_) {
      device_emulation_transform_node_ = nullptr;
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
  }

  if (inner_viewport_container_layer_) {
    inner_viewport_container_layer_->SetLayerState(
        PropertyTreeState(*transform_parent, *clip_parent, *effect_parent),
        IntPoint());
  }

  {
    DCHECK(!transform_parent->IsInSubtreeOfPageScale());

    TransformPaintPropertyNode::State state;
    state.in_subtree_of_page_scale = false;
    state.compositor_element_id = GetCompositorOverscrollElasticityElementId();
    // TODO(crbug.com/877794) Should create overscroll elasticity transform node
    // based on settings.
    if (!overscroll_elasticity_transform_node_) {
      overscroll_elasticity_transform_node_ =
          TransformPaintPropertyNode::Create(*transform_parent,
                                             std::move(state));
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    } else {
      change = std::max(change, overscroll_elasticity_transform_node_->Update(
                                    *transform_parent, std::move(state)));
    }
  }

  {
    TransformPaintPropertyNode::State state{
        TransformationMatrix().Scale(Scale())};
    state.in_subtree_of_page_scale = false;
    state.compositor_element_id = GetCompositorElementId();

    if (!scale_transform_node_) {
      scale_transform_node_ = TransformPaintPropertyNode::Create(
          *overscroll_elasticity_transform_node_.get(), std::move(state));
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    } else {
      auto effective_change_type = scale_transform_node_->Update(
          *overscroll_elasticity_transform_node_.get(), std::move(state));
      // As an optimization, attempt to directly update the compositor
      // scale translation node and return kChangedOnlyCompositedValues which
      // avoids an expensive PaintArtifactCompositor update.
      // TODO(crbug.com/953322): We need to implement this optimization for
      // CompositeAfterPaint as well.
      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
          effective_change_type ==
              PaintPropertyChangeType::kChangedOnlySimpleValues) {
        if (auto* paint_artifact_compositor = GetPaintArtifactCompositor()) {
          bool updated =
              paint_artifact_compositor->DirectlyUpdatePageScaleTransform(
                  *scale_transform_node_);
          if (updated) {
            effective_change_type =
                PaintPropertyChangeType::kChangedOnlyCompositedValues;
            scale_transform_node_->CompositorSimpleValuesUpdated();
          }
        }
      }
      change = std::max(change, effective_change_type);
    }
  }

  if (page_scale_layer_) {
    page_scale_layer_->SetLayerState(
        PropertyTreeState(*scale_transform_node_, *clip_parent, *effect_parent),
        IntPoint());
  }

  {
    ScrollPaintPropertyNode::State state;
    state.container_rect = IntRect(IntPoint(), size_);
    state.contents_size = ContentsSize();

    state.user_scrollable_horizontal =
        UserInputScrollable(kHorizontalScrollbar);
    state.user_scrollable_vertical = UserInputScrollable(kVerticalScrollbar);
    state.scrolls_inner_viewport = true;
    state.max_scroll_offset_affected_by_page_scale = true;
    state.compositor_element_id = GetCompositorScrollElementId();

    if (MainFrame() &&
        !MainFrame()->GetSettings()->GetThreadedScrollingEnabled()) {
      state.main_thread_scrolling_reasons =
          cc::MainThreadScrollingReason::kThreadedScrollingDisabled;
    }

    if (!scroll_node_) {
      scroll_node_ =
          ScrollPaintPropertyNode::Create(*scroll_parent, std::move(state));
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    } else {
      change = std::max(change,
                        scroll_node_->Update(*scroll_parent, std::move(state)));
    }
  }

  {
    ScrollOffset scroll_position = GetScrollOffset();
    TransformPaintPropertyNode::State state{
        FloatSize(-scroll_position.Width(), -scroll_position.Height())};
    state.scroll = scroll_node_;
    if (!translation_transform_node_) {
      translation_transform_node_ = TransformPaintPropertyNode::Create(
          *scale_transform_node_, std::move(state));
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    } else {
      auto effective_change_type = translation_transform_node_->Update(
          *scale_transform_node_, std::move(state));
      // As an optimization, attempt to directly update the compositor
      // translation node and return kChangedOnlyCompositedValues which avoids
      // an expensive PaintArtifactCompositor update.
      // TODO(crbug.com/953322): We need to implement this optimization for
      // CompositeAfterPaint as well.
      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
          effective_change_type ==
              PaintPropertyChangeType::kChangedOnlySimpleValues) {
        if (auto* paint_artifact_compositor = GetPaintArtifactCompositor()) {
          bool updated =
              paint_artifact_compositor->DirectlyUpdateScrollOffsetTransform(
                  *translation_transform_node_);
          if (updated) {
            effective_change_type =
                PaintPropertyChangeType::kChangedOnlyCompositedValues;
            translation_transform_node_->CompositorSimpleValuesUpdated();
          }
        }
      }
    }
  }

  if (inner_viewport_scroll_layer_) {
    inner_viewport_scroll_layer_->SetLayerState(
        PropertyTreeState(*translation_transform_node_, *clip_parent,
                          *effect_parent),
        IntPoint());
  }

  if (overlay_scrollbar_horizontal_) {
    EffectPaintPropertyNode::State state;
    state.local_transform_space = transform_parent;
    state.direct_compositing_reasons =
        CompositingReason::kActiveOpacityAnimation;
    state.has_active_opacity_animation = true;
    state.compositor_element_id =
        GetScrollbarElementId(ScrollbarOrientation::kHorizontalScrollbar);
    if (!horizontal_scrollbar_effect_node_) {
      horizontal_scrollbar_effect_node_ =
          EffectPaintPropertyNode::Create(*effect_parent, std::move(state));
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    } else {
      change = std::max(change, horizontal_scrollbar_effect_node_->Update(
                                    *effect_parent, std::move(state)));
    }

    overlay_scrollbar_horizontal_->SetLayerState(
        PropertyTreeState(*transform_parent, *context.current.clip,
                          *horizontal_scrollbar_effect_node_),
        ScrollbarOffset(ScrollbarOrientation::kHorizontalScrollbar));
  }

  if (overlay_scrollbar_vertical_) {
    EffectPaintPropertyNode::State state;
    state.local_transform_space = transform_parent;
    state.direct_compositing_reasons =
        CompositingReason::kActiveOpacityAnimation;
    state.has_active_opacity_animation = true;
    state.compositor_element_id =
        GetScrollbarElementId(ScrollbarOrientation::kVerticalScrollbar);
    if (!vertical_scrollbar_effect_node_) {
      vertical_scrollbar_effect_node_ =
          EffectPaintPropertyNode::Create(*effect_parent, std::move(state));
      change = PaintPropertyChangeType::kNodeAddedOrRemoved;
    } else {
      change = std::max(change, vertical_scrollbar_effect_node_->Update(
                                    *effect_parent, std::move(state)));
    }

    overlay_scrollbar_vertical_->SetLayerState(
        PropertyTreeState(*transform_parent, *context.current.clip,
                          *vertical_scrollbar_effect_node_),
        ScrollbarOffset(ScrollbarOrientation::kVerticalScrollbar));
  }

  return change;
}

VisualViewport::~VisualViewport() {
  SendUMAMetrics();
}

void VisualViewport::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
  ScrollableArea::Trace(visitor);
}

void VisualViewport::UpdateStyleAndLayout() const {
  if (!MainFrame())
    return;

  if (Document* document = MainFrame()->GetDocument())
    document->UpdateStyleAndLayout();
}

void VisualViewport::EnqueueScrollEvent() {
  if (Document* document = MainFrame()->GetDocument())
    document->EnqueueVisualViewportScrollEvent();
}

void VisualViewport::EnqueueResizeEvent() {
  if (Document* document = MainFrame()->GetDocument())
    document->EnqueueVisualViewportResizeEvent();
}

void VisualViewport::SetSize(const IntSize& size) {
  if (size_ == size)
    return;

  TRACE_EVENT2("blink", "VisualViewport::setSize", "width", size.Width(),
               "height", size.Height());
  size_ = size;
  needs_paint_property_update_ = true;

  TRACE_EVENT_INSTANT1("loading", "viewport", TRACE_EVENT_SCOPE_THREAD, "data",
                       ViewportToTracedValue());

  if (inner_viewport_container_layer_) {
    inner_viewport_container_layer_->SetSize(gfx::Size(size_));
    inner_viewport_scroll_layer_->CcLayer()->SetScrollable(
        static_cast<gfx::Size>(size_));

    // Need to re-compute sizes for the overlay scrollbars.
    if (overlay_scrollbar_horizontal_) {
      DCHECK(overlay_scrollbar_vertical_);
      SetupScrollbar(kHorizontalScrollbar);
      SetupScrollbar(kVerticalScrollbar);
    }
  }

  if (!MainFrame())
    return;

  EnqueueResizeEvent();
}

void VisualViewport::Reset() {
  SetScaleAndLocation(1, is_pinch_gesture_active_, FloatPoint());
}

void VisualViewport::MainFrameDidChangeSize() {
  TRACE_EVENT0("blink", "VisualViewport::mainFrameDidChangeSize");

  // In unit tests we may not have initialized the layer tree.
  if (inner_viewport_scroll_layer_)
    inner_viewport_scroll_layer_->SetSize(gfx::Size(ContentsSize()));

  needs_paint_property_update_ = true;
  ClampToBoundaries();
}

FloatRect VisualViewport::VisibleRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  FloatSize visible_size(size_);

  if (scrollbar_inclusion == kExcludeScrollbars)
    visible_size = FloatSize(ExcludeScrollbars(size_));

  visible_size.Expand(0, browser_controls_adjustment_);
  visible_size.Scale(1 / scale_);

  return FloatRect(FloatPoint(GetScrollOffset()), visible_size);
}

FloatPoint VisualViewport::ViewportCSSPixelsToRootFrame(
    const FloatPoint& point) const {
  // Note, this is in CSS Pixels so we don't apply scale.
  FloatPoint point_in_root_frame = point;
  point_in_root_frame.Move(GetScrollOffset());
  return point_in_root_frame;
}

void VisualViewport::SetLocation(const FloatPoint& new_location) {
  SetScaleAndLocation(scale_, is_pinch_gesture_active_, new_location);
}

void VisualViewport::Move(const ScrollOffset& delta) {
  SetLocation(FloatPoint(offset_ + delta));
}

void VisualViewport::SetScale(float scale) {
  SetScaleAndLocation(scale, is_pinch_gesture_active_, FloatPoint(offset_));
}

double VisualViewport::OffsetLeft() const {
  if (!MainFrame())
    return 0;

  UpdateStyleAndLayout();

  return VisibleRect().X() / MainFrame()->PageZoomFactor();
}

double VisualViewport::OffsetTop() const {
  if (!MainFrame())
    return 0;

  UpdateStyleAndLayout();

  return VisibleRect().Y() / MainFrame()->PageZoomFactor();
}

double VisualViewport::Width() const {
  UpdateStyleAndLayout();
  return VisibleWidthCSSPx();
}

double VisualViewport::Height() const {
  UpdateStyleAndLayout();
  return VisibleHeightCSSPx();
}

double VisualViewport::ScaleForVisualViewport() const {
  return Scale();
}

void VisualViewport::SetScaleAndLocation(float scale,
                                         bool is_pinch_gesture_active,
                                         const FloatPoint& location) {
  if (DidSetScaleOrLocation(scale, is_pinch_gesture_active, location)) {
    NotifyRootFrameViewport();
    Document* document = MainFrame()->GetDocument();
    if (AXObjectCache* cache = document->ExistingAXObjectCache()) {
      cache->HandleScaleAndLocationChanged(document);
    }
  }
}

double VisualViewport::VisibleWidthCSSPx() const {
  if (!MainFrame())
    return 0;

  float zoom = MainFrame()->PageZoomFactor();
  float width_css_px = VisibleRect().Width() / zoom;
  return width_css_px;
}

double VisualViewport::VisibleHeightCSSPx() const {
  if (!MainFrame())
    return 0;

  float zoom = MainFrame()->PageZoomFactor();
  float height_css_px = VisibleRect().Height() / zoom;
  return height_css_px;
}

bool VisualViewport::DidSetScaleOrLocation(float scale,
                                           bool is_pinch_gesture_active,
                                           const FloatPoint& location) {
  if (!MainFrame())
    return false;

  bool values_changed = false;

  bool notify_page_scale_factor_changed =
      is_pinch_gesture_active_ != is_pinch_gesture_active;
  is_pinch_gesture_active_ = is_pinch_gesture_active;
  if (!std::isnan(scale) && !std::isinf(scale)) {
    float clamped_scale = GetPage()
                              .GetPageScaleConstraintsSet()
                              .FinalConstraints()
                              .ClampToConstraints(scale);
    if (clamped_scale != scale_) {
      scale_ = clamped_scale;
      values_changed = true;
      notify_page_scale_factor_changed = true;
      EnqueueResizeEvent();
    }
  }
  if (notify_page_scale_factor_changed)
    GetPage().GetChromeClient().PageScaleFactorChanged();

  ScrollOffset clamped_offset = ClampScrollOffset(ToScrollOffset(location));

  // TODO(bokan): If the offset is invalid, we might end up in an infinite
  // recursion as we reenter this function on clamping. It would be cleaner to
  // avoid reentrancy but for now just prevent the stack overflow.
  // crbug.com/702771.
  if (std::isnan(clamped_offset.Width()) ||
      std::isnan(clamped_offset.Height()) ||
      std::isinf(clamped_offset.Width()) || std::isinf(clamped_offset.Height()))
    return false;

  if (clamped_offset != offset_) {
    offset_ = clamped_offset;
    GetScrollAnimator().SetCurrentOffset(offset_);

    // SVG runs with accelerated compositing disabled so no
    // ScrollingCoordinator.
    if (ScrollingCoordinator* coordinator =
            GetPage().GetScrollingCoordinator()) {
      // In BGPT, scroll offsets and related properties are set directly on the
      // property trees so all we need to do is update the scroll offset on the
      // corresponding cc::Layer. Pre-BGPT we used ScrollLayerDidChange but
      // since all we need this for is to update the cc::Layer's offset, we now
      // use the more purpose-built UpdateCompositedScrollOffset, as in PLSA.
      if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
        coordinator->ScrollableAreaScrollLayerDidChange(this);
      else
        coordinator->UpdateCompositedScrollOffset(this);
    }

    EnqueueScrollEvent();

    MainFrame()->View()->DidChangeScrollOffset();
    values_changed = true;
  }

  if (!values_changed)
    return false;

  MainFrame()->GetEventHandler().MayUpdateHoverWhenContentUnderMouseChanged(
      MouseEventManager::UpdateHoverReason::kScrollOffsetChanged);

  probe::DidChangeViewport(MainFrame());
  MainFrame()->Loader().SaveScrollState();

  ClampToBoundaries();

  needs_paint_property_update_ = true;
  if (notify_page_scale_factor_changed) {
    TRACE_EVENT_INSTANT1("loading", "viewport", TRACE_EVENT_SCOPE_THREAD,
                         "data", ViewportToTracedValue());
  }
  return true;
}

void VisualViewport::CreateLayerTree() {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  if (inner_viewport_scroll_layer_)
    return;

  DCHECK(!overlay_scrollbar_horizontal_ && !overlay_scrollbar_vertical_ &&
         !overscroll_elasticity_layer_ && !page_scale_layer_ &&
         !inner_viewport_container_layer_);

  needs_paint_property_update_ = true;

  // FIXME: The root transform layer should only be created on demand.
  root_transform_layer_ = std::make_unique<GraphicsLayer>(*this);
  inner_viewport_container_layer_ = std::make_unique<GraphicsLayer>(*this);
  // TODO(crbug.com/836884) Should remove overscroll_elasticity_layer_ after
  // BGPT landed.
  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    overscroll_elasticity_layer_ = std::make_unique<GraphicsLayer>(*this);
    overscroll_elasticity_layer_->SetElementId(
        GetCompositorOverscrollElasticityElementId());
  }
  page_scale_layer_ = std::make_unique<GraphicsLayer>(*this);
  inner_viewport_scroll_layer_ = std::make_unique<GraphicsLayer>(*this);

  ScrollingCoordinator* coordinator = GetPage().GetScrollingCoordinator();
  DCHECK(coordinator);
  // Only used by the cc property tree builder and is not needed when blink
  // generates property trees.
  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() &&
      !RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    inner_viewport_scroll_layer_->SetIsContainerForFixedPositionLayers(true);
  }
  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
    coordinator->UpdateUserInputScrollable(this);

  // Set masks to bounds so the compositor doesn't clobber a manually
  // set inner viewport container layer size.
  inner_viewport_container_layer_->SetMasksToBounds(
      GetPage().GetSettings().GetMainFrameClipsContent());
  inner_viewport_container_layer_->SetSize(gfx::Size(size_));

  inner_viewport_scroll_layer_->CcLayer()->SetScrollable(
      static_cast<gfx::Size>(size_));
  DCHECK(MainFrame());
  DCHECK(MainFrame()->GetDocument());
  inner_viewport_scroll_layer_->SetElementId(GetCompositorScrollElementId());
  page_scale_layer_->SetElementId(GetCompositorElementId());

  root_transform_layer_->AddChild(inner_viewport_container_layer_.get());
  // TODO(crbug.com/836884) Should remove overscroll_elasticity_layer_ after
  // BGPT landed.
  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    inner_viewport_container_layer_->AddChild(
        overscroll_elasticity_layer_.get());
    overscroll_elasticity_layer_->AddChild(page_scale_layer_.get());
    page_scale_layer_->AddChild(inner_viewport_scroll_layer_.get());
  } else {
    inner_viewport_container_layer_->AddChild(page_scale_layer_.get());
    page_scale_layer_->AddChild(inner_viewport_scroll_layer_.get());
  }

  // Ensure this class is set as the scroll layer's ScrollableArea.
  coordinator->ScrollableAreaScrollLayerDidChange(this);

  InitializeScrollbars();
}

void VisualViewport::AttachLayerTree(GraphicsLayer* current_layer_tree_root) {
  TRACE_EVENT1("blink", "VisualViewport::attachLayerTree",
               "currentLayerTreeRoot", (bool)current_layer_tree_root);
  if (!current_layer_tree_root) {
    if (inner_viewport_scroll_layer_)
      inner_viewport_scroll_layer_->RemoveAllChildren();
    return;
  }

  if (current_layer_tree_root->Parent() &&
      current_layer_tree_root->Parent() == inner_viewport_scroll_layer_.get())
    return;

  DCHECK(inner_viewport_scroll_layer_);
  inner_viewport_scroll_layer_->RemoveAllChildren();
  inner_viewport_scroll_layer_->AddChild(current_layer_tree_root);
}

void VisualViewport::InitializeScrollbars() {
  // Do nothing if not attached to layer tree yet - will initialize upon attach.
  if (!inner_viewport_container_layer_)
    return;

  needs_paint_property_update_ = true;

  if (VisualViewportSuppliesScrollbars() &&
      !GetPage().GetSettings().GetHideScrollbars()) {
    DCHECK(!overlay_scrollbar_horizontal_);
    DCHECK(!overlay_scrollbar_vertical_);
    overlay_scrollbar_horizontal_ = std::make_unique<GraphicsLayer>(*this);
    overlay_scrollbar_vertical_ = std::make_unique<GraphicsLayer>(*this);
    SetupScrollbar(kHorizontalScrollbar);
    SetupScrollbar(kVerticalScrollbar);
  } else {
    overlay_scrollbar_horizontal_ = nullptr;
    overlay_scrollbar_vertical_ = nullptr;
  }

  // Ensure existing LocalFrameView scrollbars are removed if the visual
  // viewport scrollbars are now supplied, or created if the visual viewport no
  // longer supplies scrollbars.
  LocalFrame* frame = MainFrame();
  if (frame && frame->View())
    frame->View()->VisualViewportScrollbarsChanged();
}

int VisualViewport::ScrollbarThickness() const {
  ScrollbarThemeOverlay& theme = ScrollbarThemeOverlay::MobileTheme();
  int thickness = theme.ScrollbarThickness(kRegularScrollbar);
  return clampTo<int>(std::floor(
      GetPage().GetChromeClient().WindowToViewportScalar(thickness)));
}

IntSize VisualViewport::ScrollbarSize(ScrollbarOrientation orientation) const {
  if (orientation == kHorizontalScrollbar) {
    int viewport_width = inner_viewport_container_layer_->Size().width();
    return IntSize(viewport_width - ScrollbarThickness(), ScrollbarThickness());
  }
  int viewport_height = inner_viewport_container_layer_->Size().height();
  return IntSize(ScrollbarThickness(), viewport_height - ScrollbarThickness());
}

IntPoint VisualViewport::ScrollbarOffset(
    ScrollbarOrientation orientation) const {
  if (orientation == kHorizontalScrollbar) {
    int viewport_height = inner_viewport_container_layer_->Size().height();
    return IntPoint(0, viewport_height - ScrollbarThickness());
  }
  int viewport_width = inner_viewport_container_layer_->Size().width();
  return IntPoint(viewport_width - ScrollbarThickness(), 0);
}

void VisualViewport::SetupScrollbar(ScrollbarOrientation orientation) {
  bool is_horizontal = orientation == kHorizontalScrollbar;
  GraphicsLayer* scrollbar_graphics_layer =
      is_horizontal ? overlay_scrollbar_horizontal_.get()
                    : overlay_scrollbar_vertical_.get();
  std::unique_ptr<ScrollingCoordinator::ScrollbarLayerGroup>&
      scrollbar_layer_group = is_horizontal ? scrollbar_layer_group_horizontal_
                                            : scrollbar_layer_group_vertical_;
  if (!scrollbar_graphics_layer->Parent())
    inner_viewport_container_layer_->AddChild(scrollbar_graphics_layer);

  if (!scrollbar_layer_group) {
    ScrollingCoordinator* coordinator = GetPage().GetScrollingCoordinator();
    DCHECK(coordinator);

    ScrollbarThemeOverlay& theme = ScrollbarThemeOverlay::MobileTheme();
    int thumb_thickness = clampTo<int>(
        std::floor(GetPage().GetChromeClient().WindowToViewportScalar(
            theme.ThumbThickness())));
    int scrollbar_margin = clampTo<int>(
        std::floor(GetPage().GetChromeClient().WindowToViewportScalar(
            theme.ScrollbarMargin())));
    scrollbar_layer_group = coordinator->CreateSolidColorScrollbarLayer(
        orientation, thumb_thickness, scrollbar_margin, false,
        GetScrollbarElementId(orientation));

    // The compositor will control the scrollbar's visibility. Set to invisible
    // by default so scrollbars don't show up in web tests.
    scrollbar_layer_group->layer->SetOpacity(0.f);
    scrollbar_graphics_layer->SetContentsToCcLayer(
        scrollbar_layer_group->layer.get(),
        /*prevent_contents_opaque_changes=*/false);
    scrollbar_graphics_layer->SetDrawsContent(false);
    scrollbar_graphics_layer->SetHitTestable(false);
    scrollbar_layer_group->scrollbar_layer->SetScrollElementId(
        inner_viewport_scroll_layer_->CcLayer()->element_id());
  }

  // Use the GraphicsLayer to position the scrollbars.
  const auto& position = ScrollbarOffset(orientation);
  scrollbar_graphics_layer->SetPosition(FloatPoint(position));

  const auto& size = ScrollbarSize(orientation);
  scrollbar_graphics_layer->SetSize(gfx::Size(size));
  scrollbar_graphics_layer->SetContentsRect(IntRect(IntPoint(), size));

  needs_paint_property_update_ = true;
}

bool VisualViewport::VisualViewportSuppliesScrollbars() const {
  return GetPage().GetSettings().GetViewportEnabled();
}

CompositorElementId VisualViewport::GetCompositorElementId() const {
  return element_id_;
}

CompositorElementId VisualViewport::GetCompositorScrollElementId() const {
  return scroll_element_id_;
}

CompositorElementId VisualViewport::GetCompositorOverscrollElasticityElementId()
    const {
  return overscroll_elasticity_element_id_;
}

bool VisualViewport::ScrollAnimatorEnabled() const {
  return GetPage().GetSettings().GetScrollAnimatorEnabled();
}

ChromeClient* VisualViewport::GetChromeClient() const {
  return &GetPage().GetChromeClient();
}

SmoothScrollSequencer* VisualViewport::GetSmoothScrollSequencer() const {
  if (!MainFrame())
    return nullptr;
  return &MainFrame()->GetSmoothScrollSequencer();
}

void VisualViewport::SetScrollOffset(const ScrollOffset& offset,
                                     ScrollType scroll_type,
                                     ScrollBehavior scroll_behavior,
                                     ScrollCallback on_finish) {
  // We clamp the offset here, because the ScrollAnimator may otherwise be
  // set to a non-clamped offset by ScrollableArea::setScrollOffset,
  // which may lead to incorrect scrolling behavior in RootFrameViewport down
  // the line.
  // TODO(eseckler): Solve this instead by ensuring that ScrollableArea and
  // ScrollAnimator are kept in sync. This requires that ScrollableArea always
  // stores fractional offsets and that truncation happens elsewhere, see
  // crbug.com/626315.
  ScrollOffset new_scroll_offset = ClampScrollOffset(offset);
  ScrollableArea::SetScrollOffset(new_scroll_offset, scroll_type,
                                  scroll_behavior, std::move(on_finish));
}

PhysicalRect VisualViewport::ScrollIntoView(
    const PhysicalRect& rect_in_absolute,
    const WebScrollIntoViewParams& params) {
  PhysicalRect scroll_snapport_rect = VisibleScrollSnapportRect();

  ScrollOffset new_scroll_offset =
      ClampScrollOffset(ScrollAlignment::GetScrollOffsetToExpose(
          scroll_snapport_rect, rect_in_absolute, params.GetScrollAlignmentX(),
          params.GetScrollAlignmentY(), GetScrollOffset()));

  if (new_scroll_offset != GetScrollOffset()) {
    ScrollBehavior behavior = params.GetScrollBehavior();
    if (params.is_for_scroll_sequence) {
      DCHECK(params.GetScrollType() == kProgrammaticScroll ||
             params.GetScrollType() == kUserScroll);
      if (SmoothScrollSequencer* sequencer = GetSmoothScrollSequencer()) {
        sequencer->QueueAnimation(this, new_scroll_offset, behavior);
      }
    } else {
      SetScrollOffset(new_scroll_offset, params.GetScrollType(), behavior,
                      ScrollCallback());
    }
  }

  return rect_in_absolute;
}

int VisualViewport::ScrollSize(ScrollbarOrientation orientation) const {
  IntSize scroll_dimensions =
      MaximumScrollOffsetInt() - MinimumScrollOffsetInt();
  return (orientation == kHorizontalScrollbar) ? scroll_dimensions.Width()
                                               : scroll_dimensions.Height();
}

IntSize VisualViewport::MinimumScrollOffsetInt() const {
  return IntSize();
}

IntSize VisualViewport::MaximumScrollOffsetInt() const {
  return FlooredIntSize(MaximumScrollOffset());
}

ScrollOffset VisualViewport::MaximumScrollOffset() const {
  if (!MainFrame())
    return ScrollOffset();

  // TODO(bokan): We probably shouldn't be storing the bounds in a float.
  // crbug.com/470718.
  FloatSize frame_view_size(ContentsSize());

  if (browser_controls_adjustment_) {
    float min_scale =
        GetPage().GetPageScaleConstraintsSet().FinalConstraints().minimum_scale;
    frame_view_size.Expand(0, browser_controls_adjustment_ / min_scale);
  }

  frame_view_size.Scale(scale_);
  frame_view_size = FloatSize(FlooredIntSize(frame_view_size));

  FloatSize viewport_size(size_);
  viewport_size.Expand(0, ceilf(browser_controls_adjustment_));

  FloatSize max_position = frame_view_size - viewport_size;
  max_position.Scale(1 / scale_);
  return ScrollOffset(max_position);
}

IntPoint VisualViewport::ClampDocumentOffsetAtScale(const IntPoint& offset,
                                                    float scale) {
  if (!MainFrame() || !MainFrame()->View())
    return IntPoint();

  LocalFrameView* view = MainFrame()->View();

  FloatSize scaled_size(ExcludeScrollbars(size_));
  scaled_size.Scale(1 / scale);

  IntSize visual_viewport_max =
      FlooredIntSize(FloatSize(ContentsSize()) - scaled_size);
  IntSize max =
      view->LayoutViewport()->MaximumScrollOffsetInt() + visual_viewport_max;
  IntSize min =
      view->LayoutViewport()
          ->MinimumScrollOffsetInt();  // VisualViewportMin should be (0, 0)

  IntSize clamped = ToIntSize(offset);
  clamped = clamped.ShrunkTo(max);
  clamped = clamped.ExpandedTo(min);
  return IntPoint(clamped);
}

void VisualViewport::SetBrowserControlsAdjustment(float adjustment) {
  if (browser_controls_adjustment_ == adjustment)
    return;

  browser_controls_adjustment_ = adjustment;
  EnqueueResizeEvent();
}

float VisualViewport::BrowserControlsAdjustment() const {
  return browser_controls_adjustment_;
}

IntRect VisualViewport::ScrollableAreaBoundingBox() const {
  // This method should return the bounding box in the top-level
  // LocalFrameView's coordinate space; however, VisualViewport technically
  // isn't a child of any Frames.  Nonetheless, the VisualViewport always
  // occupies the entire main frame so just return that.
  LocalFrame* frame = MainFrame();

  if (!frame || !frame->View())
    return IntRect();

  return frame->View()->FrameRect();
}

bool VisualViewport::UserInputScrollable(ScrollbarOrientation) const {
  // If there is a non-root fullscreen element, prevent the viewport from
  // scrolling.
  Document* main_document = MainFrame() ? MainFrame()->GetDocument() : nullptr;
  if (main_document) {
    Element* fullscreen_element =
        Fullscreen::FullscreenElementFrom(*main_document);
    if (fullscreen_element)
      return false;
  }
  return true;
}

IntSize VisualViewport::ContentsSize() const {
  LocalFrame* frame = MainFrame();
  if (!frame || !frame->View())
    return IntSize();

  return frame->View()->Size();
}

IntRect VisualViewport::VisibleContentRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  return EnclosingIntRect(VisibleRect(scrollbar_inclusion));
}

scoped_refptr<base::SingleThreadTaskRunner> VisualViewport::GetTimerTaskRunner()
    const {
  return MainFrame()->GetTaskRunner(TaskType::kInternalDefault);
}

void VisualViewport::UpdateScrollOffset(const ScrollOffset& position,
                                        ScrollType scroll_type) {
  if (!DidSetScaleOrLocation(scale_, is_pinch_gesture_active_,
                             FloatPoint(position))) {
    return;
  }
  if (IsExplicitScrollType(scroll_type)) {
    NotifyRootFrameViewport();
    if (scroll_type != kCompositorScroll && LayerForScrolling())
      LayerForScrolling()->CcLayer()->ShowScrollbars();
  }
}

GraphicsLayer* VisualViewport::LayerForContainer() const {
  return inner_viewport_container_layer_.get();
}

GraphicsLayer* VisualViewport::LayerForScrolling() const {
  return inner_viewport_scroll_layer_.get();
}

GraphicsLayer* VisualViewport::LayerForHorizontalScrollbar() const {
  return overlay_scrollbar_horizontal_.get();
}

GraphicsLayer* VisualViewport::LayerForVerticalScrollbar() const {
  return overlay_scrollbar_vertical_.get();
}

IntRect VisualViewport::ComputeInterestRect(const GraphicsLayer*,
                                            const IntRect&) const {
  return IntRect();
}

void VisualViewport::PaintContents(const GraphicsLayer*,
                                   GraphicsContext&,
                                   GraphicsLayerPaintingPhase,
                                   const IntRect&) const {}

RootFrameViewport* VisualViewport::GetRootFrameViewport() const {
  if (!MainFrame() || !MainFrame()->View())
    return nullptr;

  return MainFrame()->View()->GetRootFrameViewport();
}

LocalFrame* VisualViewport::MainFrame() const {
  return GetPage().MainFrame() && GetPage().MainFrame()->IsLocalFrame()
             ? GetPage().DeprecatedLocalMainFrame()
             : nullptr;
}

IntSize VisualViewport::ExcludeScrollbars(const IntSize& size) const {
  IntSize excluded_size = size;
  if (RootFrameViewport* root_frame_viewport = GetRootFrameViewport()) {
    excluded_size.Expand(-root_frame_viewport->VerticalScrollbarWidth(),
                         -root_frame_viewport->HorizontalScrollbarHeight());
  }
  return excluded_size;
}

bool VisualViewport::ScheduleAnimation() {
  GetPage().GetChromeClient().ScheduleAnimation(MainFrame()->View());
  return true;
}

void VisualViewport::ClampToBoundaries() {
  SetLocation(FloatPoint(offset_));
}

FloatRect VisualViewport::ViewportToRootFrame(
    const FloatRect& rect_in_viewport) const {
  FloatRect rect_in_root_frame = rect_in_viewport;
  rect_in_root_frame.Scale(1 / Scale());
  rect_in_root_frame.Move(GetScrollOffset());
  return rect_in_root_frame;
}

IntRect VisualViewport::ViewportToRootFrame(
    const IntRect& rect_in_viewport) const {
  // FIXME: How to snap to pixels?
  return EnclosingIntRect(ViewportToRootFrame(FloatRect(rect_in_viewport)));
}

FloatRect VisualViewport::RootFrameToViewport(
    const FloatRect& rect_in_root_frame) const {
  FloatRect rect_in_viewport = rect_in_root_frame;
  rect_in_viewport.Move(-GetScrollOffset());
  rect_in_viewport.Scale(Scale());
  return rect_in_viewport;
}

IntRect VisualViewport::RootFrameToViewport(
    const IntRect& rect_in_root_frame) const {
  // FIXME: How to snap to pixels?
  return EnclosingIntRect(RootFrameToViewport(FloatRect(rect_in_root_frame)));
}

FloatPoint VisualViewport::ViewportToRootFrame(
    const FloatPoint& point_in_viewport) const {
  FloatPoint point_in_root_frame = point_in_viewport;
  point_in_root_frame.Scale(1 / Scale(), 1 / Scale());
  point_in_root_frame.Move(GetScrollOffset());
  return point_in_root_frame;
}

FloatPoint VisualViewport::RootFrameToViewport(
    const FloatPoint& point_in_root_frame) const {
  FloatPoint point_in_viewport = point_in_root_frame;
  point_in_viewport.Move(-GetScrollOffset());
  point_in_viewport.Scale(Scale(), Scale());
  return point_in_viewport;
}

IntPoint VisualViewport::ViewportToRootFrame(
    const IntPoint& point_in_viewport) const {
  // FIXME: How to snap to pixels?
  return FlooredIntPoint(
      FloatPoint(ViewportToRootFrame(FloatPoint(point_in_viewport))));
}

IntPoint VisualViewport::RootFrameToViewport(
    const IntPoint& point_in_root_frame) const {
  // FIXME: How to snap to pixels?
  return FlooredIntPoint(
      FloatPoint(RootFrameToViewport(FloatPoint(point_in_root_frame))));
}

void VisualViewport::StartTrackingPinchStats() {
  if (!MainFrame())
    return;

  Document* document = MainFrame()->GetDocument();
  if (!document)
    return;

  if (!document->Url().ProtocolIsInHTTPFamily())
    return;

  track_pinch_zoom_stats_for_page_ = !ShouldDisableDesktopWorkarounds();
}

void VisualViewport::UserDidChangeScale() {
  if (!track_pinch_zoom_stats_for_page_)
    return;

  max_page_scale_ = std::max(max_page_scale_, scale_);
}

void VisualViewport::SendUMAMetrics() {
  if (track_pinch_zoom_stats_for_page_) {
    bool did_scale = max_page_scale_ > 0;

    DEFINE_STATIC_LOCAL(EnumerationHistogram, did_scale_histogram,
                        ("Viewport.DidScalePage", 2));
    did_scale_histogram.Count(did_scale ? 1 : 0);

    if (did_scale) {
      int zoom_percentage = floor(max_page_scale_ * 100);

      // See the PageScaleFactor enumeration in histograms.xml for the bucket
      // ranges.
      int bucket = floor(zoom_percentage / 25.f);

      DEFINE_STATIC_LOCAL(EnumerationHistogram, max_scale_histogram,
                          ("Viewport.MaxPageScale", 21));
      max_scale_histogram.Count(bucket);
    }
  }

  max_page_scale_ = -1;
  track_pinch_zoom_stats_for_page_ = false;
}

bool VisualViewport::ShouldDisableDesktopWorkarounds() const {
  if (!MainFrame() || !MainFrame()->View())
    return false;

  if (!MainFrame()->GetSettings()->GetViewportEnabled())
    return false;

  // A document is considered adapted to small screen UAs if one of these holds:
  // 1. The author specified viewport has a constrained width that is equal to
  //    the initial viewport width.
  // 2. The author has disabled viewport zoom.
  const PageScaleConstraints& constraints =
      GetPage().GetPageScaleConstraintsSet().PageDefinedConstraints();

  return MainFrame()->View()->GetLayoutSize().Width() == size_.Width() ||
         (constraints.minimum_scale == constraints.maximum_scale &&
          constraints.minimum_scale != -1);
}

cc::AnimationHost* VisualViewport::GetCompositorAnimationHost() const {
  DCHECK(GetPage().MainFrame()->IsLocalFrame());
  ScrollingCoordinator* c = GetPage().GetScrollingCoordinator();
  return c ? c->GetCompositorAnimationHost() : nullptr;
}

CompositorAnimationTimeline* VisualViewport::GetCompositorAnimationTimeline()
    const {
  DCHECK(GetPage().MainFrame()->IsLocalFrame());
  ScrollingCoordinator* c = GetPage().GetScrollingCoordinator();
  return c ? c->GetCompositorAnimationTimeline() : nullptr;
}

void VisualViewport::NotifyRootFrameViewport() const {
  if (!GetRootFrameViewport())
    return;

  GetRootFrameViewport()->DidUpdateVisualViewport();
}

ScrollbarTheme& VisualViewport::GetPageScrollbarTheme() const {
  return GetPage().GetScrollbarTheme();
}

void VisualViewport::SetOverlayScrollbarsHidden(bool hidden) {
  ScrollableArea::SetScrollbarsHiddenIfOverlay(hidden);
}

void VisualViewport::GraphicsLayersDidChange() {
  if (MainFrame() && MainFrame()->View())
    MainFrame()->View()->GraphicsLayersDidChange();
}

PaintArtifactCompositor* VisualViewport::GetPaintArtifactCompositor() const {
  if (MainFrame() && MainFrame()->View())
    return MainFrame()->View()->GetPaintArtifactCompositor();
  return nullptr;
}

String VisualViewport::DebugName(const GraphicsLayer* graphics_layer) const {
  String name;
  if (graphics_layer == inner_viewport_container_layer_.get()) {
    name = "Inner Viewport Container Layer";
  } else if (graphics_layer == overscroll_elasticity_layer_.get()) {
    name = "Overscroll Elasticity Layer";
  } else if (graphics_layer == page_scale_layer_.get()) {
    name = "Page Scale Layer";
  } else if (graphics_layer == inner_viewport_scroll_layer_.get()) {
    name = "Inner Viewport Scroll Layer";
  } else if (graphics_layer == overlay_scrollbar_horizontal_.get()) {
    name = "Overlay Scrollbar Horizontal Layer";
  } else if (graphics_layer == overlay_scrollbar_vertical_.get()) {
    name = "Overlay Scrollbar Vertical Layer";
  } else if (graphics_layer == root_transform_layer_.get()) {
    name = "Root Transform Layer";
  } else {
    NOTREACHED();
  }

  return name;
}

const ScrollableArea* VisualViewport::GetScrollableAreaForTesting(
    const GraphicsLayer* layer) const {
  if (layer == inner_viewport_scroll_layer_.get())
    return this;
  return nullptr;
}

std::unique_ptr<TracedValue> VisualViewport::ViewportToTracedValue() const {
  auto value = std::make_unique<TracedValue>();
  IntRect viewport = VisibleContentRect();
  value->SetInteger("x", clampTo<int>(roundf(viewport.X())));
  value->SetInteger("y", clampTo<int>(roundf(viewport.Y())));
  value->SetInteger("width", clampTo<int>(roundf(viewport.Width())));
  value->SetInteger("height", clampTo<int>(roundf(viewport.Height())));
  value->SetString("frameID",
                   IdentifiersFactory::FrameId(GetPage().MainFrame()));
  return value;
}

void VisualViewport::DisposeImpl() {
  root_transform_layer_.reset();
  inner_viewport_container_layer_.reset();
  overscroll_elasticity_layer_.reset();
  page_scale_layer_.reset();
  inner_viewport_scroll_layer_.reset();
  // scrollbar_layer_group_* are referenced from overlay_scrollbar_*, thus
  // overlay_scrollbar_* must be destroyed before scrollbar_layer_group_*.
  overlay_scrollbar_horizontal_.reset();
  overlay_scrollbar_vertical_.reset();
  scrollbar_layer_group_horizontal_.reset();
  scrollbar_layer_group_vertical_.reset();
  device_emulation_transform_node_.reset();
  overscroll_elasticity_transform_node_.reset();
  scale_transform_node_.reset();
  translation_transform_node_.reset();
  scroll_node_.reset();
  horizontal_scrollbar_effect_node_.reset();
  vertical_scrollbar_effect_node_.reset();
}

}  // namespace blink
