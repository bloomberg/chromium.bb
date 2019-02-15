// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/memory_purge_manager.h"

#include "base/memory/memory_pressure_listener.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/launching_process_state.h"

namespace blink {

MemoryPurgeManager::MemoryPurgeManager()
    : renderer_backgrounded_(kLaunchingProcessIsBackgrounded) {}

MemoryPurgeManager::~MemoryPurgeManager() = default;

void MemoryPurgeManager::PageFrozen() {
  if (!CanPurge())
    return;
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::MemoryPressureListener::SetNotificationsSuppressed(true);
}

void MemoryPurgeManager::PageUnfrozen() {
  base::MemoryPressureListener::SetNotificationsSuppressed(false);
}

void MemoryPurgeManager::SetRendererBackgrounded(bool backgrounded) {
  renderer_backgrounded_ = backgrounded;
}

bool MemoryPurgeManager::CanPurge() {
  return !base::FeatureList::IsEnabled(
             features::kPurgeMemoryOnlyForBackgroundedProcesses) ||
         renderer_backgrounded_;
}

}  // namespace blink
