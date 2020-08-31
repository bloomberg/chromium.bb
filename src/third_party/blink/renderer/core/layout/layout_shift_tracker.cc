// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"

#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

using ReattachHook = LayoutShiftTracker::ReattachHook;

namespace {

ReattachHook& GetReattachHook() {
  DEFINE_STATIC_LOCAL(Persistent<ReattachHook>, hook,
                      (MakeGarbageCollected<ReattachHook>()));
  return *hook;
}

constexpr base::TimeDelta kTimerDelay = base::TimeDelta::FromMilliseconds(500);
const float kMovementThreshold = 3.0;  // CSS pixels.

FloatPoint LogicalStart(const FloatRect& rect, const LayoutObject& object) {
  const ComputedStyle* style = object.Style();
  DCHECK(style);
  auto logical =
      PhysicalToLogical<float>(style->GetWritingMode(), style->Direction(),
                               rect.Y(), rect.MaxX(), rect.MaxY(), rect.X());
  return FloatPoint(logical.InlineStart(), logical.BlockStart());
}

float GetMoveDistance(const FloatRect& old_rect,
                      const FloatRect& new_rect,
                      const LayoutObject& object) {
  FloatSize location_delta =
      LogicalStart(new_rect, object) - LogicalStart(old_rect, object);
  return std::max(fabs(location_delta.Width()), fabs(location_delta.Height()));
}

bool EqualWithinMovementThreshold(const FloatPoint& a,
                                  const FloatPoint& b,
                                  const LayoutObject& object) {
  float threshold_physical_px =
      kMovementThreshold * object.StyleRef().EffectiveZoom();
  return fabs(a.X() - b.X()) < threshold_physical_px &&
         fabs(a.Y() - b.Y()) < threshold_physical_px;
}

bool SmallerThanRegionGranularity(const FloatRect& rect) {
  // The region uses integer coordinates, so the rects are snapped to
  // pixel boundaries. Ignore rects smaller than half a pixel.
  return rect.Width() < 0.5 || rect.Height() < 0.5;
}

const PropertyTreeState PropertyTreeStateFor(const LayoutObject& object) {
  return object.FirstFragment().LocalBorderBoxProperties();
}

void RectToTracedValue(const IntRect& rect,
                       TracedValue& value,
                       const char* key = nullptr) {
  if (key)
    value.BeginArray(key);
  else
    value.BeginArray();
  value.PushInteger(rect.X());
  value.PushInteger(rect.Y());
  value.PushInteger(rect.Width());
  value.PushInteger(rect.Height());
  value.EndArray();
}

void RegionToTracedValue(const LayoutShiftRegion& region, TracedValue& value) {
  Region blink_region;
  for (IntRect rect : region.GetRects())
    blink_region.Unite(Region(rect));

  value.BeginArray("region_rects");
  for (const IntRect& rect : blink_region.Rects())
    RectToTracedValue(rect, value);
  value.EndArray();
}

#if DCHECK_IS_ON()
bool ShouldLog(const LocalFrame& frame) {
  const String& url = frame.GetDocument()->Url().GetString();
  return !url.StartsWith("devtools:");
}
#endif

}  // namespace

LayoutShiftTracker::LayoutShiftTracker(LocalFrameView* frame_view)
    : frame_view_(frame_view),
      score_(0.0),
      weighted_score_(0.0),
      timer_(frame_view->GetFrame().GetTaskRunner(TaskType::kInternalDefault),
             this,
             &LayoutShiftTracker::TimerFired),
      frame_max_distance_(0.0),
      overall_max_distance_(0.0),
      observed_input_or_scroll_(false),
      most_recent_input_timestamp_initialized_(false) {}

void LayoutShiftTracker::ObjectShifted(
    const LayoutObject& source,
    const PropertyTreeState& property_tree_state,
    FloatRect old_rect,
    FloatRect new_rect,
    FloatSize paint_offset_delta) {
  if (old_rect.IsEmpty() || new_rect.IsEmpty())
    return;

  old_rect.Move(paint_offset_delta);

  if (EqualWithinMovementThreshold(LogicalStart(old_rect, source),
                                   LogicalStart(new_rect, source), source))
    return;

  if (SmallerThanRegionGranularity(old_rect) &&
      SmallerThanRegionGranularity(new_rect))
    return;

  // Ignore layout objects that move (in the coordinate space of the paint
  // invalidation container) on scroll.
  // TODO(skobes): Find a way to detect when these objects shift.
  if (source.IsFixedPositioned() || source.IsStickyPositioned())
    return;

  // SVG elements don't participate in the normal layout algorithms and are
  // more likely to be used for animations.
  if (source.IsSVG())
    return;

  const auto root_state = PropertyTreeStateFor(*source.View());

  FloatClipRect clip_rect =
      GeometryMapper::LocalToAncestorClipRect(property_tree_state, root_state);

  // If the clip region is empty, then the resulting layout shift isn't visible
  // in the viewport so ignore it.
  if (!clip_rect.IsInfinite() && clip_rect.Rect().IsEmpty())
    return;

  GeometryMapper::SourceToDestinationRect(property_tree_state.Transform(),
                                          root_state.Transform(), old_rect);
  GeometryMapper::SourceToDestinationRect(property_tree_state.Transform(),
                                          root_state.Transform(), new_rect);

  if (EqualWithinMovementThreshold(old_rect.Location(), new_rect.Location(),
                                   source)) {
    return;
  }

  if (EqualWithinMovementThreshold(old_rect.Location() + frame_scroll_delta_,
                                   new_rect.Location(), source)) {
    // TODO(skobes): Checking frame_scroll_delta_ is an imperfect solution to
    // allowing counterscrolled layout shifts. Ideally, we would map old_rect
    // to viewport coordinates using the previous frame's scroll tree.
    return;
  }

  FloatRect clipped_old_rect(old_rect), clipped_new_rect(new_rect);
  if (!clip_rect.IsInfinite()) {
    clipped_old_rect.Intersect(clip_rect.Rect());
    clipped_new_rect.Intersect(clip_rect.Rect());
  }

  IntRect viewport =
      IntRect(IntPoint(),
              frame_view_->GetScrollableArea()->VisibleContentRect().Size());

  IntRect visible_old_rect = RoundedIntRect(clipped_old_rect);
  visible_old_rect.Intersect(viewport);
  IntRect visible_new_rect = RoundedIntRect(clipped_new_rect);
  visible_new_rect.Intersect(viewport);

  if (visible_old_rect.IsEmpty() && visible_new_rect.IsEmpty())
    return;

  // Compute move distance based on unclipped rects, to accurately determine how
  // much the element moved.
  float move_distance = GetMoveDistance(old_rect, new_rect, source);
  frame_max_distance_ = std::max(frame_max_distance_, move_distance);

#if DCHECK_IS_ON()
  LocalFrame& frame = frame_view_->GetFrame();
  if (ShouldLog(frame)) {
    DVLOG(2) << "in " << (frame.IsMainFrame() ? "" : "subframe ")
             << frame.GetDocument()->Url().GetString() << ", "
             << source.DebugName() << " moved from " << old_rect.ToString()
             << " to " << new_rect.ToString() << " (visible from "
             << visible_old_rect.ToString() << " to "
             << visible_new_rect.ToString() << ")";
  }
#endif

  region_.AddRect(visible_old_rect);
  region_.AddRect(visible_new_rect);

  if (Node* node = source.GetNode()) {
    MaybeRecordAttribution(
        {DOMNodeIds::IdForNode(node), visible_old_rect, visible_new_rect});
  }
}

LayoutShiftTracker::Attribution::Attribution() : node_id(kInvalidDOMNodeId) {}
LayoutShiftTracker::Attribution::Attribution(DOMNodeId node_id_arg,
                                             IntRect old_visual_rect_arg,
                                             IntRect new_visual_rect_arg)
    : node_id(node_id_arg),
      old_visual_rect(old_visual_rect_arg),
      new_visual_rect(new_visual_rect_arg) {}

LayoutShiftTracker::Attribution::operator bool() const {
  return node_id != kInvalidDOMNodeId;
}

bool LayoutShiftTracker::Attribution::Encloses(const Attribution& other) const {
  return old_visual_rect.Contains(other.old_visual_rect) &&
         new_visual_rect.Contains(other.new_visual_rect);
}

int LayoutShiftTracker::Attribution::Area() const {
  int old_area = old_visual_rect.Width() * old_visual_rect.Height();
  int new_area = new_visual_rect.Width() * new_visual_rect.Height();

  IntRect intersection = Intersection(old_visual_rect, new_visual_rect);
  int shared_area = intersection.Width() * intersection.Height();
  return old_area + new_area - shared_area;
}

bool LayoutShiftTracker::Attribution::MoreImpactfulThan(
    const Attribution& other) const {
  return Area() > other.Area();
}

void LayoutShiftTracker::MaybeRecordAttribution(
    const Attribution& attribution) {
  Attribution* smallest = nullptr;
  for (auto& slot : attributions_) {
    if (!slot || attribution.Encloses(slot)) {
      slot = attribution;
      return;
    }
    if (slot.Encloses(attribution))
      return;
    if (!smallest || smallest->MoreImpactfulThan(slot))
      smallest = &slot;
  }
  // No empty slots or redundancies. Replace smallest existing slot if larger.
  if (attribution.MoreImpactfulThan(*smallest))
    *smallest = attribution;
}

void LayoutShiftTracker::NotifyObjectPrePaint(
    const LayoutObject& object,
    const PropertyTreeState& property_tree_state,
    const IntRect& old_visual_rect,
    const IntRect& new_visual_rect,
    FloatSize paint_offset_delta) {
  if (!IsActive())
    return;

  ObjectShifted(object, property_tree_state, FloatRect(old_visual_rect),
                FloatRect(new_visual_rect), paint_offset_delta);
}

double LayoutShiftTracker::SubframeWeightingFactor() const {
  LocalFrame& frame = frame_view_->GetFrame();
  if (frame.IsMainFrame())
    return 1;

  // Map the subframe view rect into the coordinate space of the local root.
  FloatClipRect subframe_cliprect =
      FloatClipRect(FloatRect(FloatPoint(), FloatSize(frame_view_->Size())));
  GeometryMapper::LocalToAncestorVisualRect(
      frame_view_->GetLayoutView()->FirstFragment().LocalBorderBoxProperties(),
      PropertyTreeState::Root(), subframe_cliprect);
  auto subframe_rect = PhysicalRect::EnclosingRect(subframe_cliprect.Rect());

  // Intersect with the portion of the local root that overlaps the main frame.
  frame.LocalFrameRoot().View()->MapToVisualRectInRemoteRootFrame(
      subframe_rect);
  IntSize subframe_visible_size = subframe_rect.PixelSnappedSize();
  IntSize main_frame_size = frame.GetPage()->GetVisualViewport().Size();

  // TODO(crbug.com/940711): This comparison ignores page scale and CSS
  // transforms above the local root.
  return static_cast<double>(subframe_visible_size.Area()) /
         main_frame_size.Area();
}

void LayoutShiftTracker::NotifyPrePaintFinished() {
  if (!IsActive())
    return;
  if (region_.IsEmpty())
    return;

  IntRect viewport = frame_view_->GetScrollableArea()->VisibleContentRect();
  if (viewport.IsEmpty())
    return;

  double viewport_area = double(viewport.Width()) * double(viewport.Height());
  double impact_fraction = region_.Area() / viewport_area;
  DCHECK_GT(impact_fraction, 0);

  DCHECK_GT(frame_max_distance_, 0.0);
  double viewport_max_dimension = std::max(viewport.Width(), viewport.Height());
  double move_distance_factor =
      (frame_max_distance_ < viewport_max_dimension)
          ? double(frame_max_distance_) / viewport_max_dimension
          : 1.0;
  double score_delta = impact_fraction * move_distance_factor;
  double weighted_score_delta = score_delta * SubframeWeightingFactor();

  overall_max_distance_ = std::max(overall_max_distance_, frame_max_distance_);

#if DCHECK_IS_ON()
  LocalFrame& frame = frame_view_->GetFrame();
  if (ShouldLog(frame)) {
    DVLOG(1) << "in " << (frame.IsMainFrame() ? "" : "subframe ")
             << frame.GetDocument()->Url().GetString() << ", viewport was "
             << (impact_fraction * 100) << "% impacted with distance fraction "
             << move_distance_factor;
  }
#endif

  if (pointerdown_pending_data_.saw_pointerdown) {
    pointerdown_pending_data_.score_delta += score_delta;
    pointerdown_pending_data_.weighted_score_delta += weighted_score_delta;
  } else {
    ReportShift(score_delta, weighted_score_delta);
  }

  if (!region_.IsEmpty())
    SetLayoutShiftRects(region_.GetRects());
  region_.Reset();

  frame_max_distance_ = 0.0;
  frame_scroll_delta_ = ScrollOffset();
  attributions_.fill(Attribution());
}

LayoutShift::AttributionList LayoutShiftTracker::CreateAttributionList() const {
  LayoutShift::AttributionList list;
  for (const Attribution& att : attributions_) {
    if (att.node_id == kInvalidDOMNodeId)
      break;
    list.push_back(LayoutShiftAttribution::Create(
        DOMNodeIds::NodeForId(att.node_id),
        DOMRectReadOnly::FromIntRect(att.old_visual_rect),
        DOMRectReadOnly::FromIntRect(att.new_visual_rect)));
  }
  return list;
}

void LayoutShiftTracker::SubmitPerformanceEntry(double score_delta,
                                                bool had_recent_input) const {
  LocalDOMWindow* window = frame_view_->GetFrame().DomWindow();
  if (!window)
    return;
  WindowPerformance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);

  double input_timestamp =
      had_recent_input ? performance->MonotonicTimeToDOMHighResTimeStamp(
                             most_recent_input_timestamp_)
                       : 0.0;
  LayoutShift* entry =
      LayoutShift::Create(performance->now(), score_delta, had_recent_input,
                          input_timestamp, CreateAttributionList());

  performance->AddLayoutShiftEntry(entry);
}

void LayoutShiftTracker::ReportShift(double score_delta,
                                     double weighted_score_delta) {
  LocalFrame& frame = frame_view_->GetFrame();
  bool had_recent_input = timer_.IsActive();

  if (!had_recent_input) {
    score_ += score_delta;
    if (weighted_score_delta > 0) {
      weighted_score_ += weighted_score_delta;
      frame.Client()->DidObserveLayoutShift(weighted_score_delta,
                                            observed_input_or_scroll_);
    }
  }

  SubmitPerformanceEntry(score_delta, had_recent_input);

  TRACE_EVENT_INSTANT2("loading", "LayoutShift", TRACE_EVENT_SCOPE_THREAD,
                       "data", PerFrameTraceData(score_delta, had_recent_input),
                       "frame", ToTraceValue(&frame));

#if DCHECK_IS_ON()
  if (ShouldLog(frame)) {
    DVLOG(1) << "in " << (frame.IsMainFrame() ? "" : "subframe ")
             << frame.GetDocument()->Url().GetString() << ", layout shift of "
             << score_delta
             << (had_recent_input ? " excluded by recent input" : " reported")
             << "; cumulative score is " << score_;
  }
#endif
}

void LayoutShiftTracker::NotifyInput(const WebInputEvent& event) {
  const WebInputEvent::Type type = event.GetType();
  const bool saw_pointerdown = pointerdown_pending_data_.saw_pointerdown;
  const bool pointerdown_became_tap =
      saw_pointerdown && type == WebInputEvent::Type::kPointerUp;
  const bool event_type_stops_pointerdown_buffering =
      type == WebInputEvent::Type::kPointerUp ||
      type == WebInputEvent::Type::kPointerCausedUaAction ||
      type == WebInputEvent::Type::kPointerCancel;

  // Only non-hovering pointerdown requires buffering.
  const bool is_hovering_pointerdown =
      type == WebInputEvent::Type::kPointerDown &&
      static_cast<const WebPointerEvent&>(event).hovering;

  const bool should_trigger_shift_exclusion =
      type == WebInputEvent::Type::kMouseDown ||
      type == WebInputEvent::Type::kKeyDown ||
      type == WebInputEvent::Type::kRawKeyDown ||
      // We need to explicitly include tap, as if there are no listeners, we
      // won't receive the pointer events.
      type == WebInputEvent::Type::kGestureTap || is_hovering_pointerdown ||
      pointerdown_became_tap;

  if (should_trigger_shift_exclusion) {
    observed_input_or_scroll_ = true;

    // This cancels any previously scheduled task from the same timer.
    timer_.StartOneShot(kTimerDelay, FROM_HERE);
    UpdateInputTimestamp(event.TimeStamp());
  }

  if (saw_pointerdown && event_type_stops_pointerdown_buffering) {
    double score_delta = pointerdown_pending_data_.score_delta;
    if (score_delta > 0)
      ReportShift(score_delta, pointerdown_pending_data_.weighted_score_delta);
    pointerdown_pending_data_ = PointerdownPendingData();
  }
  if (type == WebInputEvent::Type::kPointerDown && !is_hovering_pointerdown)
    pointerdown_pending_data_.saw_pointerdown = true;
}

void LayoutShiftTracker::UpdateInputTimestamp(base::TimeTicks timestamp) {
  if (!most_recent_input_timestamp_initialized_) {
    most_recent_input_timestamp_ = timestamp;
    most_recent_input_timestamp_initialized_ = true;
  } else if (timestamp > most_recent_input_timestamp_) {
    most_recent_input_timestamp_ = timestamp;
  }
}

void LayoutShiftTracker::NotifyScroll(mojom::blink::ScrollType scroll_type,
                                      ScrollOffset delta) {
  frame_scroll_delta_ += delta;

  // Only set observed_input_or_scroll_ for user-initiated scrolls, and not
  // other scrolls such as hash fragment navigations.
  if (scroll_type == mojom::blink::ScrollType::kUser ||
      scroll_type == mojom::blink::ScrollType::kCompositor)
    observed_input_or_scroll_ = true;
}

void LayoutShiftTracker::NotifyViewportSizeChanged() {
  // This cancels any previously scheduled task from the same timer.
  timer_.StartOneShot(kTimerDelay, FROM_HERE);
  UpdateInputTimestamp(base::TimeTicks::Now());
}

bool LayoutShiftTracker::IsActive() {
  // This eliminates noise from the private Page object created by
  // SVGImage::DataChanged.
  if (frame_view_->GetFrame().GetChromeClient().IsSVGImageChromeClient())
    return false;
  return true;
}

std::unique_ptr<TracedValue> LayoutShiftTracker::PerFrameTraceData(
    double score_delta,
    bool input_detected) const {
  auto value = std::make_unique<TracedValue>();
  value->SetDouble("score", score_delta);
  value->SetDouble("cumulative_score", score_);
  value->SetDouble("overall_max_distance", overall_max_distance_);
  value->SetDouble("frame_max_distance", frame_max_distance_);
  RegionToTracedValue(region_, *value);
  value->SetBoolean("is_main_frame", frame_view_->GetFrame().IsMainFrame());
  value->SetBoolean("had_recent_input", input_detected);
  AttributionsToTracedValue(*value);
  return value;
}

void LayoutShiftTracker::AttributionsToTracedValue(TracedValue& value) const {
  const Attribution* it = attributions_.begin();
  if (!*it)
    return;

  bool should_include_names;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("layout_shift.debug"), &should_include_names);

  value.BeginArray("impacted_nodes");
  while (it != attributions_.end() && it->node_id != kInvalidDOMNodeId) {
    value.BeginDictionary();
    value.SetInteger("node_id", it->node_id);
    RectToTracedValue(it->old_visual_rect, value, "old_rect");
    RectToTracedValue(it->new_visual_rect, value, "new_rect");
    if (should_include_names) {
      Node* node = DOMNodeIds::NodeForId(it->node_id);
      value.SetString("debug_name", node ? node->DebugName() : "");
    }
    value.EndDictionary();
    it++;
  }
  value.EndArray();
}

void LayoutShiftTracker::SetLayoutShiftRects(const Vector<IntRect>& int_rects) {
  // Store the layout shift rects in the HUD layer.
  auto* cc_layer = frame_view_->RootCcLayer();
  if (cc_layer && cc_layer->layer_tree_host()) {
    if (!cc_layer->layer_tree_host()->GetDebugState().show_layout_shift_regions)
      return;
    if (cc_layer->layer_tree_host()->hud_layer()) {
      WebVector<gfx::Rect> rects;
      Region blink_region;
      for (IntRect rect : int_rects)
        blink_region.Unite(Region(rect));
      for (const IntRect& rect : blink_region.Rects())
        rects.emplace_back(rect);
      cc_layer->layer_tree_host()->hud_layer()->SetLayoutShiftRects(
          rects.ReleaseVector());
      cc_layer->layer_tree_host()->hud_layer()->SetNeedsPushProperties();
    }
  }
}

void LayoutShiftTracker::Trace(Visitor* visitor) {
  visitor->Trace(frame_view_);
}

ReattachHook::Scope::Scope(const Node& node) : active_(node.GetLayoutObject()) {
  if (active_) {
    auto& hook = GetReattachHook();
    outer_ = hook.scope_;
    hook.scope_ = this;
  }
}

ReattachHook::Scope::~Scope() {
  if (active_) {
    auto& hook = GetReattachHook();
    hook.scope_ = outer_;
    if (!outer_)
      hook.visual_rects_.clear();
  }
}

void ReattachHook::NotifyDetach(const Node& node) {
  auto& hook = GetReattachHook();
  if (!hook.scope_)
    return;
  auto* layout_object = node.GetLayoutObject();
  if (!layout_object)
    return;
  auto& map = hook.visual_rects_;
  auto& fragment = layout_object->GetMutableForPainting().FirstFragment();

  // Save the visual rect for restoration on future reattachment.
  IntRect visual_rect = fragment.VisualRect();
  if (visual_rect.IsEmpty())
    return;
  map.Set(&node, visual_rect);
}

void ReattachHook::NotifyAttach(const Node& node) {
  auto& hook = GetReattachHook();
  if (!hook.scope_)
    return;
  auto* layout_object = node.GetLayoutObject();
  if (!layout_object)
    return;
  auto& map = hook.visual_rects_;
  auto& fragment = layout_object->GetMutableForPainting().FirstFragment();

  // Restore the visual rect that was saved during detach. Note: this does not
  // affect paint invalidation; we will fully invalidate the new layout object.
  auto iter = map.find(&node);
  if (iter == map.end())
    return;
  IntRect visual_rect = iter->value;
  fragment.SetVisualRect(visual_rect);
}

void ReattachHook::Trace(Visitor* visitor) {
  visitor->Trace(visual_rects_);
}

}  // namespace blink
