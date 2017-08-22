// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/LongTaskDetector.h"

#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

// static
LongTaskDetector& LongTaskDetector::Instance() {
  DEFINE_STATIC_LOCAL(Persistent<LongTaskDetector>, long_task_detector,
                      (new LongTaskDetector));
  DCHECK(IsMainThread());
  return *long_task_detector;
}

LongTaskDetector::LongTaskDetector() {}

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
    observer->OnLongTaskDetected(start_time, end_time);
  }
}

DEFINE_TRACE(LongTaskDetector) {
  visitor->Trace(observers_);
}

}  // namespace blink
