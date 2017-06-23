// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/event_targeter.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/ui/ws/event_targeter_delegate.h"

namespace ui {
namespace ws {

EventTargeter::HitTestRequest::HitTestRequest(EventSource event_source,
                                              const gfx::Point& location,
                                              int64_t display_id,
                                              HitTestCallback callback)
    : event_source(event_source),
      location(location),
      display_id(display_id),
      callback(std::move(callback)) {}

EventTargeter::HitTestRequest::~HitTestRequest() {}

EventTargeter::EventTargeter(EventTargeterDelegate* event_targeter_delegate)
    : event_targeter_delegate_(event_targeter_delegate),
      hit_test_in_flight_(false),
      weak_ptr_factory_(this) {}

EventTargeter::~EventTargeter() {}

void EventTargeter::FindTargetForLocation(EventSource event_source,
                                          const gfx::Point& location,
                                          int64_t display_id,
                                          HitTestCallback callback) {
  if (IsHitTestInFlight()) {
    std::unique_ptr<HitTestRequest> hittest_request =
        base::MakeUnique<HitTestRequest>(event_source, location, display_id,
                                         std::move(callback));
    hit_test_request_queue_.push(std::move(hittest_request));
    return;
  }

  ProcessFindTarget(event_source, location, display_id, std::move(callback));
}

bool EventTargeter::IsHitTestInFlight() const {
  return hit_test_in_flight_ || !hit_test_request_queue_.empty();
}

void EventTargeter::ProcessFindTarget(EventSource event_source,
                                      const gfx::Point& location,
                                      int64_t display_id,
                                      HitTestCallback callback) {
  // TODO(riajiang): After HitTestComponent is implemented, do synchronous
  // hit-test for most cases using shared memory and only ask Blink
  // asynchronously for hard cases. For now, assume all synchronous hit-tests
  // failed if the "enable-async-event-targeting" flag is turned on.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "enable-async-event-targeting")) {
    DCHECK(!hit_test_in_flight_);
    hit_test_in_flight_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&EventTargeter::FindTargetForLocationNow,
                       weak_ptr_factory_.GetWeakPtr(), event_source, location,
                       display_id, base::Passed(&callback)));
  } else {
    FindTargetForLocationNow(event_source, location, display_id,
                             std::move(callback));
  }
}

void EventTargeter::FindTargetForLocationNow(EventSource event_source,
                                             const gfx::Point& location,
                                             int64_t display_id,
                                             HitTestCallback callback) {
  LocationTarget location_target;
  location_target.location_in_root = location;
  location_target.display_id = display_id;
  ServerWindow* root = event_targeter_delegate_->GetRootWindowContaining(
      &location_target.location_in_root, &location_target.display_id);
  if (root) {
    location_target.deepest_window =
        ui::ws::FindDeepestVisibleWindowForLocation(
            root, event_source, location_target.location_in_root);
  }
  std::move(callback).Run(location_target);
  ProcessNextHitTestRequestFromQueue();
}

void EventTargeter::ProcessNextHitTestRequestFromQueue() {
  hit_test_in_flight_ = false;
  if (hit_test_request_queue_.empty()) {
    event_targeter_delegate_->ProcessNextAvailableEvent();
    return;
  }

  std::unique_ptr<HitTestRequest> hittest_request =
      std::move(hit_test_request_queue_.front());
  hit_test_request_queue_.pop();
  ProcessFindTarget(hittest_request->event_source, hittest_request->location,
                    hittest_request->display_id,
                    std::move(hittest_request->callback));
}

}  // namespace ws
}  // namespace ui
