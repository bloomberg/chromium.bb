// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LongTaskDetector_h
#define LongTaskDetector_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "platform/scheduler/base/task_time_observer.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebThread.h"

namespace blink {

class PLATFORM_EXPORT LongTaskObserver : public GarbageCollectedMixin {
 public:
  virtual ~LongTaskObserver() {}

  virtual void OnLongTaskDetected(double start_time, double end_time) = 0;
};

// LongTaskDetector detects tasks longer than kLongTaskThreshold and notifies
// observers. When it has non-zero LongTaskObserver, it adds itself as a
// TaskTimeObserver on the main thread and observes every task. When the number
// of LongTaskObservers drop to zero it automatically removes itself as a
// TaskTimeObserver.
class PLATFORM_EXPORT LongTaskDetector final
    : public GarbageCollectedFinalized<LongTaskDetector>,
      public scheduler::TaskTimeObserver {
  WTF_MAKE_NONCOPYABLE(LongTaskDetector);

 public:
  static LongTaskDetector& Instance();

  void RegisterObserver(LongTaskObserver*);
  void UnregisterObserver(LongTaskObserver*);

  void Trace(blink::Visitor*);

  static constexpr double kLongTaskThresholdSeconds = 0.05;

 private:
  LongTaskDetector();

  // scheduler::TaskTimeObserver implementation
  void WillProcessTask(double start_time) override {}
  void DidProcessTask(double start_time, double end_time) override;

  HeapHashSet<WeakMember<LongTaskObserver>> observers_;
};

}  // namespace blink

#endif  // LongTaskDetector_h
