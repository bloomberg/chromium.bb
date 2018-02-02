// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InteractiveDetector_h
#define InteractiveDetector_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/LongTaskDetector.h"
#include "platform/PODInterval.h"
#include "platform/Supplementable.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
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
  USING_GARBAGE_COLLECTED_MIXIN(InteractiveDetector);

 public:
  // This class can be easily switched out to allow better testing of
  // InteractiveDetector.
  class CORE_EXPORT NetworkActivityChecker {
   public:
    NetworkActivityChecker(Document* document) : document_(document) {}

    virtual int GetActiveConnections();
    virtual ~NetworkActivityChecker() = default;

   private:
    WeakPersistent<Document> document_;

    DISALLOW_COPY_AND_ASSIGN(NetworkActivityChecker);
  };

  static InteractiveDetector* From(Document&);
  virtual ~InteractiveDetector();

  // Calls to CurrentTimeTicksInSeconds is expensive, so we try not to call it
  // unless we really have to. If we already have the event time available, we
  // pass it in as an argument.
  void OnResourceLoadBegin(WTF::Optional<TimeTicks> load_begin_time);
  void OnResourceLoadEnd(WTF::Optional<TimeTicks> load_finish_time);

  void SetNavigationStartTime(TimeTicks navigation_start_time);
  void OnFirstMeaningfulPaintDetected(TimeTicks fmp_time);
  void OnDomContentLoadedEnd(TimeTicks dcl_time);
  void OnInvalidatingInputEvent(TimeTicks invalidation_time);
  void OnFirstInputDelay(TimeDelta delay_seconds);

  // Returns Interactive Time if already detected, or 0.0 otherwise.
  TimeTicks GetInteractiveTime() const;

  // Returns the time when page interactive was detected. The detection time can
  // be useful to make decisions about metric invalidation in scenarios like tab
  // backgrounding.
  TimeTicks GetInteractiveDetectionTime() const;

  // Returns the first time interactive detector received a significant input
  // that may cause observers to discard the interactive time value.
  TimeTicks GetFirstInvalidatingInputTime() const;

  // The duration between the hardware timestamp and being queued on the main
  // thread for the first click, tap or key press.
  TimeDelta GetFirstInputDelay() const;

  virtual void Trace(Visitor*);

 private:
  friend class InteractiveDetectorTest;

  explicit InteractiveDetector(Document&, NetworkActivityChecker*);

  TimeTicks interactive_time_;
  TimeTicks interactive_detection_time_;

  // Page event times that Interactive Detector depends on.
  // Value of 0.0 indicate the event has not been detected yet.
  struct {
    TimeTicks first_meaningful_paint;
    TimeTicks dom_content_loaded_end;
    TimeTicks nav_start;
    TimeTicks first_invalidating_input;
    TimeDelta first_input_delay;
  } page_event_times_;

  // Stores sufficiently long quiet windows on main thread and network.
  std::vector<PODInterval<TimeTicks>> main_thread_quiet_windows_;
  std::vector<PODInterval<TimeTicks>> network_quiet_windows_;

  // Start times of currently active main thread and network quiet windows.
  // Values of 0.0 implies main thread or network is not quiet at the moment.
  TimeTicks active_main_thread_quiet_window_start_;
  TimeTicks active_network_quiet_window_start_;

  // Adds currently active quiet main thread and network quiet windows to the
  // vectors. Should be called before calling
  // FindInteractiveCandidate.
  void AddCurrentlyActiveQuietIntervals(TimeTicks current_time);
  // Undoes AddCurrentlyActiveQuietIntervals.
  void RemoveCurrentlyActiveQuietIntervals();

  std::unique_ptr<NetworkActivityChecker> network_activity_checker_;
  int ActiveConnections();
  void BeginNetworkQuietPeriod(TimeTicks current_time);
  void EndNetworkQuietPeriod(TimeTicks current_time);
  // Updates current network quietness tracking information. Opens and closes
  // network quiet windows as necessary.
  void UpdateNetworkQuietState(double request_count,
                               WTF::Optional<TimeTicks> current_time);

  TaskRunnerTimer<InteractiveDetector> time_to_interactive_timer_;
  TimeTicks time_to_interactive_timer_fire_time_;
  void StartOrPostponeCITimer(TimeTicks timer_fire_time);
  void TimeToInteractiveTimerFired(TimerBase*);
  void CheckTimeToInteractiveReached();
  void OnTimeToInteractiveDetected();

  // Finds a window of length kTimeToInteractiveWindowSeconds after lower_bound
  // such that both main thread and network are quiet. Returns the end of last
  // long task before that quiet window, or lower_bound, whichever is bigger -
  // this is called the Interactive Candidate. Returns 0.0 if no such quiet
  // window is found.
  TimeTicks FindInteractiveCandidate(TimeTicks lower_bound);

  // LongTaskObserver implementation
  void OnLongTaskDetected(TimeTicks start_time, TimeTicks end_time) override;

  DISALLOW_COPY_AND_ASSIGN(InteractiveDetector);
};

}  // namespace blink

#endif
