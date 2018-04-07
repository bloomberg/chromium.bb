// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/long_task_detector.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_thread.h"

namespace blink {

// static
LongTaskDetector& LongTaskDetector::Instance() {
  DEFINE_STATIC_LOCAL(Persistent<LongTaskDetector>, long_task_detector,
                      (new LongTaskDetector));
  DCHECK(IsMainThread());
  return *long_task_detector;
}

LongTaskDetector::LongTaskDetector() = default;

void LongTaskDetector::RegisterObserver(LongTaskObserver* observer) {
  DCHECK(IsMainThread());
  DCHECK(observer);
  observers_.insert(observer);
  if (observers_.size() == 1) {
    // Number of observers just became non-zero.
    Platform::Current()->CurrentThread()->AddTaskTimeObserver(this);
  }
}

void LongTaskDetector::UnregisterObserver(LongTaskObserver* observer) {
  DCHECK(IsMainThread());
  observers_.erase(observer);
  if (observers_.size() == 0) {
    Platform::Current()->CurrentThread()->RemoveTaskTimeObserver(this);
  }
}

void LongTaskDetector::DidProcessTask(double start_time, double end_time) {
  if ((end_time - start_time) < LongTaskDetector::kLongTaskThresholdSeconds)
    return;

  for (auto& observer : observers_) {
    observer->OnLongTaskDetected(TimeTicksFromSeconds(start_time),
                                 TimeTicksFromSeconds(end_time));
  }
}

void LongTaskDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(observers_);
}

}  // namespace blink
