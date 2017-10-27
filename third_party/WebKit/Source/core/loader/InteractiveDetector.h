// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InteractiveDetector_h
#define InteractiveDetector_h

#include "core/CoreExport.h"
#include "platform/LongTaskDetector.h"
#include "platform/PODInterval.h"
#include "platform/Supplementable.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Optional.h"

namespace blink {

class Document;

// Detects when a page reaches First Idle and Time to Interactive. See
// https://goo.gl/SYt55W for detailed description and motivation of First Idle
// and Time to Interactive.
// TODO(crbug.com/631203): This class currently only detects Time to
// Interactive. Implement First Idle.
class CORE_EXPORT InteractiveDetector
    : public GarbageCollectedFinalized<InteractiveDetector>,
      public Supplement<Document>,
      public LongTaskObserver {
  WTF_MAKE_NONCOPYABLE(InteractiveDetector);
  USING_GARBAGE_COLLECTED_MIXIN(InteractiveDetector);

 public:
  // This class can be easily switched out to allow better testing of
  // InteractiveDetector.
  class NetworkActivityChecker {
    WTF_MAKE_NONCOPYABLE(NetworkActivityChecker);

   public:
    NetworkActivityChecker(Document* document) : document_(document) {}

    virtual int GetActiveConnections();
    virtual ~NetworkActivityChecker() = default;

   private:
    WeakPersistent<Document> document_;
  };

  static InteractiveDetector* From(Document&);
  virtual ~InteractiveDetector();

  // Calls to MonotonicallyIncreasingTime is expensive, so we try not to call it
  // unless we really have to. If we already have the event time available, we
  // pass it in as an argument.
  void OnResourceLoadBegin(WTF::Optional<double> load_begin_time);
  void OnResourceLoadEnd(WTF::Optional<double> load_finish_time);

  void SetNavigationStartTime(double navigation_start_time);
  void OnFirstMeaningfulPaintDetected(double fmp_time);
  void OnDomContentLoadedEnd(double dcl_time);

  // Returns Interactive Time if already detected, or 0.0 otherwise.
  double GetInteractiveTime();

  virtual void Trace(Visitor*);

 private:
  friend class InteractiveDetectorTest;

  // Required length of main thread and network quiet window for determining
  // Time to Interactive.
  static constexpr double kTimeToInteractiveWindowSeconds = 5.0;
  // Network is considered "quiet" if there are no more than 2 active network
  // requests for this duration of time.
  static constexpr int kNetworkQuietMaximumConnections = 2;

  explicit InteractiveDetector(Document&, NetworkActivityChecker*);

  double interactive_time_ = 0.0;

  // Page event times that Interactive Detector depends on.
  // Value of 0.0 indicate the event has not been detected yet.
  struct {
    double first_meaningful_paint = 0.0;
    double dom_content_loaded_end = 0.0;
    double nav_start = 0.0;
  } page_event_times_;

  // Stores sufficiently long quiet windows on main thread and network.
  std::vector<PODInterval<double>> main_thread_quiet_windows_;
  std::vector<PODInterval<double>> network_quiet_windows_;

  // Start times of currently active main thread and network quiet windows.
  // Values of 0.0 implies main thread or network is not quiet at the moment.
  double active_main_thread_quiet_window_start_ = 0.0;
  double active_network_quiet_window_start_ = 0.0;

  // Adds currently active quiet main thread and network quiet windows to the
  // vectors. Should be called before calling
  // FindInteractiveCandidate.
  void AddCurrentlyActiveQuietIntervals(double current_time);
  // Undoes AddCurrentlyActiveQuietIntervals.
  void RemoveCurrentlyActiveQuietIntervals();

  std::unique_ptr<NetworkActivityChecker> network_activity_checker_;
  int ActiveConnections();
  void BeginNetworkQuietPeriod(double current_time);
  void EndNetworkQuietPeriod(double current_time);
  // Updates current network quietness tracking information. Opens and closes
  // network quiet windows as necessary.
  void UpdateNetworkQuietState(double request_count,
                               WTF::Optional<double> current_time);

  TaskRunnerTimer<InteractiveDetector> time_to_interactive_timer_;
  double time_to_interactive_timer_fire_time_ = 0.0;
  void StartOrPostponeCITimer(double timer_fire_time);
  void TimeToInteractiveTimerFired(TimerBase*);
  void CheckTimeToInteractiveReached();
  void OnTimeToInteractiveDetected();

  // Finds a window of length kTimeToInteractiveWindowSeconds after lower_bound
  // such that both main thread and network are quiet. Returns the end of last
  // long task before that quiet window, or lower_bound, whichever is bigger -
  // this is called the Interactive Candidate. Returns 0.0 if no such quiet
  // window is found.
  double FindInteractiveCandidate(double lower_bound);

  // LongTaskObserver implementation
  void OnLongTaskDetected(double start_time, double end_time) override;
};

}  // namespace blink

#endif
