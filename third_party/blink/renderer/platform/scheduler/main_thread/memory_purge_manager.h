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

  // Called when a page is frozen. Simulates a critical memory pressure signal
  // to purge memory. If the kPurgeMemoryOnlyForBackgroundedProcesses feature
  // is enabled and the renderer is foregrounded, no signal will be sent.
  void PageFrozen();

  // Called when a page is unfrozen. Has the effect of unsuppressing memory
  // pressure notifications.
  void PageUnfrozen();

  // Called when the renderer's process priority changes.
  void SetRendererBackgrounded(bool backgrounded);

 private:
  bool CanPurge();

  bool renderer_backgrounded_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPurgeManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MEMORY_PURGE_MANAGER_H_
