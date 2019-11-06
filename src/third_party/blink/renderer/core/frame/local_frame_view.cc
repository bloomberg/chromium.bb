/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/frame/local_frame_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/renderer/core/accessibility/apply_dark_mode.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/editing/compute_layer_selection.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/frame_overlay.h"
#include "third_party/blink/renderer/core/frame/frame_view_auto_size_info.h"
#include "third_party/blink/renderer/core/frame/link_highlights.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/scroll_into_view_options.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html/portal/document_portals.h"
#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/style_retain_scope.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/layout/traced_layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_util.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator_context.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/paint/block_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_inputs_updater.h"
#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_tree_as_text.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/frame_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_controller.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/geometry/double_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/link_highlight.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

// Used to check for dirty layouts violating document lifecycle rules.
// If arg evaluates to true, the program will continue. If arg evaluates to
// false, program will crash if DCHECK_IS_ON() or return false from the current
// function.
#define CHECK_FOR_DIRTY_LAYOUT(arg) \
  do {                              \
    if (!(arg)) {                   \
      NOTREACHED();                 \
      return false;                 \
    }                               \
  } while (false)

namespace blink {
namespace {

// Page dimensions in pixels at 72 DPI.
constexpr int kA4PortraitPageWidth = 595;
constexpr int kA4PortraitPageHeight = 842;
constexpr int kLetterPortraitPageWidth = 612;
constexpr int kLetterPortraitPageHeight = 792;

// Logs a UseCounter for the size of the cursor that will be set. This will be
// used for compatibility analysis to determine whether the maximum size can be
// reduced.
void LogCursorSizeCounter(LocalFrame* frame, const Cursor& cursor) {
  DCHECK(frame);
  Image* image = cursor.GetImage();
  if (!image)
    return;
  // Should not overflow, this calculation is done elsewhere when determining
  // whether the cursor exceeds its maximum size (see event_handler.cc).
  IntSize scaled_size = image->Size();
  scaled_size.Scale(1 / cursor.ImageScaleFactor());
  if (scaled_size.Width() > 64 || scaled_size.Height() > 64) {
    UseCounter::Count(frame->GetDocument(), WebFeature::kCursorImageGT64x64);
  } else if (scaled_size.Width() > 32 || scaled_size.Height() > 32) {
    UseCounter::Count(frame->GetDocument(), WebFeature::kCursorImageGT32x32);
  } else {
    UseCounter::Count(frame->GetDocument(), WebFeature::kCursorImageLE32x32);
  }
}

// Default value and parameter name for how long we want to delay the
// compositor commit beyond the start of document lifdecycle updates to avoid
// flash between navigations. The delay should be small enough so that it won't
// confuse users expecting a new page to appear after navigation and the omnibar
// has updated the url display.
constexpr int kAvoidFlashCommitDelayDefaultInMs = 500;  // 30 frames @ 60hz
constexpr char kAvoidFlashCommitDelayParameterName[] = "commit_delay";

// Get the field trial parameter value for AvoidFlashBetweenNavigation.
base::TimeDelta GetCommitDelayForAvoidFlashBetweenNavigation() {
  DCHECK(base::FeatureList::IsEnabled(
      blink::features::kAvoidFlashBetweenNavigation));
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kAvoidFlashBetweenNavigation,
          kAvoidFlashCommitDelayParameterName,
          kAvoidFlashCommitDelayDefaultInMs));
}

}  // namespace

// The maximum number of updatePlugins iterations that should be done before
// returning.
static const unsigned kMaxUpdatePluginsIterations = 2;

static bool g_initial_track_all_paint_invalidations = false;

LocalFrameView::LocalFrameView(LocalFrame& frame)
    : LocalFrameView(frame, IntRect()) {
  Show();
}

LocalFrameView::LocalFrameView(LocalFrame& frame, const IntSize& initial_size)
    : LocalFrameView(frame, IntRect(IntPoint(), initial_size)) {
  SetLayoutSizeInternal(initial_size);
  Show();
}

LocalFrameView::LocalFrameView(LocalFrame& frame, IntRect frame_rect)
    : FrameView(frame_rect),
      frame_(frame),
      display_mode_(kWebDisplayModeBrowser),
      can_have_scrollbars_(true),
      has_pending_layout_(false),
      layout_scheduling_enabled_(true),
      layout_count_for_testing_(0),
      lifecycle_update_count_for_testing_(0),
      nested_layout_count_(0),
      update_plugins_timer_(frame.GetTaskRunner(TaskType::kInternalDefault),
                            this,
                            &LocalFrameView::UpdatePluginsTimerFired),
      first_layout_(true),
      base_background_color_(Color::kWhite),
      last_zoom_factor_(1.0f),
      media_type_(media_type_names::kScreen),
      safe_to_propagate_scroll_to_parent_(true),
      visually_non_empty_character_count_(0),
      visually_non_empty_pixel_count_(0),
      is_visually_non_empty_(false),
      sticky_position_object_count_(0),
      layout_size_fixed_to_frame_size_(true),
      needs_update_geometries_(false),
      root_layer_did_scroll_(false),
      frame_timing_requests_dirty_(true),
      // The compositor throttles the main frame using deferred commits, we
      // can't throttle it here or it seems the root compositor doesn't get
      // setup properly.
      lifecycle_updates_throttled_(!GetFrame().IsMainFrame()),
      current_update_lifecycle_phases_target_state_(
          DocumentLifecycle::kUninitialized),
      past_layout_lifecycle_update_(false),
      suppress_adjust_view_size_(false),
      intersection_observation_state_(kNotNeeded),
      needs_forced_compositing_update_(false),
      needs_focus_on_fragment_(false),
      in_lifecycle_update_(false),
      tracked_object_paint_invalidations_(
          base::WrapUnique(g_initial_track_all_paint_invalidations
                               ? new Vector<ObjectPaintInvalidation>
                               : nullptr)),
      main_thread_scrolling_reasons_(0),
      forced_layout_stack_depth_(0),
      forced_layout_start_time_(base::TimeTicks()),
      paint_frame_count_(0),
      unique_id_(NewUniqueObjectId()),
      layout_shift_tracker_(std::make_unique<LayoutShiftTracker>(this)),
      paint_timing_detector_(MakeGarbageCollected<PaintTimingDetector>(this))
#if DCHECK_IS_ON()
      ,
      is_updating_descendant_dependent_flags_(false)
#endif
{
  // Propagate the marginwidth/height and scrolling modes to the view.
  if (frame_->Owner() &&
      frame_->Owner()->ScrollingMode() == ScrollbarMode::kAlwaysOff)
    SetCanHaveScrollbars(false);
}

LocalFrameView::~LocalFrameView() {
#if DCHECK_IS_ON()
  DCHECK(has_been_disposed_);
#endif
}

void LocalFrameView::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(fragment_anchor_);
  visitor->Trace(scrollable_areas_);
  visitor->Trace(animating_scrollable_areas_);
  visitor->Trace(auto_size_info_);
  visitor->Trace(plugins_);
  visitor->Trace(scrollbars_);
  visitor->Trace(viewport_scrollable_area_);
  visitor->Trace(anchoring_adjustment_queue_);
  visitor->Trace(print_context_);
  visitor->Trace(paint_timing_detector_);
  visitor->Trace(lifecycle_observers_);
}

template <typename Function>
void LocalFrameView::ForAllChildViewsAndPlugins(const Function& function) {
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (child->View())
      function(*child->View());
  }

  for (const auto& plugin : plugins_) {
    function(*plugin);
  }

  if (Document* document = frame_->GetDocument()) {
    for (HTMLPortalElement* portal :
         DocumentPortals::From(*document).GetPortals()) {
      if (Frame* frame = portal->ContentFrame())
        function(*frame->View());
    }
  }
}

template <typename Function>
void LocalFrameView::ForAllChildLocalFrameViews(const Function& function) {
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (!child_local_frame)
      continue;
    if (LocalFrameView* child_view = child_local_frame->View())
      function(*child_view);
  }
}

// Call function for each non-throttled frame view in pre tree order.
template <typename Function>
void LocalFrameView::ForAllNonThrottledLocalFrameViews(
    const Function& function) {
  if (ShouldThrottleRendering())
    return;

  function(*this);

  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (!child_local_frame)
      continue;
    if (LocalFrameView* child_view = child_local_frame->View())
      child_view->ForAllNonThrottledLocalFrameViews(function);
  }
}

void LocalFrameView::Dispose() {
  CHECK(!IsInPerformLayout());

  // TODO(dcheng): It's wrong that the frame can be detached before the
  // LocalFrameView. Figure out what's going on and fix LocalFrameView to be
  // disposed with the correct timing.

  // We need to clear the RootFrameViewport's animator since it gets called
  // from non-GC'd objects and RootFrameViewport will still have a pointer to
  // this class.
  if (viewport_scrollable_area_)
    viewport_scrollable_area_->ClearScrollableArea();

  // Destroy |m_autoSizeInfo| as early as possible, to avoid dereferencing
  // partially destroyed |this| via |m_autoSizeInfo->m_frameView|.
  auto_size_info_.Clear();

  // FIXME: Do we need to do something here for OOPI?
  HTMLFrameOwnerElement* owner_element = frame_->DeprecatedLocalOwner();
  // TODO(dcheng): It seems buggy that we can have an owner element that points
  // to another EmbeddedContentView. This can happen when a plugin element loads
  // a frame (EmbeddedContentView A of type LocalFrameView) and then loads a
  // plugin (EmbeddedContentView B of type WebPluginContainerImpl). In this
  // case, the frame's view is A and the frame element's
  // OwnedEmbeddedContentView is B. See https://crbug.com/673170 for an example.
  if (owner_element && owner_element->OwnedEmbeddedContentView() == this)
    owner_element->SetEmbeddedContentView(nullptr);

  ClearPrintContext();

  ukm_aggregator_.reset();
  layout_shift_tracker_->Dispose();

#if DCHECK_IS_ON()
  has_been_disposed_ = true;
#endif
}

void LocalFrameView::InvalidateAllCustomScrollbarsOnActiveChanged() {
  bool uses_window_inactive_selector =
      frame_->GetDocument()->GetStyleEngine().UsesWindowInactiveSelector();

  ForAllChildLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.InvalidateAllCustomScrollbarsOnActiveChanged();
  });

  for (const auto& scrollbar : scrollbars_) {
    if (uses_window_inactive_selector && scrollbar->IsCustomScrollbar())
      scrollbar->StyleChanged();
  }
}

bool LocalFrameView::DidFirstLayout() const {
  return !first_layout_;
}

bool LocalFrameView::LifecycleUpdatesActive() const {
  return !lifecycle_updates_throttled_;
}

void LocalFrameView::SetLifecycleUpdatesThrottledForTesting() {
  lifecycle_updates_throttled_ = true;
}

void LocalFrameView::InvalidateRect(const IntRect& rect) {
  auto* layout_object = GetLayoutEmbeddedContent();
  if (!layout_object)
    return;

  IntRect paint_invalidation_rect = rect;
  paint_invalidation_rect.Move(
      (layout_object->BorderLeft() + layout_object->PaddingLeft()).ToInt(),
      (layout_object->BorderTop() + layout_object->PaddingTop()).ToInt());
  layout_object->InvalidatePaintRectangle(
      PhysicalRect(paint_invalidation_rect));
}

void LocalFrameView::FrameRectsChanged(const IntRect& old_rect) {
  const bool width_changed = Size().Width() != old_rect.Width();
  const bool height_changed = Size().Height() != old_rect.Height();

  PropagateFrameRects();

  if (FrameRect() != old_rect) {
    if (auto* layout_view = GetLayoutView())
      layout_view->SetShouldCheckForPaintInvalidation();
  }

  if (width_changed || height_changed) {
    ViewportSizeChanged(width_changed, height_changed);
    if (frame_->IsMainFrame())
      frame_->GetPage()->GetVisualViewport().MainFrameDidChangeSize();
    GetFrame().Loader().RestoreScrollPositionAndViewState();
  }
}

Page* LocalFrameView::GetPage() const {
  return GetFrame().GetPage();
}

LayoutView* LocalFrameView::GetLayoutView() const {
  return GetFrame().ContentLayoutObject();
}

ScrollingCoordinator* LocalFrameView::GetScrollingCoordinator() const {
  Page* p = GetPage();
  return p ? p->GetScrollingCoordinator() : nullptr;
}

ScrollingCoordinatorContext* LocalFrameView::GetScrollingContext() const {
  LocalFrame* root = &GetFrame().LocalFrameRoot();
  if (GetFrame() != root)
    return root->View()->GetScrollingContext();

  if (!scrolling_context_)
    scrolling_context_.reset(new ScrollingCoordinatorContext());
  return scrolling_context_.get();
}

cc::AnimationHost* LocalFrameView::GetCompositorAnimationHost() const {
  if (GetScrollingContext()->GetCompositorAnimationHost())
    return GetScrollingContext()->GetCompositorAnimationHost();

  if (!GetFrame().LocalFrameRoot().IsMainFrame())
    return nullptr;

  // TODO(kenrb): Compositor animation host and timeline for the main frame
  // still live on ScrollingCoordinator. https://crbug.com/680606.
  ScrollingCoordinator* c = GetScrollingCoordinator();
  return c ? c->GetCompositorAnimationHost() : nullptr;
}

CompositorAnimationTimeline* LocalFrameView::GetCompositorAnimationTimeline()
    const {
  if (GetScrollingContext()->GetCompositorAnimationTimeline())
    return GetScrollingContext()->GetCompositorAnimationTimeline();

  if (!GetFrame().LocalFrameRoot().IsMainFrame())
    return nullptr;

  // TODO(kenrb): Compositor animation host and timeline for the main frame
  // still live on ScrollingCoordinator. https://crbug.com/680606.
  ScrollingCoordinator* c = GetScrollingCoordinator();
  return c ? c->GetCompositorAnimationTimeline() : nullptr;
}

void LocalFrameView::SetLayoutOverflowSize(const IntSize& size) {
  if (size == layout_overflow_size_)
    return;

  layout_overflow_size_ = size;

  Page* page = GetFrame().GetPage();
  if (!page)
    return;
  page->GetChromeClient().ContentsSizeChanged(frame_.Get(), size);
}

void LocalFrameView::AdjustViewSize() {
  if (suppress_adjust_view_size_)
    return;

  LayoutView* layout_view = GetLayoutView();
  if (!layout_view)
    return;

  DCHECK_EQ(frame_->View(), this);
  SetLayoutOverflowSize(layout_view->DocumentRect().Size());
}

void LocalFrameView::AdjustViewSizeAndLayout() {
  AdjustViewSize();
  if (NeedsLayout()) {
    base::AutoReset<bool> suppress_adjust_view_size(&suppress_adjust_view_size_,
                                                    true);
    UpdateLayout();
  }
}

void LocalFrameView::UpdateAcceleratedCompositingSettings() {
  if (auto* layout_view = GetLayoutView())
    layout_view->Compositor()->UpdateAcceleratedCompositingSettings();
}

void LocalFrameView::UpdateCountersAfterStyleChange() {
  auto* layout_view = GetLayoutView();
  DCHECK(layout_view);
  layout_view->UpdateCounters();
}

void LocalFrameView::CountObjectsNeedingLayout(unsigned& needs_layout_objects,
                                               unsigned& total_objects,
                                               bool& is_subtree) {
  needs_layout_objects = 0;
  total_objects = 0;
  is_subtree = IsSubtreeLayout();
  if (is_subtree) {
    layout_subtree_root_list_.CountObjectsNeedingLayout(needs_layout_objects,
                                                        total_objects);
  } else {
    LayoutSubtreeRootList::CountObjectsNeedingLayoutInRoot(
        GetLayoutView(), needs_layout_objects, total_objects);
  }
}

void LocalFrameView::PerformPreLayoutTasks() {
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::performPreLayoutTasks");
  Lifecycle().AdvanceTo(DocumentLifecycle::kInPreLayout);

  // Don't schedule more layouts, we're in one.
  base::AutoReset<bool> change_scheduling_enabled(&layout_scheduling_enabled_,
                                                  false);

  bool was_resized = WasViewportResized();
  Document* document = frame_->GetDocument();
  if (was_resized)
    document->SetResizedForViewportUnits();

  // Viewport-dependent or device-dependent media queries may cause us to need
  // completely different style information.
  bool main_frame_rotation =
      frame_->IsMainFrame() && frame_->GetSettings() &&
      frame_->GetSettings()->GetMainFrameResizesAreOrientationChanges();
  if ((was_resized &&
       document->GetStyleEngine().MediaQueryAffectedByViewportChange()) ||
      (was_resized && main_frame_rotation &&
       document->GetStyleEngine().MediaQueryAffectedByDeviceChange())) {
    document->MediaQueryAffectingValueChanged();
  } else if (was_resized) {
    document->EvaluateMediaQueryList();
  }

  document->UpdateStyleAndLayoutTree();

  // Update style for all embedded SVG documents underneath this frame, so
  // that intrinsic size computation for any embedded objects has up-to-date
  // information before layout.
  ForAllChildLocalFrameViews([](LocalFrameView& view) {
    Document& document = *view.GetFrame().GetDocument();
    if (document.IsSVGDocument())
      document.UpdateStyleAndLayoutTree();
  });

  Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  if (was_resized)
    document->ClearResizedForViewportUnits();
}

void LocalFrameView::LayoutFromRootObject(LayoutObject& root) {
  LayoutState layout_state(root);
  root.UpdateLayout();
}

void LocalFrameView::PrepareLayoutAnalyzer() {
  bool is_tracing = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("blink.debug.layout"), &is_tracing);
  if (!is_tracing) {
    analyzer_.reset();
    return;
  }
  if (!analyzer_)
    analyzer_ = std::make_unique<LayoutAnalyzer>();
  analyzer_->Reset();
}

std::unique_ptr<TracedValue> LocalFrameView::AnalyzerCounters() {
  if (!analyzer_)
    return std::make_unique<TracedValue>();
  std::unique_ptr<TracedValue> value = analyzer_->ToTracedValue();
  value->SetString("host", GetLayoutView()->GetDocument().location()->host());
  value->SetString(
      "frame",
      String::Format("0x%" PRIxPTR, reinterpret_cast<uintptr_t>(frame_.Get())));
  value->SetInteger("contentsHeightAfterLayout",
                    GetLayoutView()->DocumentRect().Height());
  value->SetInteger("visibleHeight", Height());
  value->SetInteger("approximateBlankCharacterCount",
                    base::saturated_cast<int>(
                        FontFaceSetDocument::ApproximateBlankCharacterCount(
                            *frame_->GetDocument())));
  return value;
}

#define PERFORM_LAYOUT_TRACE_CATEGORIES \
  "blink,benchmark,rail," TRACE_DISABLED_BY_DEFAULT("blink.debug.layout")

void LocalFrameView::PerformLayout(bool in_subtree_layout) {
  DCHECK(in_subtree_layout || layout_subtree_root_list_.IsEmpty());

  int contents_height_before_layout = GetLayoutView()->DocumentRect().Height();
  TRACE_EVENT_BEGIN1(
      PERFORM_LAYOUT_TRACE_CATEGORIES, "LocalFrameView::performLayout",
      "contentsHeightBeforeLayout", contents_height_before_layout);
  PrepareLayoutAnalyzer();

  ScriptForbiddenScope forbid_script;

  if (in_subtree_layout && HasOrthogonalWritingModeRoots()) {
    // If we're going to lay out from each subtree root, rather than once from
    // LayoutView, we need to merge the depth-ordered orthogonal writing mode
    // root list into the depth-ordered list of subtrees scheduled for
    // layout. Otherwise, during layout of one such subtree, we'd risk skipping
    // over a subtree of objects needing layout.
    DCHECK(!layout_subtree_root_list_.IsEmpty());
    ScheduleOrthogonalWritingModeRootsForLayout();
  }

  DCHECK(!IsInPerformLayout());
  Lifecycle().AdvanceTo(DocumentLifecycle::kInPerformLayout);

  // performLayout is the actual guts of layout().
  // FIXME: The 300 other lines in layout() probably belong in other helper
  // functions so that a single human could understand what layout() is actually
  // doing.

  {
    // TODO(szager): Remove this after diagnosing crash.
    DocumentLifecycle::CheckNoTransitionScope check_no_transition(Lifecycle());
    if (in_subtree_layout) {
      if (analyzer_) {
        analyzer_->Increment(LayoutAnalyzer::kPerformLayoutRootLayoutObjects,
                             layout_subtree_root_list_.size());
      }
      for (auto& root : layout_subtree_root_list_.Ordered()) {
        if (!root->NeedsLayout())
          continue;
        LayoutFromRootObject(*root);

        root->PaintingLayer()->UpdateLayerPositionsAfterLayout();

        // We need to ensure that we mark up all layoutObjects up to the
        // LayoutView for paint invalidation. This simplifies our code as we
        // just always do a full tree walk.
        if (LayoutObject* container = root->Container())
          container->SetShouldCheckForPaintInvalidation();
      }
      layout_subtree_root_list_.Clear();
    } else {
      if (HasOrthogonalWritingModeRoots())
        LayoutOrthogonalWritingModeRoots();
      GetLayoutView()->UpdateLayout();
    }
  }

  frame_->GetDocument()->Fetcher()->UpdateAllImageResourcePriorities();

  Lifecycle().AdvanceTo(DocumentLifecycle::kAfterPerformLayout);

  TRACE_EVENT_END1(PERFORM_LAYOUT_TRACE_CATEGORIES,
                   "LocalFrameView::performLayout", "counters",
                   AnalyzerCounters());
  FirstMeaningfulPaintDetector::From(*frame_->GetDocument())
      .MarkNextPaintAsMeaningfulIfNeeded(
          layout_object_counter_, contents_height_before_layout,
          GetLayoutView()->DocumentRect().Height(), Height());
}

void LocalFrameView::UpdateLayout() {
  // We should never layout a Document which is not in a LocalFrame.
  DCHECK(frame_);
  DCHECK_EQ(frame_->View(), this);
  DCHECK(frame_->GetPage());

  ScriptForbiddenScope forbid_script;

  if (IsInPerformLayout() || ShouldThrottleRendering() ||
      !frame_->GetDocument()->IsActive() || frame_->IsProvisional())
    return;

  TRACE_EVENT0("blink,benchmark", "LocalFrameView::layout");

  RUNTIME_CALL_TIMER_SCOPE(V8PerIsolateData::MainThreadIsolate(),
                           RuntimeCallStats::CounterId::kUpdateLayout);

  // The actual call to UpdateGeometries is in PerformPostLayoutTasks.
  SetNeedsUpdateGeometries();

  if (auto_size_info_)
    auto_size_info_->AutoSizeIfNeeded();

  has_pending_layout_ = false;

  Document* document = frame_->GetDocument();
  TRACE_EVENT_BEGIN1("devtools.timeline", "Layout", "beginData",
                     inspector_layout_event::BeginData(this));
  probe::UpdateLayout probe(document);

  PerformPreLayoutTasks();

  VisualViewport& visual_viewport = frame_->GetPage()->GetVisualViewport();
  DoubleSize viewport_size(visual_viewport.VisibleWidthCSSPx(),
                           visual_viewport.VisibleHeightCSSPx());

  // TODO(crbug.com/460956): The notion of a single root for layout is no
  // longer applicable. Remove or update this code.
  LayoutObject* root_for_this_layout = GetLayoutView();

  FontCachePurgePreventer font_cache_purge_preventer;
  StyleRetainScope style_retain_scope;
  bool in_subtree_layout = false;
  {
    base::AutoReset<bool> change_scheduling_enabled(&layout_scheduling_enabled_,
                                                    false);
    nested_layout_count_++;

    // If the layout view was marked as needing layout after we added items in
    // the subtree roots we need to clear the roots and do the layout from the
    // layoutView.
    if (GetLayoutView()->NeedsLayout())
      ClearLayoutSubtreeRootsAndMarkContainingBlocks();
    GetLayoutView()->ClearHitTestCache();

    in_subtree_layout = IsSubtreeLayout();

    // TODO(crbug.com/460956): The notion of a single root for layout is no
    // longer applicable. Remove or update this code.
    if (in_subtree_layout)
      root_for_this_layout = layout_subtree_root_list_.RandomRoot();

    if (!root_for_this_layout) {
      // FIXME: Do we need to set m_size here?
      NOTREACHED();
      return;
    }

    if (!in_subtree_layout) {
      ClearLayoutSubtreeRootsAndMarkContainingBlocks();
      Node* body = document->body();
      if (body && body->GetLayoutObject()) {
        if (IsA<HTMLFrameSetElement>(*body)) {
          body->GetLayoutObject()->SetChildNeedsLayout();
        } else if (IsA<HTMLBodyElement>(*body)) {
          if (!first_layout_ && size_.Height() != GetLayoutSize().Height() &&
              body->GetLayoutObject()->EnclosingBox()->StretchesToViewport())
            body->GetLayoutObject()->SetChildNeedsLayout();
        }
      }

      if (first_layout_) {
        first_layout_ = false;
        last_viewport_size_ = GetLayoutSize();
        last_zoom_factor_ = GetLayoutView()->StyleRef().Zoom();

        ScrollbarMode h_mode;
        ScrollbarMode v_mode;
        GetLayoutView()->CalculateScrollbarModes(h_mode, v_mode);
        if (v_mode == ScrollbarMode::kAuto) {
          GetLayoutView()
              ->GetScrollableArea()
              ->ForceVerticalScrollbarForFirstLayout();
        }
      }

      LayoutSize old_size = size_;

      size_ = LayoutSize(GetLayoutSize());

      if (old_size != size_ && !first_layout_) {
        LayoutBox* root_layout_object =
            document->documentElement()
                ? document->documentElement()->GetLayoutBox()
                : nullptr;
        LayoutBox* body_layout_object = root_layout_object && document->body()
                                            ? document->body()->GetLayoutBox()
                                            : nullptr;
        if (body_layout_object && body_layout_object->StretchesToViewport())
          body_layout_object->SetChildNeedsLayout();
        else if (root_layout_object &&
                 root_layout_object->StretchesToViewport())
          root_layout_object->SetChildNeedsLayout();
      }
    }

    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.trees"), "LayoutTree",
        this, TracedLayoutObject::Create(*GetLayoutView(), false));

    IntSize old_size(Size());

    PerformLayout(in_subtree_layout);

    IntSize new_size(Size());
    if (old_size != new_size) {
      MarkViewportConstrainedObjectsForLayout(
          old_size.Width() != new_size.Width(),
          old_size.Height() != new_size.Height());
    }

    if (frame_->IsMainFrame()) {
      if (auto* text_autosizer = frame_->GetDocument()->GetTextAutosizer()) {
        if (text_autosizer->HasLayoutInlineSizeChanged())
          text_autosizer->UpdatePageInfoInAllFrames(frame_);
      }
    }

    if (NeedsLayout()) {
      base::AutoReset<bool> suppress(&suppress_adjust_view_size_, true);
      UpdateLayout();
    }

    DCHECK(layout_subtree_root_list_.IsEmpty());
  }  // Reset m_layoutSchedulingEnabled to its previous value.
  CheckDoesNotNeedLayout();

  DocumentLifecycle::Scope lifecycle_scope(Lifecycle(),
                                           DocumentLifecycle::kLayoutClean);

  frame_timing_requests_dirty_ = true;

  if (!in_subtree_layout)
    GetLayoutView()->EnclosingLayer()->UpdateLayerPositionsAfterLayout();

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.trees"), "LayoutTree", this,
      TracedLayoutObject::Create(*GetLayoutView(), true));

  GetLayoutView()->Compositor()->DidLayout();
  layout_count_for_testing_++;

  if (AXObjectCache* cache = document->ExistingAXObjectCache()) {
    const KURL& url = document->Url();
    if (url.IsValid() && !url.IsAboutBlankURL()) {
      cache->HandleLayoutComplete(document);
      cache->ProcessUpdatesAfterLayout(*document);
    }
  }
  UpdateDocumentAnnotatedRegions();
  CheckDoesNotNeedLayout();

  if (nested_layout_count_ == 1) {
    PerformPostLayoutTasks();
    CheckDoesNotNeedLayout();
  }

  // FIXME: The notion of a single root for layout is no longer applicable.
  // Remove or update this code. crbug.com/460596
  TRACE_EVENT_END1("devtools.timeline", "Layout", "endData",
                   inspector_layout_event::EndData(root_for_this_layout));
  probe::DidChangeViewport(frame_.Get());

  nested_layout_count_--;
  if (nested_layout_count_)
    return;

#if DCHECK_IS_ON()
  // Post-layout assert that nobody was re-marked as needing layout during
  // layout.
  GetLayoutView()->AssertSubtreeIsLaidOut();
#endif

  if (frame_->IsMainFrame()) {
    // Scrollbars changing state can cause a visual viewport size change.
    DoubleSize new_viewport_size(visual_viewport.VisibleWidthCSSPx(),
                                 visual_viewport.VisibleHeightCSSPx());
    if (new_viewport_size != viewport_size)
      frame_->GetDocument()->EnqueueVisualViewportResizeEvent();
  }

  GetFrame().GetDocument()->LayoutUpdated();
  CheckDoesNotNeedLayout();
}

void LocalFrameView::WillStartForcedLayout() {
  // UpdateLayoutIgnoringPendingStyleSheets is re-entrant for auto-sizing
  // and plugins. So keep track of stack depth.
  forced_layout_stack_depth_++;
  if (forced_layout_stack_depth_ > 1)
    return;
  forced_layout_start_time_ = base::TimeTicks::Now();
}

void LocalFrameView::DidFinishForcedLayout() {
  CHECK_GT(forced_layout_stack_depth_, (unsigned)0);
  forced_layout_stack_depth_--;
  if (!forced_layout_stack_depth_ && base::TimeTicks::IsHighResolution()) {
    LocalFrameUkmAggregator& aggregator = EnsureUkmAggregator();
    aggregator.RecordSample(
        (size_t)LocalFrameUkmAggregator::kForcedStyleAndLayout,
        forced_layout_start_time_, base::TimeTicks::Now());
  }
}

void LocalFrameView::SetNeedsPaintPropertyUpdate() {
  if (auto* layout_view = GetLayoutView())
    layout_view->SetNeedsPaintPropertyUpdate();
}

FloatSize LocalFrameView::ViewportSizeForViewportUnits() const {
  float zoom = 1;
  if (!frame_->GetDocument() || !frame_->GetDocument()->Printing())
    zoom = GetFrame().PageZoomFactor();

  auto* layout_view = GetLayoutView();
  if (!layout_view)
    return FloatSize();

  FloatSize layout_size;
  layout_size.SetWidth(layout_view->ViewWidth(kIncludeScrollbars) / zoom);
  layout_size.SetHeight(layout_view->ViewHeight(kIncludeScrollbars) / zoom);

  BrowserControls& browser_controls = frame_->GetPage()->GetBrowserControls();
  if (browser_controls.PermittedState() != cc::BrowserControlsState::kHidden) {
    // We use the layoutSize rather than frameRect to calculate viewport units
    // so that we get correct results on mobile where the page is laid out into
    // a rect that may be larger than the viewport (e.g. the 980px fallback
    // width for desktop pages). Since the layout height is statically set to
    // be the viewport with browser controls showing, we add the browser
    // controls height, compensating for page scale as well, since we want to
    // use the viewport with browser controls hidden for vh (to match Safari).
    int viewport_width = frame_->GetPage()->GetVisualViewport().Size().Width();
    if (frame_->IsMainFrame() && layout_size.Width() && viewport_width) {
      // TODO(bokan/eirage): BrowserControl height may need to account for the
      // zoom factor when use-zoom-for-dsf is enabled on Android. Confirm this
      // works correctly when that's turned on. https://crbug.com/737777.
      float page_scale_at_layout_width = viewport_width / layout_size.Width();
      layout_size.Expand(
          0, browser_controls.TotalHeight() / page_scale_at_layout_width);
    }
  }

  return layout_size;
}

FloatSize LocalFrameView::ViewportSizeForMediaQueries() const {
  FloatSize viewport_size(layout_size_);
  if (!frame_->GetDocument()->Printing()) {
    float zoom = GetFrame().PageZoomFactor();
    viewport_size.SetWidth(
        AdjustForAbsoluteZoom::AdjustInt(layout_size_.Width(), zoom));
    viewport_size.SetHeight(
        AdjustForAbsoluteZoom::AdjustInt(layout_size_.Height(), zoom));
  }
  return viewport_size;
}

DocumentLifecycle& LocalFrameView::Lifecycle() const {
  DCHECK(frame_);
  DCHECK(frame_->GetDocument());
  return frame_->GetDocument()->Lifecycle();
}

void LocalFrameView::RunPostLifecycleSteps() {
  RunIntersectionObserverSteps();
}

void LocalFrameView::RunIntersectionObserverSteps() {
#if DCHECK_IS_ON()
  bool was_dirty = NeedsLayout();
#endif
  if (ShouldThrottleRendering() || Lifecycle().LifecyclePostponed() ||
      !frame_->GetDocument()->IsActive()) {
    return;
  }
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::UpdateViewportIntersectionsForSubtree");
  SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                           LocalFrameUkmAggregator::kIntersectionObservation);
  bool needs_occlusion_tracking = UpdateViewportIntersectionsForSubtree(0);
  if (FrameOwner* owner = frame_->Owner())
    owner->SetNeedsOcclusionTracking(needs_occlusion_tracking);
#if DCHECK_IS_ON()
  DCHECK(was_dirty || !NeedsLayout());
#endif
  DeliverSynchronousIntersectionObservations();
}

void LocalFrameView::ForceUpdateViewportIntersections() {
  // IntersectionObserver targets in this frame (and its frame tree) need to
  // update; but we can't wait for a lifecycle update to run them, because a
  // hidden frame won't run lifecycle updates. Force layout and run them now.
  DocumentLifecycle::DisallowThrottlingScope disallow_throttling(Lifecycle());
  UpdateLifecycleToCompositingCleanPlusScrolling();
  UpdateViewportIntersectionsForSubtree(
      IntersectionObservation::kImplicitRootObserversNeedUpdate |
      IntersectionObservation::kIgnoreDelay);
}

LayoutSVGRoot* LocalFrameView::EmbeddedReplacedContent() const {
  auto* layout_view = this->GetLayoutView();
  if (!layout_view)
    return nullptr;

  LayoutObject* first_child = layout_view->FirstChild();
  if (!first_child || !first_child->IsBox())
    return nullptr;

  // Currently only embedded SVG documents participate in the size-negotiation
  // logic.
  return ToLayoutSVGRootOrNull(first_child);
}

bool LocalFrameView::GetIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  if (LayoutSVGRoot* content_layout_object = EmbeddedReplacedContent()) {
    content_layout_object->UnscaledIntrinsicSizingInfo(intrinsic_sizing_info);
    return true;
  }
  return false;
}

bool LocalFrameView::HasIntrinsicSizingInfo() const {
  return EmbeddedReplacedContent();
}

void LocalFrameView::UpdateGeometry() {
  LayoutEmbeddedContent* layout = GetLayoutEmbeddedContent();
  if (!layout)
    return;

  bool did_need_layout = NeedsLayout();

  PhysicalRect new_frame = layout->ReplacedContentRect();
#if DCHECK_IS_ON()
  if (new_frame.Width() != LayoutUnit::Max().RawValue() &&
      new_frame.Height() != LayoutUnit::Max().RawValue())
    DCHECK(!new_frame.size.HasFraction());
#endif
  bool bounds_will_change = PhysicalSize(Size()) != new_frame.size;

  // If frame bounds are changing mark the view for layout. Also check the
  // frame's page to make sure that the frame isn't in the process of being
  // destroyed. If iframe scrollbars needs reconstruction from native to custom
  // scrollbar, then also we need to layout the frameview.
  if (bounds_will_change)
    SetNeedsLayout();

  layout->UpdateGeometry(*this);
  // If view needs layout, either because bounds have changed or possibly
  // indicating content size is wrong, we have to do a layout to set the right
  // LocalFrameView size.
  if (NeedsLayout())
    UpdateLayout();

  if (!did_need_layout && !ShouldThrottleRendering())
    CheckDoesNotNeedLayout();
}

void LocalFrameView::AddPartToUpdate(LayoutEmbeddedObject& object) {
  DCHECK(IsInPerformLayout());
  // Tell the DOM element that it needs a Plugin update.
  Node* node = object.GetNode();
  DCHECK(node);
  if (IsHTMLObjectElement(*node) || IsHTMLEmbedElement(*node))
    ToHTMLPlugInElement(node)->SetNeedsPluginUpdate(true);

  part_update_set_.insert(&object);
}

void LocalFrameView::SetDisplayMode(WebDisplayMode mode) {
  if (mode == display_mode_)
    return;

  display_mode_ = mode;

  if (frame_->GetDocument())
    frame_->GetDocument()->MediaQueryAffectingValueChanged();
}

void LocalFrameView::SetDisplayShape(DisplayShape display_shape) {
  if (display_shape == display_shape_)
    return;

  display_shape_ = display_shape;

  if (frame_->GetDocument())
    frame_->GetDocument()->MediaQueryAffectingValueChanged();
}

void LocalFrameView::SetMediaType(const AtomicString& media_type) {
  DCHECK(frame_->GetDocument());
  media_type_ = media_type;
  frame_->GetDocument()->MediaQueryAffectingValueChanged();
}

AtomicString LocalFrameView::MediaType() const {
  // See if we have an override type.
  if (frame_->GetSettings() &&
      !frame_->GetSettings()->GetMediaTypeOverride().IsEmpty())
    return AtomicString(frame_->GetSettings()->GetMediaTypeOverride());
  return media_type_;
}

void LocalFrameView::AdjustMediaTypeForPrinting(bool printing) {
  if (printing) {
    if (media_type_when_not_printing_.IsNull())
      media_type_when_not_printing_ = MediaType();
    SetMediaType(media_type_names::kPrint);
  } else {
    if (!media_type_when_not_printing_.IsNull())
      SetMediaType(media_type_when_not_printing_);
    media_type_when_not_printing_ = g_null_atom;
  }

  frame_->GetDocument()->GetStyleEngine().MarkAllElementsForStyleRecalc(
      StyleChangeReasonForTracing::Create(
          style_change_reason::kStyleSheetChange));
}

void LocalFrameView::AddBackgroundAttachmentFixedObject(LayoutObject* object) {
  DCHECK(!background_attachment_fixed_objects_.Contains(object));

  background_attachment_fixed_objects_.insert(object);
  if (ScrollingCoordinator* scrolling_coordinator =
          this->GetScrollingCoordinator()) {
    scrolling_coordinator
        ->FrameViewHasBackgroundAttachmentFixedObjectsDidChange(this);
  }

  // Ensure main thread scrolling reasons are recomputed.
  SetNeedsPaintPropertyUpdate();
  // The object's scroll properties are not affected by its own background.
  object->SetAncestorsNeedPaintPropertyUpdateForMainThreadScrolling();
}

void LocalFrameView::RemoveBackgroundAttachmentFixedObject(
    LayoutObject* object) {
  DCHECK(background_attachment_fixed_objects_.Contains(object));

  background_attachment_fixed_objects_.erase(object);
  if (ScrollingCoordinator* scrolling_coordinator =
          this->GetScrollingCoordinator()) {
    scrolling_coordinator
        ->FrameViewHasBackgroundAttachmentFixedObjectsDidChange(this);
  }

  // Ensure main thread scrolling reasons are recomputed.
  SetNeedsPaintPropertyUpdate();
  // The object's scroll properties are not affected by its own background.
  object->SetAncestorsNeedPaintPropertyUpdateForMainThreadScrolling();
}

void LocalFrameView::AddViewportConstrainedObject(LayoutObject& object) {
  if (!viewport_constrained_objects_)
    viewport_constrained_objects_ = std::make_unique<ObjectSet>();

  if (!viewport_constrained_objects_->Contains(&object)) {
    viewport_constrained_objects_->insert(&object);

    if (ScrollingCoordinator* scrolling_coordinator =
            this->GetScrollingCoordinator())
      scrolling_coordinator->FrameViewFixedObjectsDidChange(this);
  }
}

void LocalFrameView::RemoveViewportConstrainedObject(LayoutObject& object) {
  if (viewport_constrained_objects_ &&
      viewport_constrained_objects_->Contains(&object)) {
    viewport_constrained_objects_->erase(&object);

    if (ScrollingCoordinator* scrolling_coordinator =
            this->GetScrollingCoordinator())
      scrolling_coordinator->FrameViewFixedObjectsDidChange(this);
  }
}

void LocalFrameView::ViewportSizeChanged(bool width_changed,
                                         bool height_changed) {
  DCHECK(width_changed || height_changed);
  DCHECK(frame_->GetPage());
  if (frame_->GetDocument() &&
      frame_->GetDocument()->Lifecycle().LifecyclePostponed())
    return;

  if (frame_->IsMainFrame())
    layout_shift_tracker_->NotifyViewportSizeChanged();

  auto* layout_view = GetLayoutView();
  if (layout_view) {
    // If this is the main frame, we might have got here by hiding/showing the
    // top controls.  In that case, layout won't be triggered, so we need to
    // clamp the scroll offset here.
    if (GetFrame().IsMainFrame()) {
      layout_view->Layer()->UpdateSize();
      layout_view->GetScrollableArea()->ClampScrollOffsetAfterOverflowChange();
    }

    if (layout_view->UsesCompositing()) {
      layout_view->Layer()->SetNeedsCompositingInputsUpdate();
      SetNeedsPaintPropertyUpdate();
    }
  }

  if (GetFrame().GetDocument())
    GetFrame().GetDocument()->GetRootScrollerController().DidResizeFrameView();

  // Change of viewport size after browser controls showing/hiding may affect
  // painting of the background.
  if (layout_view && frame_->IsMainFrame() &&
      frame_->GetPage()->GetBrowserControls().TotalHeight())
    layout_view->SetShouldCheckForPaintInvalidation();

  if (GetFrame().GetDocument() && !IsInPerformLayout())
    MarkViewportConstrainedObjectsForLayout(width_changed, height_changed);

  if (GetPaintTimingDetector().Visualizer())
    GetPaintTimingDetector().Visualizer()->OnViewportChanged();
}

void LocalFrameView::MarkViewportConstrainedObjectsForLayout(
    bool width_changed,
    bool height_changed) {
  if (!HasViewportConstrainedObjects() || !(width_changed || height_changed))
    return;

  for (auto* const viewport_constrained_object :
       *viewport_constrained_objects_) {
    LayoutObject* layout_object = viewport_constrained_object;
    const ComputedStyle& style = layout_object->StyleRef();
    if (width_changed) {
      if (style.Width().IsFixed() &&
          (style.Left().IsAuto() || style.Right().IsAuto())) {
        layout_object->SetNeedsPositionedMovementLayout();
      } else {
        layout_object->SetNeedsLayoutAndFullPaintInvalidation(
            layout_invalidation_reason::kSizeChanged);
      }
    }
    if (height_changed) {
      if (style.Height().IsFixed() &&
          (style.Top().IsAuto() || style.Bottom().IsAuto())) {
        layout_object->SetNeedsPositionedMovementLayout();
      } else {
        layout_object->SetNeedsLayoutAndFullPaintInvalidation(
            layout_invalidation_reason::kSizeChanged);
      }
    }
  }
}

bool LocalFrameView::ShouldSetCursor() const {
  Page* page = GetFrame().GetPage();
  return page && page->IsPageVisible() &&
         !frame_->GetEventHandler().IsMousePositionUnknown() &&
         page->GetFocusController().IsActive();
}

void LocalFrameView::InvalidateBackgroundAttachmentFixedDescendantsOnScroll(
    const LayoutObject& scrolled_object) {
  for (auto* const layout_object : background_attachment_fixed_objects_) {
    if (scrolled_object != GetLayoutView() &&
        !layout_object->IsDescendantOf(&scrolled_object))
      continue;
    // An object needs to repaint the background on scroll when it has
    // background-attachment:fixed unless the object is the LayoutView and the
    // background is not painted on the scrolling contents.
    if (layout_object == GetLayoutView() &&
        !(GetLayoutView()->GetBackgroundPaintLocation() &
          kBackgroundPaintInScrollingContents))
      continue;
    layout_object->SetBackgroundNeedsFullPaintInvalidation();
  }
}

bool LocalFrameView::InvalidateViewportConstrainedObjects() {
  bool fast_path_allowed = true;
  for (auto* const viewport_constrained_object :
       *viewport_constrained_objects_) {
    LayoutObject* layout_object = viewport_constrained_object;
    DCHECK(layout_object->StyleRef().HasViewportConstrainedPosition() ||
           layout_object->StyleRef().HasStickyConstrainedPosition());
    DCHECK(layout_object->HasLayer());
    PaintLayer* layer = ToLayoutBoxModelObject(layout_object)->Layer();

    if (layer->IsPaintInvalidationContainer())
      continue;

    // If the layer has no visible content, then we shouldn't invalidate; but
    // if we're not compositing-inputs-clean, then we can't query
    // layer->SubtreeIsInvisible() here.
    layout_object->SetSubtreeShouldCheckForPaintInvalidation();
    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
        !layer->NeedsRepaint()) {
      // Paint properties of the layer relative to its containing graphics
      // layer may change if the paint properties escape the graphics layer's
      // property state. Need to check raster invalidation for relative paint
      // property changes.
      if (auto* paint_invalidation_layer =
              layer->EnclosingLayerForPaintInvalidation()) {
        auto* mapping = paint_invalidation_layer->GetCompositedLayerMapping();
        if (!mapping)
          mapping = paint_invalidation_layer->GroupedMapping();
        if (mapping)
          mapping->SetNeedsCheckRasterInvalidation();
      }
    }

    // If the fixed layer has a blur/drop-shadow filter applied on at least one
    // of its parents, we cannot scroll using the fast path, otherwise the
    // outsets of the filter will be moved around the page.
    if (layer->HasAncestorWithFilterThatMovesPixels())
      fast_path_allowed = false;
  }
  return fast_path_allowed;
}

void LocalFrameView::ProcessUrlFragment(const KURL& url,
                                        bool same_document_navigation,
                                        bool should_scroll) {
  // We want to create the anchor even if we don't need to scroll. This ensures
  // all the side effects like setting CSS :target are correctly set.
  FragmentAnchor* anchor =
      FragmentAnchor::TryCreate(url, *frame_, same_document_navigation);

  if (anchor && should_scroll) {
    fragment_anchor_ = anchor;
    fragment_anchor_->Installed();

    // Layout needs to be clean for scrolling but if layout is needed, we'll
    // invoke after layout is completed so no need to do it here.
    if (!NeedsLayout())
      InvokeFragmentAnchor();
  }
}

void LocalFrameView::SetLayoutSize(const IntSize& size) {
  DCHECK(!LayoutSizeFixedToFrameSize());
  if (frame_->GetDocument() &&
      frame_->GetDocument()->Lifecycle().LifecyclePostponed())
    return;

  SetLayoutSizeInternal(size);
}

void LocalFrameView::SetLayoutSizeFixedToFrameSize(bool is_fixed) {
  if (layout_size_fixed_to_frame_size_ == is_fixed)
    return;

  layout_size_fixed_to_frame_size_ = is_fixed;
  if (is_fixed)
    SetLayoutSizeInternal(Size());
}

static cc::LayerSelection ComputeLayerSelection(LocalFrame& frame) {
  if (!frame.View() || frame.View()->ShouldThrottleRendering())
    return {};

  return ComputeLayerSelection(frame.Selection());
}

void LocalFrameView::UpdateCompositedSelectionIfNeeded() {
  if (!RuntimeEnabledFeatures::CompositedSelectionUpdateEnabled())
    return;

  TRACE_EVENT0("blink", "LocalFrameView::updateCompositedSelectionIfNeeded");

  Page* page = GetFrame().GetPage();
  DCHECK(page);

  LocalFrame* focused_frame = page->GetFocusController().FocusedFrame();
  LocalFrame* local_frame =
      (focused_frame &&
       (focused_frame->LocalFrameRoot() == frame_->LocalFrameRoot()))
          ? focused_frame
          : nullptr;

  if (local_frame) {
    const cc::LayerSelection& selection = ComputeLayerSelection(*local_frame);
    if (selection != cc::LayerSelection()) {
      page->GetChromeClient().UpdateLayerSelection(local_frame, selection);
      return;
    }
  }

  if (!local_frame) {
    // Clearing the mainframe when there is no focused frame (and hence
    // no localFrame) is legacy behaviour, and implemented here to
    // satisfy WebFrameTest.CompositedSelectionBoundsCleared's
    // first check that the composited selection has been cleared even
    // though no frame has focus yet. If this is not desired, then the
    // expectation needs to be removed from the test.
    local_frame = &frame_->LocalFrameRoot();
  }
  DCHECK(local_frame);
  page->GetChromeClient().ClearLayerSelection(local_frame);
}

void LocalFrameView::SetNeedsCompositingUpdate(
    CompositingUpdateType update_type) {
  if (auto* layout_view = GetLayoutView()) {
    if (frame_->GetDocument()->IsActive())
      layout_view->Compositor()->SetNeedsCompositingUpdate(update_type);
  }
}

ChromeClient* LocalFrameView::GetChromeClient() const {
  Page* page = GetFrame().GetPage();
  if (!page)
    return nullptr;
  return &page->GetChromeClient();
}

void LocalFrameView::HandleLoadCompleted() {
  // Once loading has completed, allow autoSize one last opportunity to
  // reduce the size of the frame.
  if (auto_size_info_)
    auto_size_info_->AutoSizeIfNeeded();

  if (fragment_anchor_)
    fragment_anchor_->DidCompleteLoad();
}

void LocalFrameView::ClearLayoutSubtreeRoot(const LayoutObject& root) {
  layout_subtree_root_list_.Remove(const_cast<LayoutObject&>(root));
}

void LocalFrameView::ClearLayoutSubtreeRootsAndMarkContainingBlocks() {
  layout_subtree_root_list_.ClearAndMarkContainingBlocksForLayout();
}

void LocalFrameView::AddOrthogonalWritingModeRoot(LayoutBox& root) {
  DCHECK(!root.IsLayoutScrollbarPart());
  orthogonal_writing_mode_root_list_.Add(root);
}

void LocalFrameView::RemoveOrthogonalWritingModeRoot(LayoutBox& root) {
  orthogonal_writing_mode_root_list_.Remove(root);
}

bool LocalFrameView::HasOrthogonalWritingModeRoots() const {
  return !orthogonal_writing_mode_root_list_.IsEmpty();
}

static inline void RemoveFloatingObjectsForSubtreeRoot(LayoutObject& root) {
  // TODO(kojii): Under certain conditions, moveChildTo() defers
  // removeFloatingObjects() until the containing block layouts. For
  // instance, when descendants of the moving child is floating,
  // removeChildNode() does not clear them. In such cases, at this
  // point, FloatingObjects may contain old or even deleted objects.
  // Dealing this in markAllDescendantsWithFloatsForLayout() could
  // solve, but since that is likely to suffer the performance and
  // since the containing block of orthogonal writing mode roots
  // having floats is very rare, prefer to re-create
  // FloatingObjects.
  if (LayoutBlock* cb = root.ContainingBlock()) {
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(cb);
    if ((cb->NormalChildNeedsLayout() || cb->SelfNeedsLayout()) &&
        child_block_flow) {
      child_block_flow->RemoveFloatingObjectsFromDescendants();
    }
  }
}

static bool PrepareOrthogonalWritingModeRootForLayout(LayoutObject& root) {
  DCHECK(root.IsBox() && ToLayoutBox(root).IsOrthogonalWritingModeRoot());
  if (!root.NeedsLayout() || root.IsOutOfFlowPositioned() ||
      root.IsColumnSpanAll() ||
      !root.StyleRef().LogicalHeight().IsIntrinsicOrAuto() ||
      ToLayoutBox(root).IsGridItem() || root.IsTablePart())
    return false;

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    // Do not pre-layout objects that are fully managed by LayoutNG; it is not
    // necessary and may lead to double layouts. We do need to pre-layout
    // objects whose containing block is a legacy object so that it can
    // properly compute its intrinsic size.
    if (IsManagedByLayoutNG(root))
      return false;

    // If the root is legacy but has |CachedLayoutResult|, its parent is NG,
    // which called |RunLegacyLayout()|. This parent not only needs to run
    // pre-layout, but also clearing |NeedsLayout()| without updating
    // |CachedLayoutResult| is harmful.
    if (const auto* box = ToLayoutBoxOrNull(&root)) {
      if (box->GetCachedLayoutResult())
        return false;
    }
  }

  RemoveFloatingObjectsForSubtreeRoot(root);
  return true;
}

void LocalFrameView::LayoutOrthogonalWritingModeRoots() {
  for (auto& root : orthogonal_writing_mode_root_list_.Ordered()) {
    if (PrepareOrthogonalWritingModeRootForLayout(*root))
      LayoutFromRootObject(*root);
  }
}

void LocalFrameView::ScheduleOrthogonalWritingModeRootsForLayout() {
  for (auto& root : orthogonal_writing_mode_root_list_.Ordered()) {
    if (PrepareOrthogonalWritingModeRootForLayout(*root))
      layout_subtree_root_list_.Add(*root);
  }
}

bool LocalFrameView::CheckLayoutInvalidationIsAllowed() const {
#if DCHECK_IS_ON()
  if (allows_layout_invalidation_after_layout_clean_)
    return true;

  // If we are updating all lifecycle phases beyond LayoutClean, we don't expect
  // dirty layout after LayoutClean.
  CHECK_FOR_DIRTY_LAYOUT(Lifecycle().GetState() <
                         DocumentLifecycle::kLayoutClean);

#endif
  return true;
}

void LocalFrameView::ScheduleRelayout() {
  DCHECK(frame_->View() == this);

  if (!layout_scheduling_enabled_)
    return;
  // TODO(crbug.com/590856): It's still broken when we choose not to crash when
  // the check fails.
  if (!CheckLayoutInvalidationIsAllowed())
    return;
  if (!NeedsLayout())
    return;
  if (!frame_->GetDocument()->ShouldScheduleLayout())
    return;
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "InvalidateLayout", TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_invalidate_layout_event::Data(frame_.Get()));

  ClearLayoutSubtreeRootsAndMarkContainingBlocks();

  if (has_pending_layout_)
    return;
  has_pending_layout_ = true;

  if (!ShouldThrottleRendering())
    GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());
}

void LocalFrameView::ScheduleRelayoutOfSubtree(LayoutObject* relayout_root) {
  DCHECK(frame_->View() == this);

  // TODO(crbug.com/590856): It's still broken when we choose not to crash when
  // the check fails.
  if (!CheckLayoutInvalidationIsAllowed())
    return;

  // FIXME: Should this call shouldScheduleLayout instead?
  if (!frame_->GetDocument()->IsActive())
    return;

  LayoutView* layout_view = this->GetLayoutView();
  if (layout_view && layout_view->NeedsLayout()) {
    if (relayout_root)
      relayout_root->MarkContainerChainForLayout(false);
    return;
  }

  if (relayout_root == layout_view)
    layout_subtree_root_list_.ClearAndMarkContainingBlocksForLayout();
  else
    layout_subtree_root_list_.Add(*relayout_root);

  if (layout_scheduling_enabled_) {
    has_pending_layout_ = true;

    if (!ShouldThrottleRendering())
      GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());

    Lifecycle().EnsureStateAtMost(DocumentLifecycle::kStyleClean);
  }
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "InvalidateLayout", TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_invalidate_layout_event::Data(frame_.Get()));
}

bool LocalFrameView::LayoutPending() const {
  // FIXME: This should check Document::lifecycle instead.
  return has_pending_layout_;
}

bool LocalFrameView::IsInPerformLayout() const {
  return Lifecycle().GetState() == DocumentLifecycle::kInPerformLayout;
}

bool LocalFrameView::NeedsLayout() const {
  // This can return true in cases where the document does not have a body yet.
  // Document::shouldScheduleLayout takes care of preventing us from scheduling
  // layout in that case.

  auto* layout_view = GetLayoutView();
  return LayoutPending() || (layout_view && layout_view->NeedsLayout()) ||
         IsSubtreeLayout();
}

NOINLINE bool LocalFrameView::CheckDoesNotNeedLayout() const {
  CHECK_FOR_DIRTY_LAYOUT(!LayoutPending());
  CHECK_FOR_DIRTY_LAYOUT(!GetLayoutView() || !GetLayoutView()->NeedsLayout());
  CHECK_FOR_DIRTY_LAYOUT(!IsSubtreeLayout());
  return true;
}

void LocalFrameView::SetNeedsLayout() {
  auto* layout_view = GetLayoutView();
  if (!layout_view)
    return;
  // TODO(crbug.com/590856): It's still broken if we choose not to crash when
  // the check fails.
  if (!CheckLayoutInvalidationIsAllowed())
    return;
  layout_view->SetNeedsLayout(layout_invalidation_reason::kUnknown);
}

bool LocalFrameView::HasOpaqueBackground() const {
  return !base_background_color_.HasAlpha();
}

Color LocalFrameView::BaseBackgroundColor() const {
  if (use_dark_scheme_background_ &&
      base_background_color_ != Color::kTransparent) {
    return Color::kBlack;
  }
  return base_background_color_;
}

void LocalFrameView::SetBaseBackgroundColor(const Color& background_color) {
  if (base_background_color_ == background_color)
    return;

  base_background_color_ = background_color;
  if (auto* layout_view = GetLayoutView()) {
    if (layout_view->Layer()->HasCompositedLayerMapping()) {
      CompositedLayerMapping* composited_layer_mapping =
          layout_view->Layer()->GetCompositedLayerMapping();
      composited_layer_mapping->UpdateContentsOpaque();
      if (composited_layer_mapping->MainGraphicsLayer())
        composited_layer_mapping->MainGraphicsLayer()->SetNeedsDisplay();
      if (composited_layer_mapping->ScrollingContentsLayer())
        composited_layer_mapping->ScrollingContentsLayer()->SetNeedsDisplay();
    }
  }

  if (!ShouldThrottleRendering())
    GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());
}

void LocalFrameView::SetUseDarkSchemeBackground(bool dark_scheme) {
  if (use_dark_scheme_background_ == dark_scheme)
    return;

  use_dark_scheme_background_ = dark_scheme;
  if (auto* layout_view = GetLayoutView())
    layout_view->SetBackgroundNeedsFullPaintInvalidation();
}

void LocalFrameView::UpdateBaseBackgroundColorRecursively(
    const Color& base_background_color) {
  ForAllNonThrottledLocalFrameViews(
      [base_background_color](LocalFrameView& frame_view) {
        frame_view.SetBaseBackgroundColor(base_background_color);
      });
}

void LocalFrameView::InvokeFragmentAnchor() {
  if (!fragment_anchor_)
    return;

  if (!fragment_anchor_->Invoke())
    fragment_anchor_ = nullptr;
}

void LocalFrameView::DismissFragmentAnchor() {
  if (!fragment_anchor_)
    return;

  if (fragment_anchor_->Dismiss())
    fragment_anchor_ = nullptr;
}

bool LocalFrameView::UpdatePlugins() {
  // This is always called from UpdatePluginsTimerFired.
  // update_plugins_timer should only be scheduled if we have FrameViews to
  // update. Thus I believe we can stop checking isEmpty here, and just ASSERT
  // isEmpty:
  // FIXME: This assert has been temporarily removed due to
  // https://crbug.com/430344
  if (nested_layout_count_ > 1 || part_update_set_.IsEmpty())
    return true;

  // Need to swap because script will run inside the below loop and invalidate
  // the iterator.
  EmbeddedObjectSet objects;
  objects.swap(part_update_set_);

  for (const auto& embedded_object : objects) {
    LayoutEmbeddedObject& object = *embedded_object;
    HTMLPlugInElement* element = ToHTMLPlugInElement(object.GetNode());

    // The object may have already been destroyed (thus node cleared),
    // but LocalFrameView holds a manual ref, so it won't have been deleted.
    if (!element)
      continue;

    // No need to update if it's already crashed or known to be missing.
    if (object.ShowsUnavailablePluginIndicator())
      continue;

    if (element->NeedsPluginUpdate())
      element->UpdatePlugin();
    if (EmbeddedContentView* view = element->OwnedEmbeddedContentView())
      view->UpdateGeometry();

    // Prevent plugins from causing infinite updates of themselves.
    // FIXME: Do we really need to prevent this?
    part_update_set_.erase(&object);
  }

  return part_update_set_.IsEmpty();
}

void LocalFrameView::UpdatePluginsTimerFired(TimerBase*) {
  DCHECK(!IsInPerformLayout());
  for (unsigned i = 0; i < kMaxUpdatePluginsIterations; ++i) {
    if (UpdatePlugins())
      return;
  }
}

void LocalFrameView::FlushAnyPendingPostLayoutTasks() {
  DCHECK(!IsInPerformLayout());
  if (update_plugins_timer_.IsActive()) {
    update_plugins_timer_.Stop();
    UpdatePluginsTimerFired(nullptr);
  }
}

void LocalFrameView::ScheduleUpdatePluginsIfNecessary() {
  DCHECK(!IsInPerformLayout());
  if (update_plugins_timer_.IsActive() || part_update_set_.IsEmpty())
    return;
  update_plugins_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void LocalFrameView::PerformPostLayoutTasks() {
  // FIXME: We can reach here, even when the page is not active!
  // http/tests/inspector/elements/html-link-import.html and many other
  // tests hit that case.
  // We should DCHECK(isActive()); or at least return early if we can!

  // Always called before or after performLayout(), part of the highest-level
  // layout() call.
  DCHECK(!IsInPerformLayout());
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::performPostLayoutTasks");

  frame_->Selection().DidLayout();

  DCHECK(frame_->GetDocument());

  FontFaceSetDocument::DidLayout(*frame_->GetDocument());
  // Fire a fake a mouse move event to update hover state and mouse cursor, and
  // send the right mouse out/over events.
  // TODO(lanwei): we should check whether the mouse is inside the frame before
  // dirtying the hover state.
  if (RuntimeEnabledFeatures::UpdateHoverAtBeginFrameEnabled()) {
    frame_->LocalFrameRoot().GetEventHandler().MarkHoverStateDirty();
  } else {
    frame_->GetEventHandler().MayUpdateHoverWhenContentUnderMouseChanged(
        MouseEventManager::UpdateHoverReason::kLayoutOrStyleChanged);
  }

  UpdateGeometriesIfNeeded();

  // Plugins could have torn down the page inside updateGeometries().
  if (!GetLayoutView())
    return;

  ScheduleUpdatePluginsIfNecessary();

  if (ScrollingCoordinator* scrolling_coordinator =
          this->GetScrollingCoordinator()) {
    scrolling_coordinator->NotifyGeometryChanged(this);
  }

  if (SnapCoordinator* snap_coordinator =
          frame_->GetDocument()->GetSnapCoordinator())
    snap_coordinator->UpdateAllSnapContainerData();

  SendResizeEventIfNeeded();
}

bool LocalFrameView::WasViewportResized() {
  DCHECK(frame_);
  auto* layout_view = GetLayoutView();
  if (!layout_view)
    return false;
  return (GetLayoutSize() != last_viewport_size_ ||
          layout_view->StyleRef().Zoom() != last_zoom_factor_);
}

void LocalFrameView::SendResizeEventIfNeeded() {
  DCHECK(frame_);

  auto* layout_view = GetLayoutView();
  if (!layout_view || layout_view->GetDocument().Printing())
    return;

  if (!WasViewportResized())
    return;

  last_viewport_size_ = GetLayoutSize();
  last_zoom_factor_ = layout_view->StyleRef().Zoom();

  frame_->GetDocument()->EnqueueVisualViewportResizeEvent();

  frame_->GetDocument()->EnqueueResizeEvent();

  if (frame_->IsMainFrame())
    probe::DidResizeMainFrame(frame_.Get());
}

float LocalFrameView::InputEventsScaleFactor() const {
  float page_scale = frame_->GetPage()->GetVisualViewport().Scale();
  return page_scale *
         frame_->GetPage()->GetChromeClient().InputEventsScaleForEmulation();
}

void LocalFrameView::NotifyPageThatContentAreaWillPaint() const {
  Page* page = frame_->GetPage();
  if (!page)
    return;

  if (!scrollable_areas_)
    return;

  for (const auto& scrollable_area : *scrollable_areas_) {
    if (!scrollable_area->ScrollbarsCanBeActive())
      continue;

    scrollable_area->ContentAreaWillPaint();
  }
}

void LocalFrameView::UpdateDocumentAnnotatedRegions() const {
  Document* document = frame_->GetDocument();
  if (!document->HasAnnotatedRegions())
    return;
  Vector<AnnotatedRegionValue> new_regions;
  CollectAnnotatedRegions(*(document->GetLayoutBox()), new_regions);
  if (new_regions == document->AnnotatedRegions())
    return;
  document->SetAnnotatedRegions(new_regions);

  DCHECK(frame_->Client());
  frame_->Client()->AnnotatedRegionsChanged();
}

void LocalFrameView::DidAttachDocument() {
  Page* page = frame_->GetPage();
  DCHECK(page);

  DCHECK(frame_->GetDocument());

  if (frame_->IsMainFrame()) {
    ScrollableArea& visual_viewport = frame_->GetPage()->GetVisualViewport();
    ScrollableArea* layout_viewport = LayoutViewport();
    DCHECK(layout_viewport);

    auto* root_frame_viewport = MakeGarbageCollected<RootFrameViewport>(
        visual_viewport, *layout_viewport);
    viewport_scrollable_area_ = root_frame_viewport;

    page->GlobalRootScrollerController().InitializeViewportScrollCallback(
        *root_frame_viewport, *frame_->GetDocument());

    // Allow for commits to be deferred because this is a new document.
    have_deferred_commits_ = false;
  }
}

Color LocalFrameView::DocumentBackgroundColor() const {
  // The LayoutView's background color is set in
  // Document::inheritHtmlAndBodyElementStyles.  Blend this with the base
  // background color of the LocalFrameView. This should match the color drawn
  // by ViewPainter::paintBoxDecorationBackground.
  Color result = BaseBackgroundColor();

  // If we have a fullscreen element grab the fullscreen color from the
  // backdrop.
  if (Document* doc = frame_->GetDocument()) {
    if (Element* element = Fullscreen::FullscreenElementFrom(*doc)) {
      if (LayoutObject* layout_object =
              element->PseudoElementLayoutObject(kPseudoIdBackdrop)) {
        return result.Blend(
            layout_object->ResolveColor(GetCSSPropertyBackgroundColor()));
      }
    }
  }
  auto* layout_view = GetLayoutView();
  if (layout_view) {
    result = result.Blend(
        layout_view->ResolveColor(GetCSSPropertyBackgroundColor()));
  }
  return result;
}

void LocalFrameView::WillBeRemovedFromFrame() {
  if (paint_artifact_compositor_)
    paint_artifact_compositor_->WillBeRemovedFromFrame();

  if (Settings* settings = frame_->GetSettings()) {
    DCHECK(frame_->GetPage());
    if (settings->GetSpatialNavigationEnabled()) {
      frame_->GetPage()->GetSpatialNavigationController().DidDetachFrameView(
          *this);
    }
  }
}

LocalFrameView* LocalFrameView::ParentFrameView() const {
  if (!IsAttached())
    return nullptr;

  Frame* parent_frame = frame_->Tree().Parent();
  if (auto* parent_local_frame = DynamicTo<LocalFrame>(parent_frame))
    return parent_local_frame->View();

  return nullptr;
}

LayoutEmbeddedContent* LocalFrameView::GetLayoutEmbeddedContent() const {
  return frame_->OwnerLayoutObject();
}

void LocalFrameView::VisualViewportScrollbarsChanged() {
  if (LayoutView* layout_view = GetLayoutView())
    layout_view->Layer()->ClearClipRects();
}

void LocalFrameView::UpdateGeometriesIfNeeded() {
  if (!needs_update_geometries_)
    return;
  needs_update_geometries_ = false;
  HeapVector<Member<EmbeddedContentView>> views;
  ForAllChildViewsAndPlugins(
      [&](EmbeddedContentView& view) { views.push_back(view); });

  for (const auto& view : views) {
    // Script or plugins could detach the frame so abort processing if that
    // happens.
    if (!GetLayoutView())
      break;

    view->UpdateGeometry();
  }
}

void LocalFrameView::UpdateAllLifecyclePhases(
    DocumentLifecycle::LifecycleUpdateReason reason) {
  GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kPaintClean, reason);
}

// TODO(schenney): add a scrolling update lifecycle phase.
// TODO(schenney): Pass a LifecycleUpdateReason in here
bool LocalFrameView::UpdateLifecycleToCompositingCleanPlusScrolling() {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return UpdateAllLifecyclePhasesExceptPaint();
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kCompositingClean,
      DocumentLifecycle::LifecycleUpdateReason::kOther);
}

// TODO(schenney): Pass a LifecycleUpdateReason in here
bool LocalFrameView::UpdateLifecycleToCompositingInputsClean() {
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kCompositingInputsClean,
      DocumentLifecycle::LifecycleUpdateReason::kOther);
}

// TODO(schenney): Pass a LifecycleUpdateReason in here
bool LocalFrameView::UpdateAllLifecyclePhasesExceptPaint() {
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kPrePaintClean,
      DocumentLifecycle::LifecycleUpdateReason::kOther);
}

void LocalFrameView::UpdateLifecyclePhasesForPrinting() {
  auto* local_frame_view_root = GetFrame().LocalFrameRoot().View();
  local_frame_view_root->UpdateLifecyclePhases(
      DocumentLifecycle::kPrePaintClean,
      DocumentLifecycle::LifecycleUpdateReason::kOther);

  auto* detached_frame_view = this;
  while (detached_frame_view->IsAttached() &&
         detached_frame_view != local_frame_view_root) {
    detached_frame_view = detached_frame_view->ParentFrameView();
    CHECK(detached_frame_view);
  }

  if (detached_frame_view == local_frame_view_root)
    return;
  DCHECK(!detached_frame_view->IsAttached());

  // We are printing a detached frame or a descendant of a detached frame which
  // was not reached in some phases during during |local_frame_view_root->
  // UpdateLifecyclePhasesnormal()|. We need the subtree to be ready for
  // painting.
  detached_frame_view->UpdateLifecyclePhases(
      DocumentLifecycle::kPrePaintClean,
      DocumentLifecycle::LifecycleUpdateReason::kOther);
}

// TODO(schenney): Pass a LifecycleUpdateReason in here
bool LocalFrameView::UpdateLifecycleToLayoutClean() {
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kLayoutClean,
      DocumentLifecycle::LifecycleUpdateReason::kOther);
}

void LocalFrameView::ScheduleVisualUpdateForPaintInvalidationIfNeeded() {
  LocalFrame& local_frame_root = GetFrame().LocalFrameRoot();
  if (local_frame_root.View()->current_update_lifecycle_phases_target_state_ <
          DocumentLifecycle::kPrePaintClean ||
      Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean) {
    // Schedule visual update to process the paint invalidation in the next
    // cycle.
    local_frame_root.ScheduleVisualUpdateUnlessThrottled();
  }
  // Otherwise the paint invalidation will be handled in the pre-paint
  // phase of this cycle.
}

void LocalFrameView::NotifyResizeObservers() {
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::NotifyResizeObservers");
  // Controller exists only if ResizeObserver was created.
  if (!GetFrame().GetDocument()->GetResizeObserverController())
    return;

  ResizeObserverController& resize_controller =
      frame_->GetDocument()->EnsureResizeObserverController();

  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean);

  size_t min_depth = 0;
  for (min_depth = resize_controller.GatherObservations(0);
       min_depth != ResizeObserverController::kDepthBottom;
       min_depth = resize_controller.GatherObservations(min_depth)) {
    resize_controller.DeliverObservations();
    GetFrame().GetDocument()->UpdateStyleAndLayout(Document::IsNotForcedLayout);
  }

  if (resize_controller.SkippedObservations()) {
    resize_controller.ClearObservations();
    ErrorEvent* error = ErrorEvent::Create(
        "ResizeObserver loop limit exceeded",
        SourceLocation::Capture(frame_->GetDocument()), nullptr);
    // We're using |SanitizeScriptErrors::kDoNotSanitize| as the error is made
    // by blink itself.
    // TODO(yhirano): Reconsider this.
    frame_->GetDocument()->DispatchErrorEvent(
        error, SanitizeScriptErrors::kDoNotSanitize);
    // Ensure notifications will get delivered in next cycle.
    ScheduleAnimation();
  }

  DCHECK(!GetLayoutView()->NeedsLayout());
}

void LocalFrameView::DispatchEventsForPrintingOnAllFrames() {
  DCHECK(frame_->IsMainFrame());
  for (Frame* current_frame = frame_; current_frame;
       current_frame = current_frame->Tree().TraverseNext(frame_)) {
    auto* current_local_frame = DynamicTo<LocalFrame>(current_frame);
    if (current_local_frame)
      current_local_frame->GetDocument()->DispatchEventsForPrinting();
  }
}

void LocalFrameView::SetupPrintContext() {
  if (frame_->GetDocument()->Printing())
    return;
  if (!print_context_) {
    print_context_ = MakeGarbageCollected<PrintContext>(
        frame_, /*use_printing_layout=*/true);
  }
  if (frame_->GetSettings())
    frame_->GetSettings()->SetShouldPrintBackgrounds(true);
  bool is_us = DefaultLanguage() == "en-US";
  int width = is_us ? kLetterPortraitPageWidth : kA4PortraitPageWidth;
  int height = is_us ? kLetterPortraitPageHeight : kA4PortraitPageHeight;
  print_context_->BeginPrintMode(width, height);
  print_context_->ComputePageRects(FloatSize(width, height));
  DispatchEventsForPrintingOnAllFrames();
}

void LocalFrameView::ClearPrintContext() {
  if (!print_context_)
    return;
  print_context_->EndPrintMode();
  print_context_.Clear();
}

// TODO(leviw): We don't assert lifecycle information from documents in child
// WebPluginContainerImpls.
bool LocalFrameView::UpdateLifecyclePhases(
    DocumentLifecycle::LifecycleState target_state,
    DocumentLifecycle::LifecycleUpdateReason reason) {
  // If the lifecycle is postponed, which can happen if the inspector requests
  // it, then we shouldn't update any lifecycle phases.
  if (UNLIKELY(frame_->GetDocument() &&
               frame_->GetDocument()->Lifecycle().LifecyclePostponed())) {
    return false;
  }

  // Prevent reentrance.
  // TODO(vmpstr): Should we just have a DCHECK instead here?
  if (UNLIKELY(current_update_lifecycle_phases_target_state_ !=
               DocumentLifecycle::kUninitialized)) {
    NOTREACHED()
        << "LocalFrameView::updateLifecyclePhasesInternal() reentrance";
    return false;
  }

  // This must be called from the root frame, or a detached frame for printing,
  // since it recurses down, not up. Otherwise the lifecycles of the frames
  // might be out of sync.
  DCHECK(frame_->IsLocalRoot() || !IsAttached());

  // Only the following target states are supported.
  DCHECK(target_state == DocumentLifecycle::kLayoutClean ||
         target_state == DocumentLifecycle::kCompositingInputsClean ||
         target_state == DocumentLifecycle::kCompositingClean ||
         target_state == DocumentLifecycle::kPrePaintClean ||
         target_state == DocumentLifecycle::kPaintClean);
  lifecycle_update_count_for_testing_++;

  // If the document is not active then it is either not yet initialized, or it
  // is stopping. In either case, we can't reach one of the supported target
  // states.
  if (!frame_->GetDocument()->IsActive())
    return false;

  // This is used to guard against reentrance. It is also used in conjunction
  // with the current lifecycle state to determine which phases are yet to run
  // in this cycle.
  base::AutoReset<DocumentLifecycle::LifecycleState> target_state_scope(
      &current_update_lifecycle_phases_target_state_, target_state);
  // This is used to check if we're within a lifecycle update but have passed
  // the layout update phase. Note there is a bit of a subtlety here: it's not
  // sufficient for us to check the current lifecycle state, since it can be
  // past kLayoutClean but the function to run style and layout phase has not
  // actually been run yet. Since this bool affects throttling, and throttling,
  // in turn, determines whether style and layout function will run, we need a
  // separate bool.
  base::AutoReset<bool> past_layout_lifecycle_resetter(
      &past_layout_lifecycle_update_, false);

  // If we're throttling, then we don't need to update lifecycle phases. The
  // throttling status will get updated in RunPostLifecycleSteps().
  if (ShouldThrottleRendering()) {
    return Lifecycle().GetState() == target_state;
  }

  base::AutoReset<bool> in_lifecycle_scope(&in_lifecycle_update_, true);
  lifecycle_data_.start_time = base::TimeTicks::Now();
  ++lifecycle_data_.count;

  {
    TRACE_EVENT0("blink", "LocalFrameView::WillStartLifecycleUpdate");

    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      auto lifecycle_observers = frame_view.lifecycle_observers_;
      for (auto& observer : lifecycle_observers)
        observer->WillStartLifecycleUpdate(frame_view);
    });
  }

  // If we're in PrintBrowser mode, setup a print context.
  // TODO(vmpstr): It doesn't seem like we need to do this every lifecycle
  // update, but rather once somewhere at creation time.
  if (RuntimeEnabledFeatures::PrintBrowserEnabled())
    SetupPrintContext();
  else
    ClearPrintContext();

  // Run the lifecycle updates.
  UpdateLifecyclePhasesInternal(target_state);

  {
    TRACE_EVENT0("blink", "LocalFrameView::DidFinishLifecycleUpdate");

    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      auto lifecycle_observers = frame_view.lifecycle_observers_;
      for (auto& observer : lifecycle_observers)
        observer->DidFinishLifecycleUpdate(frame_view);
    });
  }

  return Lifecycle().GetState() == target_state;
}

void LocalFrameView::UpdateLifecyclePhasesInternal(
    DocumentLifecycle::LifecycleState target_state) {
  bool run_more_lifecycle_phases =
      RunStyleAndLayoutLifecyclePhases(target_state);
  if (!run_more_lifecycle_phases)
    return;
  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean);

  if (!GetLayoutView())
    return;

#if DCHECK_IS_ON()
  DisallowLayoutInvalidationScope disallow_layout_invalidation(this);
#endif

  {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "SetLayerTreeId", TRACE_EVENT_SCOPE_THREAD, "data",
                         inspector_set_layer_tree_id::Data(frame_.Get()));
    TRACE_EVENT1("devtools.timeline", "UpdateLayerTree", "data",
                 inspector_update_layer_tree_event::Data(frame_.Get()));

    run_more_lifecycle_phases = RunCompositingLifecyclePhase(target_state);
    if (!run_more_lifecycle_phases)
      return;

    // TODO(pdr): PrePaint should be under the "Paint" devtools timeline step
    // when CompositeAfterPaint is enabled.
    run_more_lifecycle_phases = RunPrePaintLifecyclePhase(target_state);
    DCHECK(ShouldThrottleRendering() ||
           Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean);
    if (!run_more_lifecycle_phases)
      return;
  }

  // Now that we have run the lifecycle up to paint, we can reset
  // |need_paint_phase_after_throttling_| so that the paint phase will
  // properly see us as being throttled (if that was the only reason we remained
  // unthrottled), and clear the painted output.
  need_paint_phase_after_throttling_ = false;

  DCHECK_EQ(target_state, DocumentLifecycle::kPaintClean);
  RunPaintLifecyclePhase();
  DCHECK(ShouldThrottleRendering() ||
         (frame_->GetDocument()->Printing() &&
          !RuntimeEnabledFeatures::PrintBrowserEnabled()) ||
         Lifecycle().GetState() == DocumentLifecycle::kPaintClean);
}

bool LocalFrameView::RunStyleAndLayoutLifecyclePhases(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::RunStyleAndLayoutLifecyclePhases");
  {
    SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                             LocalFrameUkmAggregator::kStyleAndLayout);
    UpdateStyleAndLayoutIfNeededRecursive();
  }
  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean);

  frame_->GetDocument()
      ->GetRootScrollerController()
      .PerformRootScrollerSelection();

  // PerformRootScrollerSelection can dirty layout if an effective root
  // scroller is changed so make sure we get back to LayoutClean.
  if (RuntimeEnabledFeatures::ImplicitRootScrollerEnabled() ||
      RuntimeEnabledFeatures::SetRootScrollerEnabled()) {
    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      if (frame_view.NeedsLayout())
        frame_view.UpdateLayout();

      DCHECK(frame_view.Lifecycle().GetState() >=
             DocumentLifecycle::kLayoutClean);
    });
  }

  if (target_state == DocumentLifecycle::kLayoutClean)
    return false;

  // This will be reset by AutoReset in the calling function
  // (UpdateLifecyclePhases()).
  past_layout_lifecycle_update_ = true;

  // After layout and the |past_layout_lifecycle_update_| update, the value of
  // ShouldThrottleRendering() can change. OOPIF local frame roots that are
  // throttled can return now that layout is clean. This situation happens if
  // the throttling was disabled due to required intersection observation, which
  // can now be run.
  if (ShouldThrottleRendering())
    return false;

  // Now we can run post layout steps in preparation for further phases.
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.PerformScrollAnchoringAdjustments();
  });

  frame_->GetPage()->GetValidationMessageClient().LayoutOverlay();

  if (target_state == DocumentLifecycle::kPaintClean) {
    ForAllNonThrottledLocalFrameViews(
        [](LocalFrameView& frame_view) { frame_view.NotifyResizeObservers(); });

    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      frame_view.NotifyFrameRectsChangedIfNeeded();
    });
  }
  // If we exceed the number of re-layouts during ResizeObserver notifications,
  // then we shouldn't continue with the lifecycle updates. At that time, we
  // have scheduled an animation and we'll try again.
  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean ||
         Lifecycle().GetState() == DocumentLifecycle::kVisualUpdatePending);
  return Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean;
}

bool LocalFrameView::RunCompositingLifecyclePhase(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::RunCompositingLifecyclePhase");
  auto* layout_view = GetLayoutView();
  DCHECK(layout_view);

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                             LocalFrameUkmAggregator::kCompositing);
    layout_view->Compositor()->UpdateIfNeededRecursive(target_state);
  } else {
#if DCHECK_IS_ON()
    SetIsUpdatingDescendantDependentFlags(true);
#endif
    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      frame_view.GetLayoutView()->Layer()->UpdateDescendantDependentFlags();
      frame_view.GetLayoutView()->CommitPendingSelection();
    });
#if DCHECK_IS_ON()
    SetIsUpdatingDescendantDependentFlags(false);
#endif
  }

  // We may be in kCompositingInputsClean clean, which does not need to notify
  // the global root scroller controller.
  if (target_state >= DocumentLifecycle::kCompositingClean) {
    frame_->GetPage()->GlobalRootScrollerController().DidUpdateCompositing(
        *this);
  }

  return target_state > DocumentLifecycle::kCompositingClean;
}

bool LocalFrameView::RunPrePaintLifecyclePhase(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::RunPrePaintLifecyclePhase");

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPrePaint);
    if (frame_view.CanThrottleRendering()) {
      // This frame can be throttled but not throttled, meaning we are not in an
      // AllowThrottlingScope. Now this frame may contain dirty paint flags, and
      // we need to propagate the flags into the ancestor chain so that
      // PrePaintTreeWalk can reach this frame.
      frame_view.SetNeedsPaintPropertyUpdate();
      // We may record more foreign layers under the frame.
      frame_view.GraphicsLayersDidChange();
      if (auto* owner = frame_view.GetLayoutEmbeddedContent())
        owner->SetShouldCheckForPaintInvalidation();
    }
  });

  {
    SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                             LocalFrameUkmAggregator::kPrePaint);

    PrePaintTreeWalk().WalkTree(*this);
  }

  UpdateCompositedSelectionIfNeeded();

  frame_->GetPage()->GetValidationMessageClient().UpdatePrePaint();
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& view) {
    view.frame_->UpdateFrameColorOverlayPrePaint();
  });
  if (auto* web_local_frame_impl = WebLocalFrameImpl::FromFrame(frame_))
    web_local_frame_impl->UpdateDevToolsOverlaysPrePaint();

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPrePaintClean);
  });

  return target_state > DocumentLifecycle::kPrePaintClean;
}

template <typename Function>
static void ForAllGraphicsLayers(GraphicsLayer& layer,
                                 const Function& function) {
  function(layer);
  for (auto* child : layer.Children())
    ForAllGraphicsLayers(*child, function);
}

void LocalFrameView::RunPaintLifecyclePhase() {
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::RunPaintLifecyclePhase");
  // While printing a document, the paint walk is done by the printing component
  // into a special canvas. There is no point doing a normal paint step (or
  // animations update) when in this mode.
  //
  // RuntimeEnabledFeatures::PrintBrowserEnabled is a mode which runs the
  // browser normally, but renders every page as if it were being printed.
  // See crbug.com/667547
  bool print_mode_enabled = frame_->GetDocument()->Printing() &&
                            !RuntimeEnabledFeatures::PrintBrowserEnabled();
  if (!print_mode_enabled)
    PaintTree();

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    if (GetLayoutView()->Compositor()->InCompositingMode()) {
      GetScrollingCoordinator()->UpdateAfterPaint(this);
    }
  }

  if (!print_mode_enabled) {
    bool needed_update = !paint_artifact_compositor_ ||
                         paint_artifact_compositor_->NeedsUpdate();
    PushPaintArtifactToCompositor();
    ForAllNonThrottledLocalFrameViews([this](LocalFrameView& frame_view) {
      frame_view.GetScrollableArea()->UpdateCompositorScrollAnimations();
      if (const auto* animating_scrollable_areas =
              frame_view.AnimatingScrollableAreas()) {
        for (PaintLayerScrollableArea* area : *animating_scrollable_areas)
          area->UpdateCompositorScrollAnimations();
      }
      DocumentAnimations::UpdateAnimations(
          frame_view.GetLayoutView()->GetDocument(),
          DocumentLifecycle::kPaintClean, paint_artifact_compositor_.get());
    });

    // Initialize animation properties in the newly created paint property
    // nodes according to the current animation state. This is mainly for
    // the running composited animations which didn't change state during
    // above UpdateAnimations() but associated with new paint property nodes.
    if (needed_update) {
      auto* root_layer = RootCcLayer();
      if (root_layer && root_layer->layer_tree_host()) {
        root_layer->layer_tree_host()
            ->mutator_host()
            ->InitClientAnimationState();
      }
    }

    // Notify the controller that the artifact has been pushed and some
    // lifecycle state can be freed (such as raster invalidations).
    if (paint_controller_)
      paint_controller_->FinishCycle();

    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      // Property tree changed state is typically cleared through
      // |PaintController::FinishCycle| but that will be a no-op because
      // the paint controller is transient, so force the changed state to be
      // cleared here.
      if (paint_controller_) {
        paint_controller_->ClearPropertyTreeChangedStateTo(
            PropertyTreeState::Root());
      }
      auto* root = GetLayoutView()->Compositor()->PaintRootGraphicsLayer();
      if (root) {
        ForAllGraphicsLayers(*root, [](GraphicsLayer& layer) {
          if (layer.PaintsContentOrHitTest() && layer.HasLayerState()) {
            layer.GetPaintController().ClearPropertyTreeChangedStateTo(
                layer.GetPropertyTreeState());
          }
        });
      }
    }
  }
}

void LocalFrameView::EnqueueScrollAnchoringAdjustment(
    ScrollableArea* scrollable_area) {
  anchoring_adjustment_queue_.insert(scrollable_area);
}

void LocalFrameView::DequeueScrollAnchoringAdjustment(
    ScrollableArea* scrollable_area) {
  anchoring_adjustment_queue_.erase(scrollable_area);
}

void LocalFrameView::PerformScrollAnchoringAdjustments() {
  // Adjust() will cause a scroll which could end up causing a layout and
  // reentering this method. Copy and clear the queue so we don't modify it
  // during iteration.
  AnchoringAdjustmentQueue queue_copy = anchoring_adjustment_queue_;
  anchoring_adjustment_queue_.clear();

  for (WeakMember<ScrollableArea>& scroller : queue_copy) {
    if (scroller) {
      DCHECK(scroller->GetScrollAnchor());
      scroller->GetScrollAnchor()->Adjust();
    }
  }
}

static void RecordGraphicsLayerAsForeignLayer(
    GraphicsContext& context,
    const GraphicsLayer* graphics_layer) {
  // TODO(trchen): Currently the GraphicsLayer hierarchy is still built during
  // CompositingUpdate, and we have to clear them here to ensure no extraneous
  // layers are still attached. In future we will disable all those layer
  // hierarchy code so we won't need this line.
  graphics_layer->CcLayer()->RemoveAllChildren();
  RecordForeignLayer(context, DisplayItem::kForeignLayerWrapper,
                     graphics_layer->CcLayer(),
                     FloatPoint(graphics_layer->GetOffsetFromTransformNode()),
                     graphics_layer->GetPropertyTreeState());
}

static void CollectViewportLayersForLayerList(GraphicsContext& context,
                                              VisualViewport& visual_viewport) {
  RecordGraphicsLayerAsForeignLayer(context, visual_viewport.ContainerLayer());
  RecordGraphicsLayerAsForeignLayer(context, visual_viewport.PageScaleLayer());
  RecordGraphicsLayerAsForeignLayer(context, visual_viewport.ScrollLayer());
}

static void CollectDrawableLayersForLayerListRecursively(
    GraphicsContext& context,
    const GraphicsLayer* layer) {
  if (!layer || layer->Client().ShouldThrottleRendering() ||
      layer->Client().IsUnderSVGHiddenContainer()) {
    return;
  }

  if (layer->Client().PaintBlockedByDisplayLockIncludingAncestors(
          DisplayLockContextLifecycleTarget::kSelf)) {
    // If we skip the layer, then we need to ensure to notify the
    // display-lock, since we need to force recollect the layers when we commit.
    layer->Client().NotifyDisplayLockNeedsGraphicsLayerCollection();
    return;
  }

  // We need to collect all layers that draw content, as well as some layers
  // that don't for the purposes of hit testing. For example, an empty div
  // will not draw content but needs to create a layer to ensure scroll events
  // do not pass through it.
  if (layer->PaintsContentOrHitTest() || layer->GetHitTestable())
    RecordGraphicsLayerAsForeignLayer(context, layer);

  if (auto* contents_layer = layer->ContentsLayer()) {
    RecordForeignLayer(context, DisplayItem::kForeignLayerContentsWrapper,
                       contents_layer,
                       FloatPoint(layer->GetContentsOffsetFromTransformNode()),
                       layer->GetContentsPropertyTreeState());
  }

  for (const auto* child : layer->Children())
    CollectDrawableLayersForLayerListRecursively(context, child);
  CollectDrawableLayersForLayerListRecursively(context, layer->MaskLayer());
}

static void CollectLinkHighlightLayersForLayerListRecursively(
    GraphicsContext& context,
    const GraphicsLayer* layer) {
  if (!layer || layer->Client().ShouldThrottleRendering())
    return;

  for (auto* highlight : layer->GetLinkHighlights()) {
    auto property_tree_state = layer->GetPropertyTreeState();
    property_tree_state.SetEffect(highlight->Effect());
    RecordForeignLayer(
        context, DisplayItem::kForeignLayerLinkHighlight, highlight->Layer(),
        highlight->GetOffsetFromTransformNode(), property_tree_state);
  }

  for (const auto* child : layer->Children())
    CollectLinkHighlightLayersForLayerListRecursively(context, child);
}

static void PaintGraphicsLayerRecursively(GraphicsLayer* layer) {
  layer->PaintRecursively();

#if DCHECK_IS_ON()
  VerboseLogGraphicsLayerTree(layer);
#endif
}

void LocalFrameView::PaintTree() {
  SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                           LocalFrameUkmAggregator::kPaint);

  DCHECK(GetFrame().IsLocalRoot());

  auto* layout_view = GetLayoutView();
  DCHECK(layout_view);
  paint_frame_count_++;
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
  });

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    if (!paint_controller_)
      paint_controller_ = std::make_unique<PaintController>();

    // TODO(crbug.com/917911): Painting of overlays should not force repainting
    // of the frame contents.
    auto* web_local_frame_impl = WebLocalFrameImpl::FromFrame(frame_);
    bool has_dev_tools_overlays =
        web_local_frame_impl && web_local_frame_impl->HasDevToolsOverlays();
    if (!GetLayoutView()->Layer()->NeedsRepaint() && !has_dev_tools_overlays) {
      paint_controller_->UpdateUMACountsOnFullyCached();
    } else {
      GraphicsContext graphics_context(*paint_controller_);
      if (RuntimeEnabledFeatures::PrintBrowserEnabled())
        graphics_context.SetPrinting(true);

      if (Settings* settings = frame_->GetSettings()) {
        graphics_context.SetDarkMode(
            BuildDarkModeSettings(*settings, *GetLayoutView()));
      }

      PaintInternal(graphics_context, kGlobalPaintNormalPhase,
                    CullRect::Infinite());

      frame_->GetPage()->GetLinkHighlights().Paint(graphics_context);

      frame_->GetPage()->GetValidationMessageClient().PaintOverlay(
          graphics_context);
      ForAllNonThrottledLocalFrameViews(
          [&graphics_context](LocalFrameView& view) {
            view.frame_->PaintFrameColorOverlay(graphics_context);
          });

      // Devtools overlays query the inspected page's paint data so this update
      // needs to be after other paintings.
      if (has_dev_tools_overlays)
        web_local_frame_impl->PaintDevToolsOverlays(graphics_context);

      paint_controller_->CommitNewDisplayItems();
    }
  } else {
    // A null graphics layer can occur for painting of SVG images that are not
    // parented into the main frame tree, or when the LocalFrameView is the main
    // frame view of a page overlay. The page overlay is in the layer tree of
    // the host page and will be painted during painting of the host page.
    if (GraphicsLayer* root_graphics_layer =
            layout_view->Compositor()->PaintRootGraphicsLayer()) {
      PaintGraphicsLayerRecursively(root_graphics_layer);
    }

    // This uses an invalidation approach based on graphics layer raster
    // invalidation so it must be after paint. This adds/removes link highlight
    // layers so it must be before
    // |CollectDrawableLayersForLayerListRecursively|.
    frame_->GetPage()->GetLinkHighlights().UpdateGeometry();
  }

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
    if (auto* layout_view = frame_view.GetLayoutView())
      layout_view->Layer()->ClearNeedsRepaintRecursively();
    if (RuntimeEnabledFeatures::FirstContentfulPaintPlusPlusEnabled() ||
        RuntimeEnabledFeatures::ElementTimingEnabled(
            frame_view.GetFrame().GetDocument())) {
      frame_view.GetPaintTimingDetector().NotifyPaintFinished();
    }
  });

  PaintController::ReportUMACounts();
}

const cc::Layer* LocalFrameView::RootCcLayer() const {
  return paint_artifact_compositor_ ? paint_artifact_compositor_->RootLayer()
                                    : nullptr;
}

void LocalFrameView::PushPaintArtifactToCompositor() {
  TRACE_EVENT0("blink", "LocalFrameView::pushPaintArtifactToCompositor");
  if (!frame_->GetSettings()->GetAcceleratedCompositingEnabled())
    return;

  Page* page = GetFrame().GetPage();
  if (!page)
    return;

  if (!paint_artifact_compositor_) {
    paint_artifact_compositor_ =
        std::make_unique<PaintArtifactCompositor>(WTF::BindRepeating(
            &ScrollingCoordinator::DidScroll,
            // The layer being scrolled is destroyed before the
            // ScrollingCoordinator.
            WrapWeakPersistent(page->GetScrollingCoordinator())));
    GetLayoutView()->Compositor()->AttachRootLayerViaChromeClient();
    page->GetChromeClient().AttachRootLayer(
        paint_artifact_compositor_->RootLayer(), &GetFrame());
  }

  SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                           LocalFrameUkmAggregator::kCompositingCommit);

  // Skip updating property trees, pushing cc::Layers, and issuing raster
  // invalidations if possible.
  if (!paint_artifact_compositor_->NeedsUpdate()) {
    DCHECK(paint_controller_);
    return;
  }

  PaintArtifactCompositor::ViewportProperties viewport_properties;
  if (GetFrame().IsMainFrame()) {
    const auto& viewport = page->GetVisualViewport();
    viewport_properties.page_scale = viewport.GetPageScaleNode();
    viewport_properties.inner_scroll_translation =
        viewport.GetScrollTranslationNode();
  }

  PaintArtifactCompositor::Settings settings;
  settings.prefer_compositing_to_lcd_text =
      page->GetSettings().GetPreferCompositingToLCDTextEnabled();

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      !paint_controller_) {
    // Before CompositeAfterPaint, we need a transient PaintController to
    // collect the foreign layers, and this doesn't need caching. This shouldn't
    // affect caching status of DisplayItemClients because FinishCycle() is
    // not synchronized with other PaintControllers. This may live across frame
    // updates until GraphicsLayersDidChange() is called.
    paint_controller_ =
        std::make_unique<PaintController>(PaintController::kTransient);

    GraphicsContext context(*paint_controller_);
    // Note: Some blink unit tests run without turning on compositing which
    // means we don't create viewport layers. OOPIFs also don't have their own
    // viewport layers.
    if (GetLayoutView()->Compositor()->InCompositingMode() &&
        GetFrame() == GetPage()->MainFrame()) {
      // TODO(bokan): We should eventually stop creating layers for the visual
      // viewport. At that point, we can remove this. However, for now, CC
      // still has some dependencies on the viewport scale and scroll layers.
      CollectViewportLayersForLayerList(context,
                                        frame_->GetPage()->GetVisualViewport());
    }
    // Before CompositeAfterPaint, |PaintRootGraphicsLayer| is the ancestor of
    // all drawable layers (see: PaintLayerCompositor::PaintRootGraphicsLayer)
    // so we do not need to collect scrollbars separately.
    auto* root = GetLayoutView()->Compositor()->PaintRootGraphicsLayer();
    CollectDrawableLayersForLayerListRecursively(context, root);
    // Link highlights paint after all other layers.
    if (!frame_->GetPage()->GetLinkHighlights().IsEmpty())
      CollectLinkHighlightLayersForLayerListRecursively(context, root);

    paint_controller_->CommitNewDisplayItems();
  }

  paint_artifact_compositor_->Update(
      paint_controller_->GetPaintArtifactShared(), viewport_properties,
      settings);

  probe::LayerTreePainted(&GetFrame());
}

std::unique_ptr<JSONObject> LocalFrameView::CompositedLayersAsJSON(
    LayerTreeFlags flags) {
  return GetFrame()
      .LocalFrameRoot()
      .View()
      ->paint_artifact_compositor_->LayersAsJSON(flags);
}

void LocalFrameView::UpdateStyleAndLayoutIfNeededRecursive() {
  if (ShouldThrottleRendering() || !frame_->GetDocument()->IsActive())
    return;

  ScopedFrameBlamer frame_blamer(frame_);
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::updateStyleAndLayoutIfNeededRecursive");

  // We have to crawl our entire subtree looking for any FrameViews that need
  // layout and make sure they are up to date.
  // Mac actually tests for intersection with the dirty region and tries not to
  // update layout for frames that are outside the dirty region.  Not only does
  // this seem pointless (since those frames will have set a zero timer to
  // layout anyway), but it is also incorrect, since if two frames overlap, the
  // first could be excluded from the dirty region but then become included
  // later by the second frame adding rects to the dirty region when it lays
  // out.

  {
    SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                             LocalFrameUkmAggregator::kStyle);
    frame_->GetDocument()->UpdateStyleAndLayoutTree();

    // Update style for all embedded SVG documents underneath this frame, so
    // that intrinsic size computation for any embedded objects has up-to-date
    // information before layout.
    ForAllChildLocalFrameViews([](LocalFrameView& view) {
      Document& document = *view.GetFrame().GetDocument();
      if (document.IsSVGDocument())
        document.UpdateStyleAndLayoutTree();
    });
  }

  CHECK(!ShouldThrottleRendering());
  CHECK(frame_->GetDocument()->IsActive());
  CHECK(!nested_layout_count_);

  if (NeedsLayout()) {
    SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                             LocalFrameUkmAggregator::kLayout);
    UpdateLayout();
  }

  CheckDoesNotNeedLayout();

  // WebView plugins need to update regardless of whether the
  // LayoutEmbeddedObject that owns them needed layout.
  // TODO(schenney): This currently runs the entire lifecycle on plugin
  // WebViews. We should have a way to only run these other Documents to the
  // same lifecycle stage as this frame.
  for (const auto& plugin : plugins_) {
    plugin->UpdateAllLifecyclePhases();
  }
  CheckDoesNotNeedLayout();

  // FIXME: Calling layout() shouldn't trigger script execution or have any
  // observable effects on the frame tree but we're not quite there yet.
  HeapVector<Member<LocalFrameView>> frame_views;
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (!child_local_frame)
      continue;
    if (LocalFrameView* view = child_local_frame->View())
      frame_views.push_back(view);
  }

  for (const auto& frame_view : frame_views)
    frame_view->UpdateStyleAndLayoutIfNeededRecursive();

  // These asserts ensure that parent frames are clean, when child frames
  // finished updating layout and style.
  // TODO(szager): this is the last call to CheckDoesNotNeedLayout during the
  // lifecycle code, but it can happen that NeedsLayout() becomes true after
  // this point, even while the document lifecycle proceeds to kLayoutClean
  // and beyond. Figure out how this happens, and do something sensible.
  CheckDoesNotNeedLayout();
#if DCHECK_IS_ON()
  frame_->GetDocument()->GetLayoutView()->AssertLaidOut();
#endif

  UpdateGeometriesIfNeeded();

  if (Lifecycle().GetState() < DocumentLifecycle::kLayoutClean)
    Lifecycle().AdvanceTo(DocumentLifecycle::kLayoutClean);

  if (AXObjectCache* cache = GetFrame().GetDocument()->ExistingAXObjectCache())
    cache->ProcessUpdatesAfterLayout(*GetFrame().GetDocument());

  // Ensure that we become visually non-empty eventually.
  // TODO(esprehn): This should check isRenderingReady() instead.
  if (GetFrame().GetDocument()->HasFinishedParsing() &&
      GetFrame().Loader().StateMachine()->CommittedFirstRealDocumentLoad())
    is_visually_non_empty_ = true;

  GetFrame().Selection().UpdateStyleAndLayoutIfNeeded();
  GetFrame().GetPage()->GetDragCaret().UpdateStyleAndLayoutIfNeeded();
}

void LocalFrameView::EnableAutoSizeMode(const IntSize& min_size,
                                        const IntSize& max_size) {
  if (!auto_size_info_)
    auto_size_info_ = MakeGarbageCollected<FrameViewAutoSizeInfo>(this);

  auto_size_info_->ConfigureAutoSizeMode(min_size, max_size);
  SetLayoutSizeFixedToFrameSize(true);
  SetNeedsLayout();
  ScheduleRelayout();
}

void LocalFrameView::DisableAutoSizeMode() {
  if (!auto_size_info_)
    return;

  SetLayoutSizeFixedToFrameSize(false);
  SetNeedsLayout();
  ScheduleRelayout();

  // Since autosize mode forces the scrollbar mode, change them to being auto.
  GetLayoutView()->SetAutosizeScrollbarModes(ScrollbarMode::kAuto,
                                             ScrollbarMode::kAuto);
  auto_size_info_.Clear();
}

void LocalFrameView::ForceLayoutForPagination(
    const FloatSize& page_size,
    const FloatSize& original_page_size,
    float maximum_shrink_factor) {
  // Dumping externalRepresentation(m_frame->layoutObject()).ascii() is a good
  // trick to see the state of things before and after the layout
  if (LayoutView* layout_view = this->GetLayoutView()) {
    float page_logical_width = layout_view->StyleRef().IsHorizontalWritingMode()
                                   ? page_size.Width()
                                   : page_size.Height();
    float page_logical_height =
        layout_view->StyleRef().IsHorizontalWritingMode() ? page_size.Height()
                                                          : page_size.Width();

    LayoutUnit floored_page_logical_width =
        static_cast<LayoutUnit>(page_logical_width);
    LayoutUnit floored_page_logical_height =
        static_cast<LayoutUnit>(page_logical_height);
    layout_view->SetLogicalWidth(floored_page_logical_width);
    layout_view->SetPageLogicalHeight(floored_page_logical_height);
    layout_view->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kPrintingChanged);
    UpdateLayout();

    // If we don't fit in the given page width, we'll lay out again. If we don't
    // fit in the page width when shrunk, we will lay out at maximum shrink and
    // clip extra content.
    // FIXME: We are assuming a shrink-to-fit printing implementation.  A
    // cropping implementation should not do this!
    bool horizontal_writing_mode =
        layout_view->StyleRef().IsHorizontalWritingMode();
    PhysicalRect document_rect(layout_view->DocumentRect());
    LayoutUnit doc_logical_width = horizontal_writing_mode
                                       ? document_rect.Width()
                                       : document_rect.Height();
    if (doc_logical_width > page_logical_width) {
      FloatSize expected_page_size(
          std::min<float>(document_rect.Width().ToFloat(),
                          page_size.Width() * maximum_shrink_factor),
          std::min<float>(document_rect.Height().ToFloat(),
                          page_size.Height() * maximum_shrink_factor));
      FloatSize max_page_size = frame_->ResizePageRectsKeepingRatio(
          FloatSize(original_page_size.Width(), original_page_size.Height()),
          expected_page_size);
      page_logical_width = horizontal_writing_mode ? max_page_size.Width()
                                                   : max_page_size.Height();
      page_logical_height = horizontal_writing_mode ? max_page_size.Height()
                                                    : max_page_size.Width();

      floored_page_logical_width = static_cast<LayoutUnit>(page_logical_width);
      floored_page_logical_height =
          static_cast<LayoutUnit>(page_logical_height);
      layout_view->SetLogicalWidth(floored_page_logical_width);
      layout_view->SetPageLogicalHeight(floored_page_logical_height);
      layout_view->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          layout_invalidation_reason::kPrintingChanged);
      UpdateLayout();

      PhysicalRect updated_document_rect(layout_view->DocumentRect());
      LayoutUnit doc_logical_height = horizontal_writing_mode
                                          ? updated_document_rect.Height()
                                          : updated_document_rect.Width();
      LayoutUnit doc_logical_top = horizontal_writing_mode
                                       ? updated_document_rect.Y()
                                       : updated_document_rect.X();
      LayoutUnit doc_logical_right = horizontal_writing_mode
                                         ? updated_document_rect.Right()
                                         : updated_document_rect.Bottom();
      LayoutUnit clipped_logical_left;
      if (!layout_view->StyleRef().IsLeftToRightDirection()) {
        clipped_logical_left =
            LayoutUnit(doc_logical_right - page_logical_width);
      }
      LayoutRect overflow(clipped_logical_left, doc_logical_top,
                          LayoutUnit(page_logical_width), doc_logical_height);

      if (!horizontal_writing_mode)
        overflow = overflow.TransposedRect();
      AdjustViewSizeAndLayout();
      // This is how we clip in case we overflow again.
      layout_view->ClearLayoutOverflow();
      layout_view->AddLayoutOverflow(overflow);
      return;
    }
  }

  if (TextAutosizer* text_autosizer = frame_->GetDocument()->GetTextAutosizer())
    text_autosizer->UpdatePageInfo();
  AdjustViewSizeAndLayout();
}

IntRect LocalFrameView::ConvertFromLayoutObject(
    const LayoutObject& layout_object,
    const IntRect& layout_object_rect) const {
  // Convert from page ("absolute") to LocalFrameView coordinates.
  return PixelSnappedIntRect(
      layout_object.LocalToAbsoluteRect(PhysicalRect(layout_object_rect)));
}

IntRect LocalFrameView::ConvertToLayoutObject(const LayoutObject& layout_object,
                                              const IntRect& frame_rect) const {
  return PixelSnappedIntRect(
      layout_object.AbsoluteToLocalRect(PhysicalRect(frame_rect)));
}

IntPoint LocalFrameView::ConvertFromLayoutObject(
    const LayoutObject& layout_object,
    const IntPoint& layout_object_point) const {
  return RoundedIntPoint(ConvertFromLayoutObject(
      layout_object, PhysicalOffset(layout_object_point)));
}

IntPoint LocalFrameView::ConvertToLayoutObject(
    const LayoutObject& layout_object,
    const IntPoint& frame_point) const {
  return RoundedIntPoint(
      ConvertToLayoutObject(layout_object, PhysicalOffset(frame_point)));
}

PhysicalOffset LocalFrameView::ConvertFromLayoutObject(
    const LayoutObject& layout_object,
    const PhysicalOffset& layout_object_offset) const {
  return layout_object.LocalToAbsolutePoint(layout_object_offset);
}

PhysicalOffset LocalFrameView::ConvertToLayoutObject(
    const LayoutObject& layout_object,
    const PhysicalOffset& frame_offset) const {
  return PhysicalOffset::FromFloatPointRound(
      ConvertToLayoutObject(layout_object, FloatPoint(frame_offset)));
}

FloatPoint LocalFrameView::ConvertToLayoutObject(
    const LayoutObject& layout_object,
    const FloatPoint& frame_point) const {
  return layout_object.AbsoluteToLocalFloatPoint(frame_point);
}

IntPoint LocalFrameView::ConvertSelfToChild(const EmbeddedContentView& child,
                                            const IntPoint& point) const {
  IntPoint new_point(point);
  new_point.MoveBy(-child.Location());
  return new_point;
}

IntRect LocalFrameView::RootFrameToDocument(const IntRect& rect_in_root_frame) {
  IntPoint offset = RootFrameToDocument(rect_in_root_frame.Location());
  IntRect local_rect = rect_in_root_frame;
  local_rect.SetLocation(offset);
  return local_rect;
}

IntPoint LocalFrameView::RootFrameToDocument(
    const IntPoint& point_in_root_frame) {
  return FlooredIntPoint(RootFrameToDocument(FloatPoint(point_in_root_frame)));
}

FloatPoint LocalFrameView::RootFrameToDocument(
    const FloatPoint& point_in_root_frame) {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return point_in_root_frame;

  FloatPoint local_frame = ConvertFromRootFrame(point_in_root_frame);
  return local_frame + layout_viewport->GetScrollOffset();
}

IntRect LocalFrameView::DocumentToFrame(const IntRect& rect_in_document) const {
  IntRect rect_in_frame = rect_in_document;
  rect_in_frame.SetLocation(DocumentToFrame(rect_in_document.Location()));
  return rect_in_frame;
}

DoublePoint LocalFrameView::DocumentToFrame(
    const DoublePoint& point_in_document) const {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return point_in_document;

  return point_in_document - layout_viewport->GetScrollOffset();
}

IntPoint LocalFrameView::DocumentToFrame(
    const IntPoint& point_in_document) const {
  return FlooredIntPoint(DocumentToFrame(DoublePoint(point_in_document)));
}

FloatPoint LocalFrameView::DocumentToFrame(
    const FloatPoint& point_in_document) const {
  return FloatPoint(DocumentToFrame(DoublePoint(point_in_document)));
}

PhysicalOffset LocalFrameView::DocumentToFrame(
    const PhysicalOffset& offset_in_document) const {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return offset_in_document;

  return offset_in_document -
         PhysicalOffset::FromFloatSizeRound(layout_viewport->GetScrollOffset());
}

PhysicalRect LocalFrameView::DocumentToFrame(
    const PhysicalRect& rect_in_document) const {
  return PhysicalRect(DocumentToFrame(rect_in_document.offset),
                      rect_in_document.size);
}

IntPoint LocalFrameView::FrameToDocument(const IntPoint& point_in_frame) const {
  return FlooredIntPoint(FrameToDocument(PhysicalOffset(point_in_frame)));
}

PhysicalOffset LocalFrameView::FrameToDocument(
    const PhysicalOffset& offset_in_frame) const {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return offset_in_frame;

  return offset_in_frame +
         PhysicalOffset::FromFloatSizeRound(layout_viewport->GetScrollOffset());
}

IntRect LocalFrameView::FrameToDocument(const IntRect& rect_in_frame) const {
  return IntRect(FrameToDocument(rect_in_frame.Location()),
                 rect_in_frame.Size());
}

PhysicalRect LocalFrameView::FrameToDocument(
    const PhysicalRect& rect_in_frame) const {
  return PhysicalRect(FrameToDocument(rect_in_frame.offset),
                      rect_in_frame.size);
}

IntRect LocalFrameView::ConvertToContainingEmbeddedContentView(
    const IntRect& local_rect) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    auto* layout_object = GetLayoutEmbeddedContent();
    if (!layout_object)
      return local_rect;

    IntRect rect(local_rect);
    // Add borders and padding
    rect.Move(
        (layout_object->BorderLeft() + layout_object->PaddingLeft()).ToInt(),
        (layout_object->BorderTop() + layout_object->PaddingTop()).ToInt());
    return parent->ConvertFromLayoutObject(*layout_object, rect);
  }

  return local_rect;
}

IntRect LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const IntRect& parent_rect) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    IntRect local_rect = parent_rect;
    local_rect.SetLocation(
        parent->ConvertSelfToChild(*this, local_rect.Location()));
    return local_rect;
  }

  return parent_rect;
}

PhysicalOffset LocalFrameView::ConvertToContainingEmbeddedContentView(
    const PhysicalOffset& local_offset) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    auto* layout_object = GetLayoutEmbeddedContent();
    if (!layout_object)
      return local_offset;

    PhysicalOffset point(local_offset);

    // Add borders and padding
    point += PhysicalOffset(
        layout_object->BorderLeft() + layout_object->PaddingLeft(),
        layout_object->BorderTop() + layout_object->PaddingTop());
    return parent->ConvertFromLayoutObject(*layout_object, point);
  }

  return local_offset;
}

FloatPoint LocalFrameView::ConvertToContainingEmbeddedContentView(
    const FloatPoint& local_point) const {
  if (ParentFrameView()) {
    auto* layout_object = GetLayoutEmbeddedContent();
    if (!layout_object)
      return local_point;

    PhysicalOffset point = PhysicalOffset::FromFloatPointRound(local_point);

    // Add borders and padding
    point.left += layout_object->BorderLeft() + layout_object->PaddingLeft();
    point.top += layout_object->BorderTop() + layout_object->PaddingTop();
    return FloatPoint(layout_object->LocalToAbsolutePoint(point));
  }

  return local_point;
}

PhysicalOffset LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const PhysicalOffset& parent_offset) const {
  return PhysicalOffset::FromFloatPointRound(
      ConvertFromContainingEmbeddedContentView(FloatPoint(parent_offset)));
}

FloatPoint LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const FloatPoint& parent_point) const {
  return FloatPoint(
      ConvertFromContainingEmbeddedContentView(DoublePoint(parent_point)));
}

DoublePoint LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const DoublePoint& parent_point) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    // Get our layoutObject in the parent view
    auto* layout_object = GetLayoutEmbeddedContent();
    if (!layout_object)
      return parent_point;

    DoublePoint point = DoublePoint(parent->ConvertToLayoutObject(
        *layout_object, FloatPoint(parent_point)));
    // Subtract borders and padding
    point.Move(
        (-layout_object->BorderLeft() - layout_object->PaddingLeft())
            .ToDouble(),
        (-layout_object->BorderTop() - layout_object->PaddingTop()).ToDouble());
    return point;
  }

  return parent_point;
}

IntPoint LocalFrameView::ConvertToContainingEmbeddedContentView(
    const IntPoint& local_point) const {
  return RoundedIntPoint(
      ConvertToContainingEmbeddedContentView(PhysicalOffset(local_point)));
}

void LocalFrameView::SetInitialTracksPaintInvalidationsForTesting(
    bool track_paint_invalidations) {
  g_initial_track_all_paint_invalidations = track_paint_invalidations;
}

void LocalFrameView::SetTracksPaintInvalidations(
    bool track_paint_invalidations) {
  if (track_paint_invalidations == IsTrackingPaintInvalidations())
    return;

  // Ensure the document is up-to-date before tracking invalidations.
  UpdateAllLifecyclePhases(DocumentLifecycle::LifecycleUpdateReason::kTest);

  for (Frame* frame = &frame_->Tree().Top(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    if (auto* layout_view = local_frame->ContentLayoutObject()) {
      layout_view->GetFrameView()->tracked_object_paint_invalidations_ =
          base::WrapUnique(track_paint_invalidations
                               ? new Vector<ObjectPaintInvalidation>
                               : nullptr);
      if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
        if (paint_artifact_compositor_) {
          paint_artifact_compositor_->SetTracksRasterInvalidations(
              track_paint_invalidations);
        }
      } else {
        layout_view->Compositor()->UpdateTrackingRasterInvalidations();
      }
    }
  }

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
                       "LocalFrameView::setTracksPaintInvalidations",
                       TRACE_EVENT_SCOPE_GLOBAL, "enabled",
                       track_paint_invalidations);
}

void LocalFrameView::TrackObjectPaintInvalidation(
    const DisplayItemClient& client,
    PaintInvalidationReason reason) {
  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
                       "InvalidateDisplayItemClient", TRACE_EVENT_SCOPE_GLOBAL,
                       "client", client.DebugName().Utf8(), "reason",
                       PaintInvalidationReasonToString(reason));
  if (!tracked_object_paint_invalidations_)
    return;

  ObjectPaintInvalidation invalidation = {client.DebugName(), reason};
  tracked_object_paint_invalidations_->push_back(invalidation);
}

void LocalFrameView::AddResizerArea(LayoutBox& resizer_box) {
  DCHECK(!RuntimeEnabledFeatures::PaintNonFastScrollableRegionsEnabled());
  if (!resizer_areas_)
    resizer_areas_ = std::make_unique<ResizerAreaSet>();
  resizer_areas_->insert(&resizer_box);
}

void LocalFrameView::RemoveResizerArea(LayoutBox& resizer_box) {
  DCHECK(!RuntimeEnabledFeatures::PaintNonFastScrollableRegionsEnabled());
  if (!resizer_areas_)
    return;

  ResizerAreaSet::iterator it = resizer_areas_->find(&resizer_box);
  if (it != resizer_areas_->end())
    resizer_areas_->erase(it);
}

void LocalFrameView::ScheduleAnimation() {
  if (auto* client = GetChromeClient())
    client->ScheduleAnimation(this);
}

bool LocalFrameView::FrameIsScrollableDidChange() {
  DCHECK(GetFrame().IsLocalRoot());
  return GetScrollingContext()->WasScrollable() !=
         LayoutViewport()->ScrollsOverflow();
}

void LocalFrameView::ClearFrameIsScrollableDidChange() {
  GetScrollingContext()->SetWasScrollable(
      GetFrame().LocalFrameRoot().View()->LayoutViewport()->ScrollsOverflow());
}

void LocalFrameView::ScrollableAreasDidChange() {
  // Layout may update scrollable area bounding boxes. It also sets the same
  // dirty flag making this one redundant (See
  // |ScrollingCoordinator::notifyGeometryChanged|).
  // So if layout is expected, ignore this call allowing scrolling coordinator
  // to be notified post-layout to recompute gesture regions.
  // TODO(wjmaclean): It would be nice to move the !NeedsLayout() check from
  // here to SetScrollGestureRegionIsDirty(), but at present doing so breaks
  // web tests. This suggests that there is something that wants to set the
  // dirty bit when layout is needed, and won't re-try setting the bit after
  // layout has completed - it would be nice to find that and fix it.
  if (!NeedsLayout())
    GetScrollingContext()->SetScrollGestureRegionIsDirty(true);
}

void LocalFrameView::AddScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  DCHECK(scrollable_area);
  if (!scrollable_areas_)
    scrollable_areas_ = MakeGarbageCollected<ScrollableAreaSet>();
  scrollable_areas_->insert(scrollable_area);

  if (GetScrollingCoordinator())
    ScrollableAreasDidChange();
}

void LocalFrameView::RemoveScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  if (!scrollable_areas_)
    return;
  scrollable_areas_->erase(scrollable_area);

  if (GetScrollingCoordinator())
    ScrollableAreasDidChange();
}

void LocalFrameView::AddAnimatingScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  DCHECK(scrollable_area);
  if (!animating_scrollable_areas_)
    animating_scrollable_areas_ = MakeGarbageCollected<ScrollableAreaSet>();
  animating_scrollable_areas_->insert(scrollable_area);
}

void LocalFrameView::RemoveAnimatingScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  if (!animating_scrollable_areas_)
    return;
  animating_scrollable_areas_->erase(scrollable_area);
}

void LocalFrameView::AttachToLayout() {
  CHECK(!IsAttached());
  if (frame_->GetDocument())
    CHECK_NE(Lifecycle().GetState(), DocumentLifecycle::kStopping);
  SetAttached(true);
  LocalFrameView* parent_view = ParentFrameView();
  CHECK(parent_view);
  if (parent_view->IsVisible())
    SetParentVisible(true);
  UpdateRenderThrottlingStatus(IsHiddenForThrottling(),
                               parent_view->CanThrottleRendering());

  // We may have updated paint properties in detached frame subtree for
  // printing (see UpdateLifecyclePhasesForPrinting()). The paint properties
  // may change after the frame is attached.
  if (auto* layout_view = GetLayoutView()) {
    layout_view->AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kPrinting);
  }
}

void LocalFrameView::DetachFromLayout() {
  CHECK(IsAttached());
  SetParentVisible(false);
  SetAttached(false);

  // We may need update paint properties in detached frame subtree for printing.
  // See UpdateLifecyclePhasesForPrinting().
  if (auto* layout_view = GetLayoutView()) {
    layout_view->AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kPrinting);
  }
}

void LocalFrameView::AddPlugin(WebPluginContainerImpl* plugin) {
  DCHECK(!plugins_.Contains(plugin));
  plugins_.insert(plugin);
}

void LocalFrameView::RemovePlugin(WebPluginContainerImpl* plugin) {
  DCHECK(plugins_.Contains(plugin));
  plugins_.erase(plugin);
}

void LocalFrameView::RemoveScrollbar(Scrollbar* scrollbar) {
  DCHECK(scrollbars_.Contains(scrollbar));
  scrollbars_.erase(scrollbar);
}

void LocalFrameView::AddScrollbar(Scrollbar* scrollbar) {
  DCHECK(!scrollbars_.Contains(scrollbar));
  scrollbars_.insert(scrollbar);
}

bool LocalFrameView::VisualViewportSuppliesScrollbars() {
  // On desktop, we always use the layout viewport's scrollbars.
  if (!frame_->GetSettings() || !frame_->GetSettings()->GetViewportEnabled() ||
      !frame_->GetDocument() || !frame_->GetPage())
    return false;

  if (!LayoutViewport())
    return false;

  const TopDocumentRootScrollerController& controller =
      frame_->GetPage()->GlobalRootScrollerController();
  return controller.RootScrollerArea() == LayoutViewport();
}

AXObjectCache* LocalFrameView::ExistingAXObjectCache() const {
  if (GetFrame().GetDocument())
    return GetFrame().GetDocument()->ExistingAXObjectCache();
  return nullptr;
}

void LocalFrameView::SetCursor(const Cursor& cursor) {
  Page* page = GetFrame().GetPage();
  if (!page || frame_->GetEventHandler().IsMousePositionUnknown())
    return;
  LogCursorSizeCounter(&GetFrame(), cursor);
  page->GetChromeClient().SetCursor(cursor, frame_);
}

void LocalFrameView::PropagateFrameRects() {
  TRACE_EVENT0("blink", "LocalFrameView::PropagateFrameRects");
  if (LayoutSizeFixedToFrameSize())
    SetLayoutSizeInternal(Size());

  ForAllChildViewsAndPlugins([](EmbeddedContentView& view) {
    auto* local_frame_view = DynamicTo<LocalFrameView>(view);
    if (!local_frame_view || !local_frame_view->ShouldThrottleRendering()) {
      view.PropagateFrameRects();
    }
  });

  GetFrame().Client()->FrameRectsChanged(FrameRect());

  // It's possible for changing the frame rect to not generate a layout
  // or any other event tracked by accessibility, we've seen this with
  // Android WebView. Ensure that the root of the accessibility tree is
  // invalidated so that it gets the right bounding rect.
  if (AXObjectCache* cache = ExistingAXObjectCache())
    cache->HandleFrameRectsChanged(*GetFrame().GetDocument());
}

void LocalFrameView::SetLayoutSizeInternal(const IntSize& size) {
  if (layout_size_ == size)
    return;
  layout_size_ = size;

  if (frame_->IsMainFrame() && frame_->GetDocument())
    TextAutosizer::UpdatePageInfoInAllFrames(frame_);

  SetNeedsLayout();
}

void LocalFrameView::ClipPaintRect(FloatRect* paint_rect) const {
  // TODO(wangxianzhu): Support ChromeClient::VisibleContentRectForPainting()
  // with CompositeAfterPaint.
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  // Paint the whole rect if "MainFrameClipsContent" is false, meaning that
  // WebPreferences::record_whole_document is true.
  if (!frame_->GetSettings()->GetMainFrameClipsContent())
    return;

  // By default we consider the bounds of the FrameView to be what is considered
  // visible for the Frame.
  IntRect visible_rect = IntRect(IntPoint(), Size());
  // Non-main frames always clip to their FrameView bounds. Main frames can
  // have this behaviour modified by devtools.
  if (frame_->IsMainFrame()) {
    // If devtools is overriding the viewport, then the FrameView's bounds are
    // not what we should paint, instead we should paint inside the bounds
    // specified by devtools.
    GetPage()->GetChromeClient().OverrideVisibleRectForMainFrame(*frame_,
                                                                 &visible_rect);
  }
  paint_rect->Intersect(visible_rect);
}

void LocalFrameView::DidChangeScrollOffset() {
  GetFrame().Client()->DidChangeScrollOffset();
  if (GetFrame().IsMainFrame()) {
    GetFrame().GetPage()->GetChromeClient().MainFrameScrollOffsetChanged(
        GetFrame());
  }
}

ScrollableArea* LocalFrameView::ScrollableAreaWithElementId(
    const CompositorElementId& id) {
  // Check for the layout viewport, which may not be in scrollable_areas_ if it
  // is styled overflow: hidden.  (Other overflow: hidden elements won't have
  // composited scrolling layers per crbug.com/784053, so we don't have to worry
  // about them.)
  ScrollableArea* viewport = LayoutViewport();
  if (id == viewport->GetCompositorElementId())
    return viewport;

  if (scrollable_areas_) {
    // This requires iterating over all scrollable areas. We may want to store a
    // map of ElementId to ScrollableArea if this is an issue for performance.
    for (ScrollableArea* scrollable_area : *scrollable_areas_) {
      if (id == scrollable_area->GetCompositorElementId())
        return scrollable_area;
    }
  }
  return nullptr;
}

void LocalFrameView::ScrollRectToVisibleInRemoteParent(
    const PhysicalRect& rect_to_scroll,
    const WebScrollIntoViewParams& params) {
  DCHECK(GetFrame().IsLocalRoot() && !GetFrame().IsMainFrame() &&
         safe_to_propagate_scroll_to_parent_);
  PhysicalRect new_rect = ConvertToRootFrame(rect_to_scroll);
  GetFrame().Client()->ScrollRectToVisibleInParentFrame(
      WebRect(new_rect.X().ToInt(), new_rect.Y().ToInt(),
              new_rect.Width().ToInt(), new_rect.Height().ToInt()),
      params);
}

void LocalFrameView::NotifyFrameRectsChangedIfNeeded() {
  if (root_layer_did_scroll_) {
    root_layer_did_scroll_ = false;
    PropagateFrameRects();
  }
}

PhysicalOffset LocalFrameView::ViewportToFrame(
    const PhysicalOffset& point_in_viewport) const {
  PhysicalOffset point_in_root_frame = PhysicalOffset::FromFloatPointRound(
      frame_->GetPage()->GetVisualViewport().ViewportToRootFrame(
          FloatPoint(point_in_viewport)));
  return ConvertFromRootFrame(point_in_root_frame);
}

FloatPoint LocalFrameView::ViewportToFrame(
    const FloatPoint& point_in_viewport) const {
  FloatPoint point_in_root_frame(
      frame_->GetPage()->GetVisualViewport().ViewportToRootFrame(
          point_in_viewport));
  return ConvertFromRootFrame(point_in_root_frame);
}

IntRect LocalFrameView::ViewportToFrame(const IntRect& rect_in_viewport) const {
  IntRect rect_in_root_frame =
      frame_->GetPage()->GetVisualViewport().ViewportToRootFrame(
          rect_in_viewport);
  return ConvertFromRootFrame(rect_in_root_frame);
}

IntPoint LocalFrameView::ViewportToFrame(
    const IntPoint& point_in_viewport) const {
  return RoundedIntPoint(ViewportToFrame(PhysicalOffset(point_in_viewport)));
}

IntRect LocalFrameView::FrameToViewport(const IntRect& rect_in_frame) const {
  IntRect rect_in_root_frame = ConvertToRootFrame(rect_in_frame);
  return frame_->GetPage()->GetVisualViewport().RootFrameToViewport(
      rect_in_root_frame);
}

IntPoint LocalFrameView::FrameToViewport(const IntPoint& point_in_frame) const {
  IntPoint point_in_root_frame = ConvertToRootFrame(point_in_frame);
  return frame_->GetPage()->GetVisualViewport().RootFrameToViewport(
      point_in_root_frame);
}

IntRect LocalFrameView::FrameToScreen(const IntRect& rect) const {
  if (auto* client = GetChromeClient())
    return client->ViewportToScreen(FrameToViewport(rect), this);
  return IntRect();
}

IntPoint LocalFrameView::SoonToBeRemovedUnscaledViewportToContents(
    const IntPoint& point_in_viewport) const {
  IntPoint point_in_root_frame = FlooredIntPoint(
      frame_->GetPage()->GetVisualViewport().ViewportCSSPixelsToRootFrame(
          FloatPoint(point_in_viewport)));
  return ConvertFromRootFrame(point_in_root_frame);
}

void LocalFrameView::Paint(GraphicsContext& context,
                           const GlobalPaintFlags global_paint_flags,
                           const CullRect& cull_rect,
                           const IntSize& paint_offset) const {
  // |paint_offset| is not used because paint properties of the contents will
  // ensure the correct location.
  PaintInternal(context, global_paint_flags, cull_rect);
}

void LocalFrameView::PaintInternal(GraphicsContext& context,
                                   const GlobalPaintFlags global_paint_flags,
                                   const CullRect& cull_rect) const {
  FramePainter(*this).Paint(context, global_paint_flags, cull_rect);
}

static bool PaintOutsideOfLifecycleIsAllowed(GraphicsContext& context,
                                             const LocalFrameView& frame_view) {
  // A paint outside of lifecycle should not conflict about paint controller
  // caching with the default painting executed during lifecycle update,
  // otherwise the caller should either use a transient paint controller or
  // explicitly skip cache.
  if (context.GetPaintController().IsSkippingCache())
    return true;
  // For CompositeAfterPaint, they always conflict because we always paint into
  // paint_controller_ during lifecycle update.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return false;
  // For pre-CompositeAfterPaint, they conflict if the local frame root has a
  // a root graphics layer.
  return !frame_view.GetFrame()
              .LocalFrameRoot()
              .View()
              ->GetLayoutView()
              ->Compositor()
              ->PaintRootGraphicsLayer();
}

void LocalFrameView::PaintOutsideOfLifecycle(
    GraphicsContext& context,
    const GlobalPaintFlags global_paint_flags,
    const CullRect& cull_rect) {
  DCHECK(PaintOutsideOfLifecycleIsAllowed(context, *this));

  base::AutoReset<bool> past_layout_lifecycle_resetter(
      &past_layout_lifecycle_update_, true);

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
  });

  PaintInternal(context, global_paint_flags, cull_rect);

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
  });
}

void LocalFrameView::PaintContentsOutsideOfLifecycle(
    GraphicsContext& context,
    const GlobalPaintFlags global_paint_flags,
    const CullRect& cull_rect) {
  DCHECK(PaintOutsideOfLifecycleIsAllowed(context, *this));

  base::AutoReset<bool> past_layout_lifecycle_resetter(
      &past_layout_lifecycle_update_, true);

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
  });

  FramePainter(*this).PaintContents(context, global_paint_flags, cull_rect);

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
  });
}

sk_sp<PaintRecord> LocalFrameView::GetPaintRecord() const {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK_EQ(DocumentLifecycle::kPaintClean, Lifecycle().GetState());
  DCHECK(frame_->IsLocalRoot());
  DCHECK(paint_controller_);
  return paint_controller_->GetPaintArtifact().GetPaintRecord(
      PropertyTreeState::Root());
}

IntRect LocalFrameView::ConvertToRootFrame(const IntRect& local_rect) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    IntRect parent_rect = ConvertToContainingEmbeddedContentView(local_rect);
    return parent->ConvertToRootFrame(parent_rect);
  }
  return local_rect;
}

IntPoint LocalFrameView::ConvertToRootFrame(const IntPoint& local_point) const {
  return RoundedIntPoint(ConvertToRootFrame(PhysicalOffset(local_point)));
}

PhysicalOffset LocalFrameView::ConvertToRootFrame(
    const PhysicalOffset& local_offset) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    PhysicalOffset parent_offset =
        ConvertToContainingEmbeddedContentView(local_offset);
    return parent->ConvertToRootFrame(parent_offset);
  }
  return local_offset;
}

FloatPoint LocalFrameView::ConvertToRootFrame(
    const FloatPoint& local_point) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    FloatPoint parent_point =
        ConvertToContainingEmbeddedContentView(local_point);
    return parent->ConvertToRootFrame(parent_point);
  }
  return local_point;
}

PhysicalRect LocalFrameView::ConvertToRootFrame(
    const PhysicalRect& local_rect) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    PhysicalOffset parent_offset =
        ConvertToContainingEmbeddedContentView(local_rect.offset);
    PhysicalRect parent_rect(parent_offset, local_rect.size);
    return parent->ConvertToRootFrame(parent_rect);
  }
  return local_rect;
}

IntRect LocalFrameView::ConvertFromRootFrame(
    const IntRect& rect_in_root_frame) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    IntRect parent_rect = parent->ConvertFromRootFrame(rect_in_root_frame);
    return ConvertFromContainingEmbeddedContentView(parent_rect);
  }
  return rect_in_root_frame;
}

IntPoint LocalFrameView::ConvertFromRootFrame(
    const IntPoint& point_in_root_frame) const {
  return RoundedIntPoint(
      ConvertFromRootFrame(PhysicalOffset(point_in_root_frame)));
}

PhysicalOffset LocalFrameView::ConvertFromRootFrame(
    const PhysicalOffset& offset_in_root_frame) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    PhysicalOffset parent_point =
        parent->ConvertFromRootFrame(offset_in_root_frame);
    return ConvertFromContainingEmbeddedContentView(parent_point);
  }
  return offset_in_root_frame;
}

FloatPoint LocalFrameView::ConvertFromRootFrame(
    const FloatPoint& point_in_root_frame) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    FloatPoint parent_point = parent->ConvertFromRootFrame(point_in_root_frame);
    return ConvertFromContainingEmbeddedContentView(parent_point);
  }
  return point_in_root_frame;
}

void LocalFrameView::ParentVisibleChanged() {
  // As parent visibility changes, we may need to recomposite this frame view
  // and potentially child frame views.
  SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);

  if (!IsSelfVisible())
    return;

  bool visible = IsParentVisible();
  ForAllChildViewsAndPlugins(
      [visible](EmbeddedContentView& embedded_content_view) {
        embedded_content_view.SetParentVisible(visible);
      });
}

void LocalFrameView::SelfVisibleChanged() {
  // FrameView visibility affects PLC::CanBeComposited, which in turn affects
  // compositing inputs.
  if (LayoutView* view = GetLayoutView())
    view->Layer()->SetNeedsCompositingInputsUpdate();
}

void LocalFrameView::Show() {
  if (!IsSelfVisible()) {
    SetSelfVisible(true);
    if (GetScrollingCoordinator())
      GetScrollingContext()->SetScrollGestureRegionIsDirty(true);
    SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
    if (IsParentVisible()) {
      ForAllChildViewsAndPlugins(
          [](EmbeddedContentView& embedded_content_view) {
            embedded_content_view.SetParentVisible(true);
          });
    }
  }
}

void LocalFrameView::Hide() {
  if (IsSelfVisible()) {
    if (IsParentVisible()) {
      ForAllChildViewsAndPlugins(
          [](EmbeddedContentView& embedded_content_view) {
            embedded_content_view.SetParentVisible(false);
          });
    }
    SetSelfVisible(false);
    if (GetScrollingCoordinator())
      GetScrollingContext()->SetScrollGestureRegionIsDirty(true);
    SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
  }
}

int LocalFrameView::ViewportWidth() const {
  int viewport_width = GetLayoutSize().Width();
  return AdjustForAbsoluteZoom::AdjustInt(viewport_width, GetLayoutView());
}

ScrollableArea* LocalFrameView::GetScrollableArea() {
  if (viewport_scrollable_area_)
    return viewport_scrollable_area_.Get();

  return LayoutViewport();
}

PaintLayerScrollableArea* LocalFrameView::LayoutViewport() const {
  auto* layout_view = GetLayoutView();
  return layout_view ? layout_view->GetScrollableArea() : nullptr;
}

RootFrameViewport* LocalFrameView::GetRootFrameViewport() {
  return viewport_scrollable_area_.Get();
}

void LocalFrameView::CollectAnnotatedRegions(
    LayoutObject& layout_object,
    Vector<AnnotatedRegionValue>& regions) const {
  // LayoutTexts don't have their own style, they just use their parent's style,
  // so we don't want to include them.
  if (layout_object.IsText())
    return;

  layout_object.AddAnnotatedRegions(regions);
  for (LayoutObject* curr = layout_object.SlowFirstChild(); curr;
       curr = curr->NextSibling())
    CollectAnnotatedRegions(*curr, regions);
}

bool LocalFrameView::UpdateViewportIntersectionsForSubtree(
    unsigned parent_flags) {
  // TODO(dcheng): Since LocalFrameView tree updates are deferred, FrameViews
  // might still be in the LocalFrameView hierarchy even though the associated
  // Document is already detached. Investigate if this check and a similar check
  // in lifecycle updates are still needed when there are no more deferred
  // LocalFrameView updates: https://crbug.com/561683
  if (!GetFrame().GetDocument()->IsActive())
    return false;

  unsigned flags = GetIntersectionObservationFlags(parent_flags);
  bool needs_occlusion_tracking = false;

  if (!NeedsLayout()) {
    // Notify javascript IntersectionObservers
    if (IntersectionObserverController* controller =
            GetFrame().GetDocument()->GetIntersectionObserverController()) {
      needs_occlusion_tracking |= controller->ComputeIntersections(flags);
    }
    intersection_observation_state_ = kNotNeeded;
  }

  UpdateViewportIntersection(flags, needs_occlusion_tracking);

  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    needs_occlusion_tracking |=
        child->View()->UpdateViewportIntersectionsForSubtree(flags);
  }

  for (HTMLPortalElement* portal :
       DocumentPortals::From(*frame_->GetDocument()).GetPortals()) {
    if (portal->ContentFrame()) {
      needs_occlusion_tracking |=
          portal->ContentFrame()->View()->UpdateViewportIntersectionsForSubtree(
              flags);
    }
  }

  return needs_occlusion_tracking;
}

void LocalFrameView::DeliverSynchronousIntersectionObservations() {
  if (IntersectionObserverController* controller =
          GetFrame().GetDocument()->GetIntersectionObserverController()) {
    controller->DeliverNotifications(
        IntersectionObserver::kDeliverDuringPostLifecycleSteps);
  }
  ForAllChildLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.DeliverSynchronousIntersectionObservations();
  });
}

void LocalFrameView::CrossOriginStatusChanged() {
  // If any of these conditions hold, then a change in cross-origin status does
  // not affect throttling.
  if (lifecycle_updates_throttled_ ||
      IsSubtreeThrottled() || !IsHiddenForThrottling()) {
    return;
  }
  RenderThrottlingStatusChanged();
  // We need to invalidate unconditionally, so if it didn't happen during
  // RenderThrottlingStatusChanged, do it now.
  if (CanThrottleRendering())
    InvalidateForThrottlingChange();
  // Immediately propagate changes to children.
  UpdateRenderThrottlingStatus(IsHiddenForThrottling(), IsSubtreeThrottled(),
                               true);
}

void LocalFrameView::VisibilityForThrottlingChanged() {
  if (FrameScheduler* frame_scheduler = frame_->GetFrameScheduler()) {
    // TODO(szager): Per crbug.com/994443, maybe this should be:
    //   SetFrameVisible(IsHiddenForThrottling() || IsSubtreeThrottled());
    frame_scheduler->SetFrameVisible(!IsHiddenForThrottling());
  }
}

void LocalFrameView::RenderThrottlingStatusChanged() {
  TRACE_EVENT0("blink", "LocalFrameView::RenderThrottlingStatusChanged");
  DCHECK(!IsInPerformLayout());
  DCHECK(!frame_->GetDocument() || !frame_->GetDocument()->InStyleRecalc());

  // We may record more/less foreign layers under the frame.
  GraphicsLayersDidChange();

  if (!CanThrottleRendering())
    InvalidateForThrottlingChange();

  // If we have become unthrottled, this is essentially a no-op since we're
  // going to paint anyway. If we have become throttled, then this will force
  // one lifecycle update to clear the painted output.
  if (GetFrame().IsLocalRoot())
    need_paint_phase_after_throttling_ = true;

#if DCHECK_IS_ON()
  // Make sure we never have an unthrottled frame inside a throttled one.
  LocalFrameView* parent = ParentFrameView();
  while (parent) {
    DCHECK(CanThrottleRendering() || !parent->CanThrottleRendering());
    parent = parent->ParentFrameView();
  }
#endif
}

void LocalFrameView::InvalidateForThrottlingChange() {
  // ScrollingCoordinator needs to update according to the new throttling
  // status.
  if (ScrollingCoordinator* coordinator = this->GetScrollingCoordinator())
    coordinator->NotifyGeometryChanged(this);
  // Start ticking animation frames again if necessary.
  if (GetPage())
    GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());
  // Force a full repaint of this frame to ensure we are not left with a
  // partially painted version of this frame's contents if we skipped
  // painting them while the frame was throttled.
  LayoutView* layout_view = GetLayoutView();
  if (layout_view) {
    layout_view->InvalidatePaintForViewAndCompositedLayers();
    // Also need to update all paint properties that might be skipped while
    // the frame was throttled.
    layout_view->AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kPreviouslySkipped);
  }
}

void LocalFrameView::SetNeedsForcedCompositingUpdate() {
  needs_forced_compositing_update_ = true;
  if (LocalFrameView* parent = ParentFrameView())
    parent->SetNeedsForcedCompositingUpdate();
}

void LocalFrameView::SetIntersectionObservationState(
    IntersectionObservationState state) {
  if (intersection_observation_state_ >= state)
    return;
  intersection_observation_state_ = state;
}

void LocalFrameView::SetPaintArtifactCompositorNeedsUpdate() {
  LocalFrameView* root = GetFrame().LocalFrameRoot().View();
  if (root && root->paint_artifact_compositor_)
    root->paint_artifact_compositor_->SetNeedsUpdate();
}

void LocalFrameView::GraphicsLayersDidChange() {
  LocalFrameView* root = GetFrame().LocalFrameRoot().View();
  if (root) {
    // We will re-collect GraphicsLayers in PushPaintArtifactsToCompositor().
    root->paint_controller_ = nullptr;
    if (root->paint_artifact_compositor_)
      root->paint_artifact_compositor_->SetNeedsUpdate();
  }
}

PaintArtifactCompositor* LocalFrameView::GetPaintArtifactCompositor() const {
  LocalFrameView* root = GetFrame().LocalFrameRoot().View();
  return root ? root->paint_artifact_compositor_.get() : nullptr;
}

unsigned LocalFrameView::GetIntersectionObservationFlags(
    unsigned parent_flags) const {
  unsigned flags = 0;

  const LocalFrame& target_frame = GetFrame();
  const Frame& root_frame = target_frame.Tree().Top();
  if (&root_frame == &target_frame ||
      target_frame.GetSecurityContext()->GetSecurityOrigin()->CanAccess(
          root_frame.GetSecurityContext()->GetSecurityOrigin())) {
    flags |= IntersectionObservation::kReportImplicitRootBounds;
  }

  // Observers with explicit roots only need to be checked on the same frame,
  // since in this case target and root must be in the same document.
  if (intersection_observation_state_ != kNotNeeded) {
    flags |= (IntersectionObservation::kExplicitRootObserversNeedUpdate |
              IntersectionObservation::kImplicitRootObserversNeedUpdate);
  }

  // For observers with implicit roots, we need to check state on the whole
  // local frame tree, as passed down from the parent.
  flags |= (parent_flags &
            IntersectionObservation::kImplicitRootObserversNeedUpdate);

  // The kIgnoreDelay parameter is used to force computation in an OOPIF which
  // is hidden in the parent document, thus not running lifecycle updates. It
  // applies to the entire frame tree.
  flags |= (parent_flags & IntersectionObservation::kIgnoreDelay);

  return flags;
}

bool LocalFrameView::ShouldThrottleRendering() const {
  bool throttled_for_global_reasons = CanThrottleRendering() &&
                                      frame_->GetDocument() &&
                                      Lifecycle().ThrottlingAllowed();
  if (!throttled_for_global_reasons || needs_forced_compositing_update_ ||
      need_paint_phase_after_throttling_) {
    return false;
  }

  // Only lifecycle phases up to layout are needed to generate an
  // intersection observation.
  return intersection_observation_state_ != kRequired ||
         GetFrame().LocalFrameRoot().View()->past_layout_lifecycle_update_;
}

bool LocalFrameView::CanThrottleRendering() const {
  if (lifecycle_updates_throttled_)
    return true;
  if (IsSubtreeThrottled())
    return true;
  // We only throttle hidden cross-origin frames. This is to avoid a situation
  // where an ancestor frame directly depends on the pipeline timing of a
  // descendant and breaks as a result of throttling. The rationale is that
  // cross-origin frames must already communicate with asynchronous messages,
  // so they should be able to tolerate some delay in receiving replies from a
  // throttled peer.
  return IsHiddenForThrottling() && frame_->IsCrossOriginSubframe();
}

void LocalFrameView::UpdateRenderThrottlingStatus(bool hidden_for_throttling,
                                                  bool subtree_throttled,
                                                  bool recurse) {
  bool was_throttled = CanThrottleRendering();
  FrameView::UpdateRenderThrottlingStatus(hidden_for_throttling,
                                          subtree_throttled, recurse);
  if (was_throttled != CanThrottleRendering())
    RenderThrottlingStatusChanged();
}

void LocalFrameView::BeginLifecycleUpdates() {
  // Avoid pumping frames for the initially empty document.
  // TODO(schenney): This seems pointless because main frame updates do occur
  // for pages like about:blank, at least according to log messages.
  if (!GetFrame().Loader().StateMachine()->CommittedFirstRealDocumentLoad())
    return;
  lifecycle_updates_throttled_ = false;
  if (auto* owner = GetLayoutEmbeddedContent())
    owner->SetShouldCheckForPaintInvalidation();

  LayoutView* layout_view = GetLayoutView();
  bool layout_view_is_empty = layout_view && !layout_view->FirstChild();
  if (layout_view_is_empty && !DidFirstLayout() && !NeedsLayout()) {
    // Make sure a display:none iframe gets an initial layout pass.
    layout_view->SetNeedsLayout(layout_invalidation_reason::kAddedToLayout,
                                kMarkOnlyThis);
  }

  ScheduleAnimation();
  SetIntersectionObservationState(kRequired);

  // Non-main-frame lifecycle and commit deferral are controlled by their
  // main frame.
  if (!GetFrame().IsMainFrame())
    return;

  Document* document = GetFrame().GetDocument();
  ChromeClient& chrome_client = GetFrame().GetPage()->GetChromeClient();

  // Determine if we want to defer commits to the compositor once lifecycle
  // updates start. Doing so allows us to update the page lifecycle but not
  // present the results to screen until we see first contentful paint is
  // available or until a timer expires.
  // This is enabled only if kAvoidFlashBetweenNavigation is enabled, and
  // the document loading is regular HTML served over HTTP/HTTPs.
  // And only defer commits once. This method gets called multiple times,
  // and we do not want to defer a second time if we have already done
  // so once and resumed commits already.
  if (document &&
      base::FeatureList::IsEnabled(
          blink::features::kAvoidFlashBetweenNavigation) &&
      document->DeferredCompositorCommitIsAllowed() &&
      !have_deferred_commits_) {
    chrome_client.StartDeferringCommits(
        GetFrame(), GetCommitDelayForAvoidFlashBetweenNavigation());
    have_deferred_commits_ = true;
  }

  chrome_client.BeginLifecycleUpdates(GetFrame());
}

void LocalFrameView::SetInitialViewportSize(const IntSize& viewport_size) {
  if (viewport_size == initial_viewport_size_)
    return;

  initial_viewport_size_ = viewport_size;
  if (Document* document = frame_->GetDocument())
    document->GetStyleEngine().InitialViewportChanged();
}

int LocalFrameView::InitialViewportWidth() const {
  DCHECK(frame_->IsMainFrame());
  return initial_viewport_size_.Width();
}

int LocalFrameView::InitialViewportHeight() const {
  DCHECK(frame_->IsMainFrame());
  return initial_viewport_size_.Height();
}

bool LocalFrameView::HasVisibleSlowRepaintViewportConstrainedObjects() const {
  if (!ViewportConstrainedObjects())
    return false;
  for (const LayoutObject* layout_object : *ViewportConstrainedObjects()) {
    DCHECK(layout_object->IsBoxModelObject() && layout_object->HasLayer());
    DCHECK(layout_object->StyleRef().GetPosition() == EPosition::kFixed ||
           layout_object->StyleRef().GetPosition() == EPosition::kSticky);
    if (ToLayoutBoxModelObject(layout_object)->IsSlowRepaintConstrainedObject())
      return true;
  }
  return false;
}

MainThreadScrollingReasons LocalFrameView::MainThreadScrollingReasonsPerFrame()
    const {
  MainThreadScrollingReasons reasons =
      static_cast<MainThreadScrollingReasons>(0);

  if (ShouldThrottleRendering())
    return reasons;

  if (HasBackgroundAttachmentFixedObjects()) {
    reasons |=
        cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  }

  // Force main-thread scrolling if the frame has uncomposited position: fixed
  // elements.  Note: we care about this not only for input-scrollable frames
  // but also for overflow: hidden frames, because script can run composited
  // smooth-scroll animations.  For this reason, we use HasOverflow instead of
  // ScrollsOverflow (which is false for overflow: hidden).
  if (LayoutViewport()->HasOverflow() &&
      GetLayoutView()->StyleRef().VisibleToHitTesting() &&
      HasVisibleSlowRepaintViewportConstrainedObjects()) {
    reasons |=
        cc::MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects;
  }
  return reasons;
}

MainThreadScrollingReasons LocalFrameView::GetMainThreadScrollingReasons()
    const {
  MainThreadScrollingReasons reasons =
      static_cast<MainThreadScrollingReasons>(0);

  if (!GetPage()->GetSettings().GetThreadedScrollingEnabled())
    reasons |= cc::MainThreadScrollingReason::kThreadedScrollingDisabled;

  if (!GetPage()->MainFrame()->IsLocalFrame())
    return reasons;

  // TODO(alexmos,kenrb): For OOPIF, local roots that are different from
  // the main frame can't be used in the calculation, since they use
  // different compositors with unrelated state, which breaks some of the
  // calculations below.
  if (&frame_->LocalFrameRoot() != GetPage()->MainFrame())
    return reasons;

  // Walk the tree to the root. Use the gathered reasons to determine
  // whether the target frame should be scrolled on main thread regardless
  // other subframes on the same page.
  for (Frame* frame = frame_; frame; frame = frame->Tree().Parent()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    reasons |= local_frame->View()->MainThreadScrollingReasonsPerFrame();
  }

  DCHECK(
      !cc::MainThreadScrollingReason::HasNonCompositedScrollReasons(reasons));
  return reasons;
}

String LocalFrameView::MainThreadScrollingReasonsAsText() {
  MainThreadScrollingReasons reasons = 0;
  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean);
  const auto* properties = GetLayoutView()->FirstFragment().PaintProperties();
  if (properties && properties->Scroll())
    reasons = properties->Scroll()->GetMainThreadScrollingReasons();
  return String(cc::MainThreadScrollingReason::AsText(reasons).c_str());
}

bool LocalFrameView::MapToVisualRectInTopFrameSpace(PhysicalRect& rect) {
  // This is the top-level frame, so no mapping necessary.
  if (frame_->IsMainFrame())
    return true;

  PhysicalRect viewport_intersection_rect(
      GetFrame().RemoteViewportIntersection());
  return rect.InclusiveIntersect(viewport_intersection_rect);
}

void LocalFrameView::ApplyTransformForTopFrameSpace(
    TransformState& transform_state) {
  // This is the top-level frame, so no mapping necessary.
  if (frame_->IsMainFrame())
    return;

  PhysicalRect viewport_intersection_rect(
      GetFrame().RemoteViewportIntersection());
  transform_state.Move(-viewport_intersection_rect.offset);
}

LayoutUnit LocalFrameView::CaretWidth() const {
  return LayoutUnit(
      std::max<float>(1.0, GetChromeClient()->WindowToViewportScalar(1)));
}

LocalFrameUkmAggregator& LocalFrameView::EnsureUkmAggregator() {
  if (!ukm_aggregator_) {
    ukm_aggregator_ = base::MakeRefCounted<LocalFrameUkmAggregator>(
        frame_->GetDocument()->UkmSourceID(),
        frame_->GetDocument()->UkmRecorder());
  }
  return *ukm_aggregator_;
}

void LocalFrameView::RegisterForLifecycleNotifications(
    LifecycleNotificationObserver* observer) {
  lifecycle_observers_.insert(observer);
}

void LocalFrameView::UnregisterFromLifecycleNotifications(
    LifecycleNotificationObserver* observer) {
  lifecycle_observers_.erase(observer);
}

#if DCHECK_IS_ON()
LocalFrameView::DisallowLayoutInvalidationScope::
    DisallowLayoutInvalidationScope(LocalFrameView* view)
    : local_frame_view_(view) {
  local_frame_view_->allows_layout_invalidation_after_layout_clean_ = false;
  local_frame_view_->ForAllChildLocalFrameViews([](LocalFrameView& frame_view) {
    if (!frame_view.ShouldThrottleRendering())
      frame_view.CheckDoesNotNeedLayout();
    frame_view.allows_layout_invalidation_after_layout_clean_ = false;
  });
}

LocalFrameView::DisallowLayoutInvalidationScope::
    ~DisallowLayoutInvalidationScope() {
  local_frame_view_->allows_layout_invalidation_after_layout_clean_ = true;
  local_frame_view_->ForAllChildLocalFrameViews([](LocalFrameView& frame_view) {
    if (!frame_view.ShouldThrottleRendering())
      frame_view.CheckDoesNotNeedLayout();
    frame_view.allows_layout_invalidation_after_layout_clean_ = true;
  });
}

#endif

}  // namespace blink
