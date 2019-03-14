// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_lifecycle_state.h"

namespace blink {

// Manages the process-wide memory purging associated with freezing.
class PLATFORM_EXPORT MemoryPurgeManager {
 public:
  MemoryPurgeManager();
  ~MemoryPurgeManager();

  // Called when a page is created or destroyed, to maintain the total count of
  // pages owned by a renderer.
  void OnPageCreated(PageLifecycleState state);
  void OnPageDestroyed(PageLifecycleState state);

  // Called when a page is frozen. If all pages are frozen or
  // |kFreezePurgeMemoryAllPagesFrozen| is disabled, and the renderer is
  // backgrounded, ensures that a delayed memory purge is scheduled.
  void OnPageFrozen();

  // Called when a page is resumed (unfrozen). Has the effect of unsuppressing
  // memory pressure notifications.
  void OnPageResumed();

  // Called when the renderer's process priority changes.
  void SetRendererBackgrounded(bool backgrounded);

  // The time of purging after all pages have been frozen.
  static constexpr int kDefaultTimeToPurgeAfterFreezing = 0;

 private:
  // Called when the timer expires. Simulates a critical memory pressure signal
  // to purge memory. Suppresses memory pressure notifications if all pages
  // are frozen.
  void PerformMemoryPurge();

  // Returns true if all of the following are true:
  // - The renderer is backgrounded.
  // - All pages are frozen or kFreezePurgeMemoryAllPagesFrozen is disabled.
  bool CanPurge() const;

  // Returns true if |total_page_count_| == |frozen_page_count_|
  bool AreAllPagesFrozen() const;

  bool renderer_backgrounded_;

  int total_page_count_;
  int frozen_page_count_;

  // Timer to delay memory purging.
  //
  // TODO(adityakeerthi): This timer should use a best-effort task runner.
  base::OneShotTimer purge_timer_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPurgeManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_
