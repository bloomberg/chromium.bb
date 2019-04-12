// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/jank_tracker.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

static constexpr TimeDelta kTimerDelay = TimeDelta::FromMilliseconds(500);
static const float kRegionGranularitySteps = 60.0;
static const float kMovementThreshold = 3.0;  // CSS pixels.

static FloatPoint LogicalStart(const FloatRect& rect,
                               const LayoutObject& object) {
  const ComputedStyle* style = object.Style();
  DCHECK(style);
  auto logical =
      PhysicalToLogical<float>(style->GetWritingMode(), style->Direction(),
                               rect.Y(), rect.MaxX(), rect.MaxY(), rect.X());
  return FloatPoint(logical.InlineStart(), logical.BlockStart());
}

static float GetMoveDistance(const FloatRect& old_rect,
                             const FloatRect& new_rect,
                             const LayoutObject& object) {
  FloatSize location_delta =
      LogicalStart(new_rect, object) - LogicalStart(old_rect, object);
  return std::max(fabs(location_delta.Width()), fabs(location_delta.Height()));
}

static float RegionGranularityScale(const IntRect& viewport) {
  if (RuntimeEnabledFeatures::JankTrackingSweepLineEnabled())
    return 1;

  return kRegionGranularitySteps /
         std::min(viewport.Height(), viewport.Width());
}

static bool EqualWithinMovementThreshold(const FloatPoint& a,
                                         const FloatPoint& b,
                                         const LayoutObject& object) {
  float threshold_physical_px =
      kMovementThreshold * object.StyleRef().EffectiveZoom();
  return fabs(a.X() - b.X()) < threshold_physical_px &&
         fabs(a.Y() - b.Y()) < threshold_physical_px;
}

static bool SmallerThanRegionGranularity(const FloatRect& rect,
                                         float granularity_scale) {
  return rect.Width() * granularity_scale < 0.5 ||
         rect.Height() * granularity_scale < 0.5;
}

static const TransformPaintPropertyNode& TransformNodeFor(
    LayoutObject& object) {
  return object.FirstFragment().LocalBorderBoxProperties().Transform();
}

static void RegionToTracedValue(const Region& region,
                                double granularity_scale,
                                TracedValue& value) {
  value.BeginArray("region_rects");
  for (const IntRect& rect : region.Rects()) {
    value.BeginArray();
    value.PushInteger(clampTo<int>(roundf(rect.X() / granularity_scale)));
    value.PushInteger(clampTo<int>(roundf(rect.Y() / granularity_scale)));
    value.PushInteger(clampTo<int>(roundf(rect.Width() / granularity_scale)));
    value.PushInteger(clampTo<int>(roundf(rect.Height() / granularity_scale)));
    value.EndArray();
  }
  value.EndArray();
}

static void RegionToTracedValue(const JankRegion& region,
                                double granularity_scale,
                                TracedValue& value) {
  Region old_region;
  for (IntRect rect : region.GetRects())
    old_region.Unite(Region(rect));
  RegionToTracedValue(old_region, granularity_scale, value);
}

JankTracker::JankTracker(LocalFrameView* frame_view)
    : frame_view_(frame_view),
      score_(0.0),
      weighted_score_(0.0),
      timer_(frame_view->GetFrame().GetTaskRunner(TaskType::kInternalDefault),
             this,
             &JankTracker::TimerFired),
      max_distance_(0.0) {}

void JankTracker::AccumulateJank(const LayoutObject& source,
                                 const PaintLayer& painting_layer,
                                 FloatRect old_rect,
                                 FloatRect new_rect) {
  if (old_rect.IsEmpty() || new_rect.IsEmpty())
    return;

  if (EqualWithinMovementThreshold(LogicalStart(old_rect, source),
                                   LogicalStart(new_rect, source), source))
    return;

  IntRect viewport =
      IntRect(IntPoint(),
              frame_view_->GetScrollableArea()->VisibleContentRect().Size());
  float scale = RegionGranularityScale(viewport);

  if (SmallerThanRegionGranularity(old_rect, scale) &&
      SmallerThanRegionGranularity(new_rect, scale))
    return;

  // Ignore layout objects that move (in the coordinate space of the paint
  // invalidation container) on scroll.
  // TODO(skobes): Find a way to detect when these objects jank.
  if (source.IsFixedPositioned() || source.IsStickyPositioned())
    return;

  // SVG elements don't participate in the normal layout algorithms and are
  // more likely to be used for animations.
  if (source.IsSVG())
    return;

  const auto& local_xform = TransformNodeFor(painting_layer.GetLayoutObject());
  const auto& root_xform = TransformNodeFor(*source.View());

  GeometryMapper::SourceToDestinationRect(local_xform, root_xform, old_rect);
  GeometryMapper::SourceToDestinationRect(local_xform, root_xform, new_rect);

  if (!old_rect.Intersects(viewport) && !new_rect.Intersects(viewport))
    return;

  DVLOG(2) << source.DebugName() << " moved from " << old_rect.ToString()
           << " to " << new_rect.ToString();

  max_distance_ =
      std::max(max_distance_, GetMoveDistance(old_rect, new_rect, source));

  IntRect visible_old_rect = RoundedIntRect(old_rect);
  visible_old_rect.Intersect(viewport);

  IntRect visible_new_rect = RoundedIntRect(new_rect);
  visible_new_rect.Intersect(viewport);

  visible_old_rect.Scale(scale);
  visible_new_rect.Scale(scale);

  if (RuntimeEnabledFeatures::JankTrackingSweepLineEnabled()) {
    region_experimental_.AddRect(visible_old_rect);
    region_experimental_.AddRect(visible_new_rect);
  } else {
    region_.Unite(Region(visible_old_rect));
    region_.Unite(Region(visible_new_rect));
  }
}

void JankTracker::NotifyObjectPrePaint(const LayoutObject& object,
                                       const IntRect& old_visual_rect,
                                       const PaintLayer& painting_layer) {
  if (!IsActive())
    return;

  AccumulateJank(object, painting_layer, FloatRect(old_visual_rect),
                 FloatRect(object.FirstFragment().VisualRect()));
}

void JankTracker::NotifyCompositedLayerMoved(const PaintLayer& paint_layer,
                                             FloatRect old_layer_rect,
                                             FloatRect new_layer_rect) {
  if (!IsActive())
    return;

  // Make sure we can access a transform node.
  LayoutObject& layout_object = paint_layer.GetLayoutObject();
  if (!layout_object.FirstFragment().HasLocalBorderBoxProperties())
    return;

  // Convert to the local transform space, whose origin is the layer's previous
  // location because the property trees haven't been updated yet.
  FloatPoint transform_parent_offset = -old_layer_rect.Location();
  old_layer_rect.MoveBy(transform_parent_offset);
  new_layer_rect.MoveBy(transform_parent_offset);

  AccumulateJank(layout_object, paint_layer, old_layer_rect, new_layer_rect);
}

double JankTracker::SubframeWeightingFactor() const {
  LocalFrame& frame = frame_view_->GetFrame();
  if (frame.IsMainFrame())
    return 1;

  // Map the subframe view rect into the coordinate space of the local root.
  FloatClipRect subframe_cliprect =
      FloatClipRect(FloatRect(FloatPoint(), FloatSize(frame_view_->Size())));
  GeometryMapper::LocalToAncestorVisualRect(
      frame_view_->GetLayoutView()->FirstFragment().LocalBorderBoxProperties(),
      PropertyTreeState::Root(), subframe_cliprect);
  LayoutRect subframe_rect = LayoutRect(subframe_cliprect.Rect());

  // Intersect with the portion of the local root that overlaps the main frame.
  frame.LocalFrameRoot().View()->MapToVisualRectInTopFrameSpace(subframe_rect);
  IntSize subframe_visible_size = subframe_rect.PixelSnappedSize();

  // TODO(crbug.com/939050): This does not update on window resize.
  IntSize main_frame_size = frame.GetPage()->GetVisualViewport().Size();

  // TODO(crbug.com/940711): This comparison ignores page scale and CSS
  // transforms above the local root.
  return static_cast<double>(subframe_visible_size.Area()) /
         main_frame_size.Area();
}

void JankTracker::NotifyPrePaintFinished() {
  if (!IsActive())
    return;
  bool use_sweep_line = RuntimeEnabledFeatures::JankTrackingSweepLineEnabled();
  bool region_is_empty =
      use_sweep_line ? region_experimental_.IsEmpty() : region_.IsEmpty();
  if (region_is_empty)
    return;

  IntRect viewport = frame_view_->GetScrollableArea()->VisibleContentRect();
  double granularity_scale = RegionGranularityScale(viewport);
  viewport.Scale(granularity_scale);

  if (viewport.IsEmpty())
    return;

  double viewport_area = double(viewport.Width()) * double(viewport.Height());
  uint64_t region_area =
      use_sweep_line ? region_experimental_.Area() : region_.Area();
  double jank_fraction = region_area / viewport_area;
  DCHECK_GT(jank_fraction, 0);

  score_ += jank_fraction;

  DVLOG(1) << "viewport " << (jank_fraction * 100)
           << "% janked, raising score to " << score_;

  LocalFrame& frame = frame_view_->GetFrame();

  TRACE_EVENT_INSTANT2("loading", "FrameLayoutJank", TRACE_EVENT_SCOPE_THREAD,
                       "data",
                       PerFrameTraceData(jank_fraction, granularity_scale),
                       "frame", ToTraceValue(&frame));

  double weighted_jank_fraction = jank_fraction * SubframeWeightingFactor();
  if (weighted_jank_fraction > 0) {
    weighted_score_ += weighted_jank_fraction;
    frame.Client()->DidObserveLayoutJank(weighted_jank_fraction);
  }

  if (RuntimeEnabledFeatures::LayoutJankAPIEnabled(frame.GetDocument()) &&
      frame.DomWindow()) {
    WindowPerformance* performance =
        DOMWindowPerformance::performance(*frame.DomWindow());
    if (performance &&
        (performance->HasObserverFor(PerformanceEntry::kLayoutJank) ||
         performance->ShouldBufferEntries())) {
      performance->AddLayoutJankFraction(jank_fraction);
    }
  }

  if (use_sweep_line)
    region_experimental_.Reset();
  else
    region_ = Region();
}

void JankTracker::NotifyInput(const WebInputEvent& event) {
  bool event_is_meaningful =
      event.GetType() == WebInputEvent::kMouseDown ||
      event.GetType() == WebInputEvent::kKeyDown ||
      event.GetType() == WebInputEvent::kRawKeyDown ||
      // We need to explicitly include tap, as if there are no listeners, we
      // won't receive the pointer events.
      event.GetType() == WebInputEvent::kGestureTap ||
      // Ignore kPointerDown, since it might be a scroll.
      event.GetType() == WebInputEvent::kPointerUp;

  if (!event_is_meaningful)
    return;

  // This cancels any previously scheduled task from the same timer.
  timer_.StartOneShot(kTimerDelay, FROM_HERE);
}

bool JankTracker::IsActive() {
  // This eliminates noise from the private Page object created by
  // SVGImage::DataChanged.
  if (frame_view_->GetFrame().GetChromeClient().IsSVGImageChromeClient())
    return false;

  if (timer_.IsActive())
    return false;

  return true;
}

std::unique_ptr<TracedValue> JankTracker::PerFrameTraceData(
    double jank_fraction,
    double granularity_scale) const {
  auto value = std::make_unique<TracedValue>();
  value->SetDouble("jank_fraction", jank_fraction);
  value->SetDouble("cumulative_score", score_);
  value->SetDouble("max_distance", max_distance_);
  if (RuntimeEnabledFeatures::JankTrackingSweepLineEnabled())
    RegionToTracedValue(region_experimental_, granularity_scale, *value);
  else
    RegionToTracedValue(region_, granularity_scale, *value);
  value->SetBoolean("is_main_frame", frame_view_->GetFrame().IsMainFrame());
  return value;
}

}  // namespace blink
