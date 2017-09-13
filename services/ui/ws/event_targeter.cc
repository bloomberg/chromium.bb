// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/event_targeter.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/host/hit_test/hit_test_query.h"
#include "services/ui/common/switches.h"
#include "services/ui/ws/event_location.h"
#include "services/ui/ws/event_targeter_delegate.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace ui {
namespace ws {

EventTargeter::EventTargeter(EventTargeterDelegate* event_targeter_delegate)
    : event_targeter_delegate_(event_targeter_delegate),
      weak_ptr_factory_(this) {}

EventTargeter::~EventTargeter() {}

void EventTargeter::FindTargetForLocation(EventSource event_source,
                                          const EventLocation& event_location,
                                          HitTestCallback callback) {
  // TODO(riajiang): After the async ask-client part is implemented, the async
  // part should be moved to after sync viz-hit-test call.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseAsyncEventTargeting)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&EventTargeter::FindTargetForLocationNow,
                                  weak_ptr_factory_.GetWeakPtr(), event_source,
                                  event_location, base::Passed(&callback)));
  } else {
    FindTargetForLocationNow(event_source, event_location, std::move(callback));
  }
}

void EventTargeter::FindTargetForLocationNow(
    EventSource event_source,
    const EventLocation& event_location,
    HitTestCallback callback) {
  ServerWindow* root = event_targeter_delegate_->GetRootWindowForDisplay(
      event_location.display_id);
  DeepestWindow deepest_window;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseVizHitTest)) {
    if (root) {
      deepest_window = ui::ws::FindDeepestVisibleWindowForLocation(
          root, event_source, gfx::ToFlooredPoint(event_location.raw_location));
    }
  } else {
    viz::HitTestQuery* hit_test_query =
        event_targeter_delegate_->GetHitTestQueryForDisplay(
            event_location.display_id);
    if (hit_test_query) {
      viz::Target target = hit_test_query->FindTargetForLocation(
          event_source, gfx::ToFlooredPoint(event_location.raw_location));
      if (target.frame_sink_id.is_valid()) {
        ServerWindow* target_window =
            event_targeter_delegate_->GetWindowFromFrameSinkId(
                target.frame_sink_id);
        if (!target_window) {
          // TODO(riajiang): Investigate when this would be a security fault.
          // http://crbug.com/746470
          base::RecordAction(
              base::UserMetricsAction("EventTargeting_DeletedTarget"));
        }
        deepest_window.window = target_window;
        // TODO(riajiang): use |target.location_in_target|.
      }
    }
  }
  std::move(callback).Run(event_location, deepest_window);
}

}  // namespace ws
}  // namespace ui
