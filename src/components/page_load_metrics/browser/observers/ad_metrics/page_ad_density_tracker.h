// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_PAGE_AD_DENSITY_TRACKER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_PAGE_AD_DENSITY_TRACKER_H_

#include <set>
#include <unordered_map>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace page_load_metrics {

// Tracks the ad density of a page through the page's lifecycle.
// It has the following usage:
//    1. Set subframe and mainframe rects using subframe and mainframe rect
//        operations (AddRect, RemoveRect, UpdateMainFrameRect).
//    2. Once a page has a main frame rect, get current density using
//        DensityByHeight or DensityByArea.
class PageAdDensityTracker {
 public:
  PageAdDensityTracker();
  ~PageAdDensityTracker();

  PageAdDensityTracker(const PageAdDensityTracker&) = delete;
  PageAdDensityTracker& operator=(const PageAdDensityTracker&) = delete;

  // Operations to track sub frame rects in the page density calcluation.
  void AddRect(int rect_id, const gfx::Rect& rect);

  // Removes a rect from the tracker if it is currently being tracked.
  // Otherwise RemoveRect is a no op.
  void RemoveRect(int rect_id);

  // Operations to track the main frame dimensions. The main frame rect has to
  // be set to calculate density.
  void UpdateMainFrameRect(const gfx::Rect& rect);

  // Returns the density by height, as a value from 0-100. If the density
  // calculation fails (i.e. no main frame size), this returns -1. Percentage
  // density by height is calculated as the the combined height of ads divided
  // by the page's height.
  int MaxPageAdDensityByHeight();

  // Returns the density by area, as a value from 0-100. If the density
  // calculation fails (i.e. no main frame size), this returns -1.
  int MaxPageAdDensityByArea();

 private:
  // An event to process corresponding to the top or bottom of each rect.
  struct RectEvent {
    RectEvent(int id, bool is_bottom, const gfx::Rect& rect);
    RectEvent(const RectEvent& other);

    // A unique identifier set when adding and removing rect events
    // corresponding to a single rect.
    int rect_id;
    bool is_bottom;
    gfx::Rect rect;

    // RectEvents are sorted by descending y value of the segment associated
    // with the event.
    bool operator<(const RectEvent& rhs) const;
  };

  // Iterators into the set of rect events for efficient removal of
  // rect events by rect_id. Maintained by |rect_events_iterators_|.
  struct RectEventSetIterators {
    RectEventSetIterators(std::set<RectEvent>::iterator top,
                          std::set<RectEvent>::iterator bottom);
    RectEventSetIterators(const RectEventSetIterators& other);

    std::set<RectEvent>::const_iterator top_it;
    std::set<RectEvent>::const_iterator bottom_it;
  };

  // Calculates the combined area and height of the set of rects, this populates
  // total_height_ and total_area_.
  void CalculateDensity();

  // Maintain a sorted set of rect events for use in calculating ad area.
  std::set<RectEvent> rect_events_;

  // Map from rect_id to iterators of rect events in rect_events_. This allows
  // efficient removal according to rect_id.
  std::unordered_map<int, RectEventSetIterators> rect_events_iterators_;

  // Percentage of page ad density as a value from 0-100. These only have
  // a value of -1 when ad density has not yet been calculated successfully.
  int max_page_ad_density_by_area_ = -1;
  int max_page_ad_density_by_height_ = -1;

  absl::optional<gfx::Rect> last_main_frame_size_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_AD_METRICS_PAGE_AD_DENSITY_TRACKER_H_"
