// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/ad_metrics/page_ad_density_tracker.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace page_load_metrics {

namespace {

int CalculateIntersectedLength(int start1, int end1, int start2, int end2) {
  DCHECK_LE(start1, end1);
  DCHECK_LE(start2, end2);

  return std::max(0, std::min(end1, end2) - std::max(start1, start2));
}

// Calculates the combined length of a set of line segments within boundaries.
// This counts each overlapping area a single time and does not include areas
// where there is no line segment.
//
// TODO(https://crbug.com/1068586): Optimize segment length calculation.
// AddSegment and RemoveSegment are both logarithmic operations, making this
// linearithmic with the number of segments. However the expected number
// of segments at any given time in the density calculation is low.
class BoundedSegmentLength {
 public:
  // An event to process corresponding to the left or right point of each
  // line segment.
  struct SegmentEvent {
    SegmentEvent(int segment_id, int pos, bool is_segment_start)
        : segment_id(segment_id),
          pos(pos),
          is_segment_start(is_segment_start) {}
    SegmentEvent(const SegmentEvent& other) = default;

    // Tiebreak with position with |is_segment_start| and |segment_id|.
    bool operator<(const SegmentEvent& rhs) const {
      if (pos == rhs.pos) {
        if (segment_id == rhs.segment_id) {
          return is_segment_start != rhs.is_segment_start;
        } else {
          return segment_id < rhs.segment_id;
        }
      } else {
        return pos < rhs.pos;
      }
    }

    int segment_id;
    int pos;
    bool is_segment_start;
  };

  // Iterators into the set of segment events for efficient removal of
  // segment events by segment_id. Maintained by |segment_event_iterators_|.
  struct SegmentEventSetIterators {
    SegmentEventSetIterators(std::set<SegmentEvent>::iterator start,
                             std::set<SegmentEvent>::iterator end)
        : start_it(start), end_it(end) {}

    SegmentEventSetIterators(const SegmentEventSetIterators& other) = default;

    std::set<SegmentEvent>::const_iterator start_it;
    std::set<SegmentEvent>::const_iterator end_it;
  };

  BoundedSegmentLength(int bound_start, int bound_end)
      : bound_start_(bound_start), bound_end_(bound_end) {
    DCHECK_LE(bound_start_, bound_end_);
  }

  BoundedSegmentLength(const BoundedSegmentLength&) = delete;
  BoundedSegmentLength& operator=(const BoundedSegmentLength&) = delete;

  ~BoundedSegmentLength() = default;

  // Add a line segment to the set of active line segments, the segment
  // corresponds to the bottom or top of a rect.
  void AddSegment(int segment_id, int start, int end) {
    DCHECK_LE(start, end);

    int clipped_start = std::max(bound_start_, start);
    int clipped_end = std::min(bound_end_, end);
    if (clipped_start >= clipped_end)
      return;

    // Safe as insert will never return an invalid iterator, it will point to
    // the existing element if already in the set.
    auto start_it = active_segments_
                        .insert(SegmentEvent(segment_id, clipped_start,
                                             true /*is_segment_start*/))
                        .first;
    auto end_it = active_segments_
                      .insert(SegmentEvent(segment_id, clipped_end,
                                           false /*is_segment_start*/))
                      .first;

    segment_event_iterators_.emplace(
        segment_id, SegmentEventSetIterators(start_it, end_it));
  }

  // Remove a segment from the set of active line segmnets.
  void RemoveSegment(int segment_id) {
    auto it = segment_event_iterators_.find(segment_id);
    if (it == segment_event_iterators_.end())
      return;

    const SegmentEventSetIterators& set_its = it->second;
    active_segments_.erase(set_its.start_it);
    active_segments_.erase(set_its.end_it);
    segment_event_iterators_.erase(segment_id);
  }

  // Calculate the combined length of segments in the active set of segments by
  // iterating over the sorted set of segment events.
  absl::optional<int> Length() {
    base::CheckedNumeric<int> length = 0;
    absl::optional<int> last_event_pos;
    int num_active = 0;
    for (const auto& segment_event : active_segments_) {
      if (!last_event_pos) {
        DCHECK(segment_event.is_segment_start);
        last_event_pos = segment_event.pos;
      }

      if (num_active > 0)
        length += segment_event.pos - last_event_pos.value();

      last_event_pos = segment_event.pos;
      if (segment_event.is_segment_start) {
        num_active += 1;
      } else {
        num_active -= 1;
      }
    }

    absl::optional<int> total_length;
    if (length.IsValid())
      total_length = length.ValueOrDie();

    return total_length;
  }

 private:
  int bound_start_;
  int bound_end_;

  std::set<SegmentEvent> active_segments_;

  // Map from the segment_id passed by user to the Segment struct.
  std::unordered_map<int, SegmentEventSetIterators> segment_event_iterators_;
};

}  // namespace

PageAdDensityTracker::RectEvent::RectEvent(int id,
                                           bool is_bottom,
                                           const gfx::Rect& rect)
    : rect_id(id), is_bottom(is_bottom), rect(rect) {}

PageAdDensityTracker::RectEvent::RectEvent(const RectEvent& other) = default;

PageAdDensityTracker::RectEventSetIterators::RectEventSetIterators(
    std::set<RectEvent>::iterator top,
    std::set<RectEvent>::iterator bottom)
    : top_it(top), bottom_it(bottom) {}

PageAdDensityTracker::RectEventSetIterators::RectEventSetIterators(
    const RectEventSetIterators& other) = default;

PageAdDensityTracker::PageAdDensityTracker() {
  start_time_ = base::TimeTicks::Now();
  last_viewport_density_recording_time_ = start_time_;
}

PageAdDensityTracker::~PageAdDensityTracker() = default;

int PageAdDensityTracker::MaxPageAdDensityByHeight() const {
  return max_page_ad_density_by_height_;
}

int PageAdDensityTracker::MaxPageAdDensityByArea() const {
  return max_page_ad_density_by_area_;
}

int PageAdDensityTracker::AverageViewportAdDensityByArea() const {
  base::TimeTicks now = base::TimeTicks::Now();

  base::TimeDelta total_elapsed_time = now - start_time_;
  if (total_elapsed_time == base::TimeDelta())
    return -1;

  base::TimeDelta last_elapsed_time =
      now - last_viewport_density_recording_time_;

  double total_viewport_ad_density_by_area =
      cumulative_viewport_ad_density_by_area_ +
      (last_viewport_ad_density_by_area_ * last_elapsed_time.InMicrosecondsF());

  return std::lround(total_viewport_ad_density_by_area /
                     total_elapsed_time.InMicrosecondsF());
}

int PageAdDensityTracker::ViewportAdDensityByArea() const {
  return last_viewport_ad_density_by_area_;
}

void PageAdDensityTracker::AddRect(int rect_id, const gfx::Rect& rect) {
  // Check that we do not already have rect events for the rect.
  DCHECK(rect_events_iterators_.find(rect_id) == rect_events_iterators_.end());

  // We do not track empty rects.
  if (rect.IsEmpty())
    return;

  // Limit the maximum number of rects tracked to 50 due to poor worst
  // case performance.
  const int kMaxRectsTracked = 50;
  if (rect_events_iterators_.size() > kMaxRectsTracked)
    return;

  auto top_it =
      rect_events_.insert(RectEvent(rect_id, true /*is_bottom*/, rect)).first;
  auto bottom_it =
      rect_events_.insert(RectEvent(rect_id, false /*is_bottom*/, rect)).first;
  rect_events_iterators_.emplace(rect_id,
                                 RectEventSetIterators(top_it, bottom_it));

  // TODO(https://crbug.com/1068586): Improve performance by adding additional
  // throttling to only calculate when max density can decrease (frame deleted
  // or moved).
  CalculatePageAdDensity();

  CalculateViewportAdDensity();
}

void PageAdDensityTracker::RemoveRect(int rect_id) {
  auto it = rect_events_iterators_.find(rect_id);

  if (it == rect_events_iterators_.end())
    return;

  const RectEventSetIterators& set_its = it->second;
  rect_events_.erase(set_its.top_it);
  rect_events_.erase(set_its.bottom_it);
  rect_events_iterators_.erase(rect_id);
}

void PageAdDensityTracker::UpdateMainFrameRect(const gfx::Rect& rect) {
  if (rect == last_main_frame_rect_)
    return;

  last_main_frame_rect_ = rect;
  CalculatePageAdDensity();
}

void PageAdDensityTracker::UpdateMainFrameViewportRect(const gfx::Rect& rect) {
  if (rect == last_main_frame_viewport_rect_)
    return;

  last_main_frame_viewport_rect_ = rect;
  CalculateViewportAdDensity();
}

void PageAdDensityTracker::CalculatePageAdDensity() {
  AdDensityCalculationResult result =
      CalculateDensityWithin(last_main_frame_rect_);
  if (result.ad_density_by_area) {
    max_page_ad_density_by_area_ = std::max(result.ad_density_by_area.value(),
                                            max_page_ad_density_by_area_);
  }
  if (result.ad_density_by_height) {
    max_page_ad_density_by_height_ = std::max(
        result.ad_density_by_height.value(), max_page_ad_density_by_height_);
  }
}

void PageAdDensityTracker::CalculateViewportAdDensity() {
  AdDensityCalculationResult result =
      CalculateDensityWithin(last_main_frame_viewport_rect_);
  if (!result.ad_density_by_area)
    return;

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta elapsed_time = now - last_viewport_density_recording_time_;

  cumulative_viewport_ad_density_by_area_ +=
      last_viewport_ad_density_by_area_ * elapsed_time.InMicrosecondsF();

  last_viewport_density_recording_time_ = now;

  last_viewport_ad_density_by_area_ = result.ad_density_by_area.value();
}

// Ad density measurement uses a modified Bentley's Algorithm, the high level
// approach is described on: http://jeffe.cs.illinois.edu/open/klee.html.
PageAdDensityTracker::AdDensityCalculationResult
PageAdDensityTracker::CalculateDensityWithin(const gfx::Rect& bounding_rect) {
  // Cannot calculate density if `bounding_rect` is empty.
  if (bounding_rect.IsEmpty())
    return {};

  BoundedSegmentLength horizontal_segment_length_tracker(
      /*bound_start=*/bounding_rect.x(),
      /*bound_end=*/bounding_rect.x() + bounding_rect.width());

  absl::optional<int> last_y;
  base::CheckedNumeric<int> total_area = 0;
  base::CheckedNumeric<int> total_height = 0;
  for (const auto& rect_event : rect_events_) {
    if (!last_y) {
      DCHECK(rect_event.is_bottom);
      horizontal_segment_length_tracker.AddSegment(
          rect_event.rect_id, rect_event.rect.x(),
          rect_event.rect.x() + rect_event.rect.width());
      last_y = rect_event.rect.bottom();
    }

    int current_y =
        rect_event.is_bottom ? rect_event.rect.bottom() : rect_event.rect.y();
    DCHECK_LE(current_y, last_y.value());

    // If the segment length value is invalid, skip this ad density calculation.
    absl::optional<int> horizontal_segment_length =
        horizontal_segment_length_tracker.Length();
    if (!horizontal_segment_length)
      return {};

    // Check that the segment length multiplied by the height of the block
    // does not overflow an int.
    base::CheckedNumeric<int> current_area = *horizontal_segment_length;
    int vertical_segment_length = CalculateIntersectedLength(
        current_y, last_y.value(), bounding_rect.y(), bounding_rect.bottom());

    current_area *= vertical_segment_length;

    if (!current_area.IsValid())
      return {};

    total_area += current_area;

    if (*horizontal_segment_length > 0)
      total_height += vertical_segment_length;

    // As we are iterating from the bottom of the page to the top, add segments
    // when we see the start (bottom) of a new rect.
    if (rect_event.is_bottom) {
      horizontal_segment_length_tracker.AddSegment(
          rect_event.rect_id, rect_event.rect.x(),
          rect_event.rect.x() + rect_event.rect.width());
    } else {
      horizontal_segment_length_tracker.RemoveSegment(rect_event.rect_id);
    }
    last_y = current_y;
  }

  // If the measured height or area is invalid, skip recording this ad density
  // calculation.
  if (!total_height.IsValid() || !total_area.IsValid())
    return {};

  AdDensityCalculationResult result;

  // TODO(yaoxia): For viewport density we don't care about density by height.
  // Consider having a param which skips the height calculation.
  base::CheckedNumeric<int> ad_density_by_height =
      total_height * 100 / bounding_rect.height();
  if (ad_density_by_height.IsValid()) {
    result.ad_density_by_height = ad_density_by_height.ValueOrDie();
  }

  // Invalidate the check numeric if the checked area is invalid.
  base::CheckedNumeric<int> ad_density_by_area =
      total_area * 100 /
      (bounding_rect.size().GetCheckedArea().ValueOrDefault(
          std::numeric_limits<int>::max()));
  if (ad_density_by_area.IsValid()) {
    result.ad_density_by_area = ad_density_by_area.ValueOrDie();
  }

  return result;
}

bool PageAdDensityTracker::RectEvent::operator<(const RectEvent& rhs) const {
  int lhs_y = is_bottom ? rect.bottom() : rect.y();
  int rhs_y = rhs.is_bottom ? rhs.rect.bottom() : rhs.rect.y();

  // Tiebreak with |rect_id| and |is_bottom|.
  if (lhs_y == rhs_y) {
    if (rect_id == rhs.rect_id) {
      return is_bottom == rhs.is_bottom;
    } else {
      return rect_id < rhs.rect_id;
    }
  } else {
    return lhs_y > rhs_y;
  }
}

}  // namespace page_load_metrics
