// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// Manages the process-wide memory purging associated with freezing.
class PLATFORM_EXPORT MemoryPurgeManager {
 public:
  MemoryPurgeManager();
  ~MemoryPurgeManager();

  // Called when a page is created or destroyed, to maintain the total count of
  // pages owned by a renderer.
  void OnPageCreated(bool is_frozen);
  void OnPageDestroyed(bool was_frozen);

  // Called when a page is frozen. If the renderer is backgrounded, sends a
  // critical memory pressure signal to purge memory.
  void OnPageFrozen();

  // Called when a page is unfrozen. Has the effect of unsuppressing memory
  // pressure notifications.
  void OnPageUnfrozen();

  // Called when the renderer's process priority changes.
  void SetRendererBackgrounded(bool backgrounded);

 private:
  bool CanPurge() const;

  bool AreAllPagesFrozen() const;

  bool renderer_backgrounded_;

  int total_page_count_;
  int frozen_page_count_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPurgeManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_
