// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/memory_purge_manager.h"

#include "base/memory/memory_pressure_listener.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/launching_process_state.h"

namespace blink {

MemoryPurgeManager::MemoryPurgeManager()
    : renderer_backgrounded_(kLaunchingProcessIsBackgrounded),
      total_page_count_(0),
      frozen_page_count_(0) {}

MemoryPurgeManager::~MemoryPurgeManager() = default;

void MemoryPurgeManager::OnPageCreated(bool is_frozen) {
  total_page_count_++;
  if (is_frozen) {
    frozen_page_count_++;
  } else {
    base::MemoryPressureListener::SetNotificationsSuppressed(false);
  }
}

void MemoryPurgeManager::OnPageDestroyed(bool was_frozen) {
  DCHECK_GT(total_page_count_, 0);
  DCHECK_GE(frozen_page_count_, 0);
  total_page_count_--;
  if (was_frozen)
    frozen_page_count_--;
  DCHECK_LE(frozen_page_count_, total_page_count_);
}

void MemoryPurgeManager::OnPageFrozen() {
  DCHECK_LT(frozen_page_count_, total_page_count_);
  frozen_page_count_++;

  if (!CanPurge())
    return;

  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  if (AreAllPagesFrozen())
    base::MemoryPressureListener::SetNotificationsSuppressed(true);
}

void MemoryPurgeManager::OnPageUnfrozen() {
  DCHECK_GT(frozen_page_count_, 0);
  frozen_page_count_--;
  base::MemoryPressureListener::SetNotificationsSuppressed(false);
}

void MemoryPurgeManager::SetRendererBackgrounded(bool backgrounded) {
  renderer_backgrounded_ = backgrounded;
}

bool MemoryPurgeManager::CanPurge() const {
  return renderer_backgrounded_;
}

bool MemoryPurgeManager::AreAllPagesFrozen() const {
  return total_page_count_ == frozen_page_count_;
}

}  // namespace blink
