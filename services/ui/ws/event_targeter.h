// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EVENT_TARGETER_H_
#define SERVICES_UI_WS_EVENT_TARGETER_H_

#include <stdint.h>
#include <queue>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/ui/ws/window_finder.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
namespace ws {
class EventTargeterDelegate;

// The target |deepest_window| for a given location, locations and |display_id|
// are associated with the display |deepest_window| is on. |location_in_root|
// is the location in root window's coord-space while |location_in_target| is
// the transformed location in the |deepest_window|'s coord-space.
struct LocationTarget {
  DeepestWindow deepest_window;
  gfx::Point location_in_root;
  gfx::Point location_in_target;
  int64_t display_id = display::kInvalidDisplayId;
};

using HitTestCallback = base::OnceCallback<void(const LocationTarget&)>;

// Finds the target window for a location.
class EventTargeter {
 public:
  explicit EventTargeter(EventTargeterDelegate* event_targeter_delegate);
  ~EventTargeter();

  // Calls WindowFinder to find the target for |location|.
  // |callback| is called with the LocationTarget found.
  void FindTargetForLocation(EventSource event_source,
                             const gfx::Point& location,
                             int64_t display_id,
                             HitTestCallback callback);

  bool IsHitTestInFlight() const;

 private:
  struct HitTestRequest {
    HitTestRequest(EventSource event_source,
                   const gfx::Point& location,
                   int64_t display_id,
                   HitTestCallback hittest_callback);
    ~HitTestRequest();

    EventSource event_source;
    gfx::Point location;
    int64_t display_id;
    HitTestCallback callback;
  };

  void ProcessFindTarget(EventSource event_source,
                         const gfx::Point& location,
                         int64_t display_id,
                         HitTestCallback callback);

  void FindTargetForLocationNow(EventSource event_source,
                                const gfx::Point& location,
                                int64_t display_id,
                                HitTestCallback callback);

  void ProcessNextHitTestRequestFromQueue();

  EventTargeterDelegate* event_targeter_delegate_;

  // True if we are waiting for the result of a hit-test. False otherwise.
  bool hit_test_in_flight_;

  // Requests for a new location while waiting on an existing request are added
  // here.
  std::queue<std::unique_ptr<HitTestRequest>> hit_test_request_queue_;

  base::WeakPtrFactory<EventTargeter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EventTargeter);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EVENT_TARGETER_H_
