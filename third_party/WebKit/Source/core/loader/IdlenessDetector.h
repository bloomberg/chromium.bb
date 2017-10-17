// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IdlenessDetector_h
#define IdlenessDetector_h

#include "core/CoreExport.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/scheduler/base/task_time_observer.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class LocalFrame;
class ResourceFetcher;

// IdlenessDetector observes network request count everytime a load is
// finshed after DOMContentLoadedEventEnd is fired, and emit network almost idle
// signal when there are no more than 2 network connection active in 0.5 second,
// and emit network idle signal when there is 0 network connection active in 0.5
// second.
class CORE_EXPORT IdlenessDetector
    : public GarbageCollectedFinalized<IdlenessDetector>,
      public scheduler::TaskTimeObserver {
  WTF_MAKE_NONCOPYABLE(IdlenessDetector);

 public:
  explicit IdlenessDetector(LocalFrame*);

  void Shutdown();
  void DomContentLoadedEventFired();
  // TODO(lpy) Don't need to pass in fetcher once the command line of disabling
  // PlzNavigate is removed.
  void OnWillSendRequest(ResourceFetcher*);
  void OnDidLoadResource();

  DECLARE_TRACE();

 private:
  friend class IdlenessDetectorTest;

  // The page is quiet if there are no more than 2 active network requests for
  // this duration of time.
  static constexpr double kNetworkQuietWindowSeconds = 0.5;
  static constexpr double kNetworkQuietWatchdogSeconds = 2;
  static constexpr int kNetworkQuietMaximumConnections = 2;

  // scheduler::TaskTimeObserver implementation
  void WillProcessTask(double start_time) override;
  void DidProcessTask(double start_time, double end_time) override;

  void Stop();
  void NetworkQuietTimerFired(TimerBase*);

  Member<LocalFrame> local_frame_;
  bool task_observer_added_;
  // Store the accumulated time of network quiet.
  double network_0_quiet_ = 0;
  double network_2_quiet_ = 0;
  // Record the actual start time of network quiet.
  double network_0_quiet_start_time_;
  double network_2_quiet_start_time_;
  TaskRunnerTimer<IdlenessDetector> network_quiet_timer_;
};

}  // namespace blink

#endif
