// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/event_targeter.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/host/hit_test/hit_test_query.h"
#include "services/ui/common/switches.h"
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
  // TODO(riajiang): After the async ask-client part is implemented, the async
  // part should be moved to after sync viz-hit-test call.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseAsyncEventTargeting)) {
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
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseVizHitTest)) {
    if (root) {
      location_target.deepest_window =
          ui::ws::FindDeepestVisibleWindowForLocation(
              root, event_source, location_target.location_in_root);
    }
  } else {
    viz::HitTestQuery* hit_test_query =
        event_targeter_delegate_->GetHitTestQueryForDisplay(
            location_target.display_id);
    if (hit_test_query) {
      viz::Target target = hit_test_query->FindTargetForLocation(
          location_target.location_in_root);
      if (target.frame_sink_id.is_valid()) {
        ServerWindow* target_window =
            event_targeter_delegate_->GetWindowFromFrameSinkId(
                target.frame_sink_id);
        if (!target_window) {
          // TODO(riajiang): There's no target window with this frame_sink_id,
          // maybe a security fault. http://crbug.com/746470
          NOTREACHED();
        }
        location_target.deepest_window.window = target_window;
        location_target.location_in_target = target.location_in_target;
      }
    }
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
