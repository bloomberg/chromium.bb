// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {
namespace scheduler {

namespace {
// Minimum time interval between nested loop runs.
constexpr WTF::TimeDelta kNestedLoopMinimumInterval =
    WTF::TimeDelta::FromMilliseconds(15);
}  // namespace

// static
CooperativeSchedulingManager* CooperativeSchedulingManager::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<CooperativeSchedulingManager>,
                                  manager, ());
  return &(*manager);
}

CooperativeSchedulingManager::WhitelistedStackScope::WhitelistedStackScope(
    CooperativeSchedulingManager* manager) {
  cooperative_scheduling_manager_ = manager;
  cooperative_scheduling_manager_->EnterWhitelistedStackScope();
}

CooperativeSchedulingManager::WhitelistedStackScope::~WhitelistedStackScope() {
  cooperative_scheduling_manager_->LeaveWhitelistedStackScope();
}

CooperativeSchedulingManager::CooperativeSchedulingManager() {}

void CooperativeSchedulingManager::EnterWhitelistedStackScope() {
  TRACE_EVENT_ASYNC_BEGIN0("renderer.scheduler",
                           "PreemptionWhitelistedStackScope", this);

  whitelisted_stack_scope_depth_++;
}

void CooperativeSchedulingManager::LeaveWhitelistedStackScope() {
  TRACE_EVENT_ASYNC_END0("renderer.scheduler",
                         "PreemptionWhitelistedStackScope", this);
  whitelisted_stack_scope_depth_--;
  DCHECK_GE(whitelisted_stack_scope_depth_, 0);
}

void CooperativeSchedulingManager::SafepointSlow() {
  // Avoid nesting more than two levels.
  if (running_nested_loop_)
    return;

  // TODO(keishi): Also bail if V8 EnteredContextCount is more than 1

  RunNestedLoop();
}

void CooperativeSchedulingManager::RunNestedLoop() {
  TRACE_EVENT0("renderer.scheduler",
               "CooperativeSchedulingManager::RunNestedLoop");
  base::AutoReset<bool>(&running_nested_loop_, true);
  wait_until_ = WTF::CurrentTimeTicks() + kNestedLoopMinimumInterval;

  // TODO(keishi): Ask scheduler to run high priority tasks from different
  // EventLoops.
}

}  // namespace scheduler
}  // namespace blink
