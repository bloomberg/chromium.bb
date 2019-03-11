// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_COOPERATIVE_SCHEDULING_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_COOPERATIVE_SCHEDULING_MANAGER_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {
namespace scheduler {

// This class manages the states for cooperative scheduling and decides whether
// or not to run a nested loop or not.
class PLATFORM_EXPORT CooperativeSchedulingManager {
  USING_FAST_MALLOC(CooperativeSchedulingManager);

 public:
  // This class is used to mark JS executions that have a C++ stack that has
  // been whitelisted for reentry.
  class PLATFORM_EXPORT WhitelistedStackScope {
    STACK_ALLOCATED();

   public:
    WhitelistedStackScope(CooperativeSchedulingManager*);
    ~WhitelistedStackScope();

   private:
    CooperativeSchedulingManager* cooperative_scheduling_manager_;
  };

  // Returns an shared instance for the current thread.
  static CooperativeSchedulingManager* Instance();

  CooperativeSchedulingManager();
  virtual ~CooperativeSchedulingManager() = default;

  // Returns true if the C++ stack has been whitelisted for reentry.
  bool InWhitelistedStackScope() const {
    return whitelisted_stack_scope_depth_ > 0;
  }

  // Calls to this should be inserted where nested loops can be run safely.
  // Typically this is is where Blink has not modified any global state that the
  // nested code could touch.
  void Safepoint();

 protected:
  virtual void RunNestedLoop();

 private:
  void EnterWhitelistedStackScope();
  void LeaveWhitelistedStackScope();
  void SafepointSlow();

  int whitelisted_stack_scope_depth_ = 0;
  bool running_nested_loop_ = false;
  WTF::TimeTicks wait_until_;

  DISALLOW_COPY_AND_ASSIGN(CooperativeSchedulingManager);
};

inline void CooperativeSchedulingManager::Safepoint() {
  if (!InWhitelistedStackScope())
    return;

  if (WTF::CurrentTimeTicks() < wait_until_)
    return;

  SafepointSlow();
}

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_COOPERATIVE_SCHEDULING_MANAGER_H_
