// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_thread.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

ParkableStringManager& ParkableStringManager::Instance() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(ParkableStringManager, instance, ());
  return instance;
}

ParkableStringManager::~ParkableStringManager() {}

void ParkableStringManager::SetRendererBackgrounded(bool backgrounded) {
  backgrounded_ = backgrounded;

  if (backgrounded_) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        Platform::Current()->CurrentThread()->GetTaskRunner();
    if (task_runner) {  // nullptr in tests.
      task_runner->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ParkableStringManager::ParkAllIfRendererBackgrounded,
                         base::Unretained(this)),
          base::TimeDelta::FromSeconds(10));
    }
  }
}

bool ParkableStringManager::IsRendererBackgrounded() const {
  return backgrounded_;
}

// static
bool ParkableStringManager::ShouldPark(const StringImpl& string) {
  // Don't attempt to park strings smaller than this size.
  static constexpr unsigned int kSizeThreshold = 10000;
  // TODO(lizeb): Consider parking non-main thread strings.
  return string.length() > kSizeThreshold && IsMainThread();
}

scoped_refptr<ParkableStringImpl> ParkableStringManager::Add(
    scoped_refptr<StringImpl>&& string) {
  StringImpl* raw_ptr = string.get();
  auto it = table_.find(raw_ptr);
  if (it != table_.end())
    return it->value;

  auto new_parkable_string = base::MakeRefCounted<ParkableStringImpl>(
      std::move(string), ParkableStringImpl::ParkableState::kParkable);
  table_.insert(raw_ptr, new_parkable_string.get());
  return new_parkable_string;
}

void ParkableStringManager::Remove(StringImpl* string) {
  DCHECK(string);
  DCHECK(ShouldPark(*string));
  auto it = table_.find(string);
  DCHECK(it != table_.end());
  table_.erase(it);
}

void ParkableStringManager::ParkAllIfRendererBackgrounded() {
  DCHECK(IsMainThread());
  if (!IsRendererBackgrounded())
    return;

  size_t total_size = 0, count = 0;
  for (ParkableStringImpl* str : table_.Values()) {
    str->Park();
    total_size += str->CharactersSizeInBytes();
    count += 1;
  }

  size_t total_size_kb = total_size / 1000;
  UMA_HISTOGRAM_COUNTS_100000("Memory.MovableStringsTotalSizeKb",
                              total_size_kb);
  UMA_HISTOGRAM_COUNTS_1000("Memory.MovableStringsCount", table_.size());
}

ParkableStringManager::ParkableStringManager()
    : backgrounded_(false), table_() {}

}  // namespace blink
