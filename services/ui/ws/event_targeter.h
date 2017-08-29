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

// Contains a location relative to a particular display.
struct DisplayLocation {
  gfx::Point location;
  int64_t display_id;
};

using HitTestCallback =
    base::OnceCallback<void(const DisplayLocation&, const DeepestWindow&)>;

// Finds the target window for a location.
class EventTargeter {
 public:
  explicit EventTargeter(EventTargeterDelegate* event_targeter_delegate);
  ~EventTargeter();

  // Calls WindowFinder to find the target for |display_location|. |callback| is
  // called with the found target.
  void FindTargetForLocation(EventSource event_source,
                             const DisplayLocation& display_location,
                             HitTestCallback callback);

  bool IsHitTestInFlight() const;

 private:
  struct HitTestRequest {
    HitTestRequest(EventSource event_source,
                   const DisplayLocation& display_location,
                   HitTestCallback hittest_callback);
    ~HitTestRequest();

    EventSource event_source;
    DisplayLocation display_location;
    HitTestCallback callback;
  };

  void ProcessFindTarget(EventSource event_source,
                         const DisplayLocation& display_location,
                         HitTestCallback callback);

  void FindTargetForLocationNow(EventSource event_source,
                                const DisplayLocation& display_location,
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
