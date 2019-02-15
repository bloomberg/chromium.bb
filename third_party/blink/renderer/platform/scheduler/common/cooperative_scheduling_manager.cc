// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"

#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {
namespace scheduler {

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

}  // namespace scheduler
}  // namespace blink
