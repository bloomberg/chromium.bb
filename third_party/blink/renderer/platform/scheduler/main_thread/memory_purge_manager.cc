// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/memory_purge_manager.h"

#include "base/feature_list.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/field_trial_params.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/launching_process_state.h"

namespace blink {

namespace {

base::TimeDelta FreezePurgeMemoryAllPagesFrozenDelay() {
  static const base::FeatureParam<int>
      kFreezePurgeMemoryAllPagesFrozenDelayInMinutes{
          &blink::features::kFreezePurgeMemoryAllPagesFrozen,
          "delay-in-minutes",
          MemoryPurgeManager::kDefaultTimeToPurgeAfterFreezing};
  return base::TimeDelta::FromMinutes(
      kFreezePurgeMemoryAllPagesFrozenDelayInMinutes.Get());
}

}  // namespace

MemoryPurgeManager::MemoryPurgeManager()
    : renderer_backgrounded_(kLaunchingProcessIsBackgrounded),
      total_page_count_(0),
      frozen_page_count_(0) {}

MemoryPurgeManager::~MemoryPurgeManager() = default;

void MemoryPurgeManager::OnPageCreated(PageLifecycleState state) {
  total_page_count_++;
  if (state == PageLifecycleState::kFrozen) {
    frozen_page_count_++;
  } else {
    base::MemoryPressureListener::SetNotificationsSuppressed(false);
  }

  if (!CanPurge())
    purge_timer_.Stop();
}

void MemoryPurgeManager::OnPageDestroyed(PageLifecycleState state) {
  DCHECK_GT(total_page_count_, 0);
  DCHECK_GE(frozen_page_count_, 0);
  total_page_count_--;
  if (state == PageLifecycleState::kFrozen)
    frozen_page_count_--;
  DCHECK_LE(frozen_page_count_, total_page_count_);
}

void MemoryPurgeManager::OnPageFrozen() {
  DCHECK_LT(frozen_page_count_, total_page_count_);
  frozen_page_count_++;

  if (purge_timer_.IsRunning())
    return;

  if (!CanPurge())
    return;

  purge_timer_.Start(FROM_HERE, FreezePurgeMemoryAllPagesFrozenDelay(), this,
                     &MemoryPurgeManager::PerformMemoryPurge);
}

void MemoryPurgeManager::OnPageResumed() {
  DCHECK_GT(frozen_page_count_, 0);
  frozen_page_count_--;
  if (!CanPurge())
    purge_timer_.Stop();
  base::MemoryPressureListener::SetNotificationsSuppressed(false);
}

void MemoryPurgeManager::PerformMemoryPurge() {
  DCHECK(CanPurge());

  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  if (AreAllPagesFrozen())
    base::MemoryPressureListener::SetNotificationsSuppressed(true);
}

void MemoryPurgeManager::SetRendererBackgrounded(bool backgrounded) {
  if (!backgrounded)
    purge_timer_.Stop();
  renderer_backgrounded_ = backgrounded;
}

bool MemoryPurgeManager::CanPurge() const {
  if (!renderer_backgrounded_)
    return false;

  if (!AreAllPagesFrozen() && base::FeatureList::IsEnabled(
                                  features::kFreezePurgeMemoryAllPagesFrozen)) {
    return false;
  }

  return true;
}

bool MemoryPurgeManager::AreAllPagesFrozen() const {
  return total_page_count_ == frozen_page_count_;
}

}  // namespace blink
