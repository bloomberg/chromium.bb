// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_INTERACTIVE_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_INTERACTIVE_DETECTOR_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/loader/long_task_detector.h"
#include "third_party/blink/renderer/core/page/page_hidden_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/pod_interval.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class TickClock;
}  // namespace base

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace blink {

class Document;
class Event;

// Detects when a page reaches First Idle and Time to Interactive. See
// https://goo.gl/SYt55W for detailed description and motivation of First Idle
// and Time to Interactive.
// TODO(crbug.com/631203): This class currently only detects Time to
// Interactive. Implement First Idle.
class CORE_EXPORT InteractiveDetector
    : public GarbageCollected<InteractiveDetector>,
      public Supplement<Document>,
      public ExecutionContextLifecycleObserver,
      public LongTaskObserver {
  USING_GARBAGE_COLLECTED_MIXIN(InteractiveDetector);

 public:
  static const char kSupplementName[];

  // This class can be easily switched out to allow better testing of
  // InteractiveDetector.
  class CORE_EXPORT NetworkActivityChecker {
   public:
    explicit NetworkActivityChecker(Document* document) : document_(document) {}

    virtual int GetActiveConnections();
    virtual ~NetworkActivityChecker() = default;

   private:
    WeakPersistent<Document> document_;

    DISALLOW_COPY_AND_ASSIGN(NetworkActivityChecker);
  };

  static InteractiveDetector* From(Document&);
  // Exposed for tests. See crbug.com/810381. We must use a consistent address
  // for the supplement name.
  static const char* SupplementName();

  explicit InteractiveDetector(Document&, NetworkActivityChecker*);
  ~InteractiveDetector() override = default;

  // Calls to base::TimeTicks::Now().since_origin().InSecondsF() is expensive,
  // so we try not to call it unless we really have to. If we already have the
  // event time available, we pass it in as an argument.
  void OnResourceLoadBegin(base::Optional<base::TimeTicks> load_begin_time);
  void OnResourceLoadEnd(base::Optional<base::TimeTicks> load_finish_time);

  void SetNavigationStartTime(base::TimeTicks navigation_start_time);
  void OnFirstContentfulPaint(base::TimeTicks first_contentful_paint);
  void OnDomContentLoadedEnd(base::TimeTicks dcl_time);
  void OnInvalidatingInputEvent(base::TimeTicks invalidation_time);
  void OnPageHiddenChanged(bool is_hidden);

  // The duration between the hardware timestamp and being queued on the main
  // thread for the first click, tap, key press, cancelable touchstart, or
  // pointer down followed by a pointer up.
  base::Optional<base::TimeDelta> GetFirstInputDelay() const;

  // The timestamp of the event whose delay is reported by GetFirstInputDelay().
  base::Optional<base::TimeTicks> GetFirstInputTimestamp() const;

  // Queueing Time of the meaningful input event with longest delay. Meaningful
  // input events are click, tap, key press, cancellable touchstart, or pointer
  // down followed by a pointer up.
  base::Optional<base::TimeDelta> GetLongestInputDelay() const;

  // The timestamp of the event whose delay is reported by
  // GetLongestInputDelay().
  base::Optional<base::TimeTicks> GetLongestInputTimestamp() const;

  // Process an input event, updating first_input_delay and
  // first_input_timestamp if needed.
  void HandleForInputDelay(const Event&,
                           base::TimeTicks event_platform_timestamp,
                           base::TimeTicks processing_start);

  // ExecutionContextLifecycleObserver
  void ContextDestroyed() override;

  void Trace(Visitor*) override;

  void SetTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing);
  // The caller owns the |clock| which must outlive the InteractiveDetector.
  void SetTickClockForTesting(const base::TickClock* clock);

  ukm::UkmRecorder* GetUkmRecorder() const;

  void SetUkmRecorderForTesting(ukm::UkmRecorder* test_ukm_recorder);

 private:
  friend class InteractiveDetectorTest;

  const base::TickClock* clock_;

  base::TimeTicks interactive_time_;
  base::TimeTicks interactive_detection_time_;

  // Page event times that Interactive Detector depends on.
  // Null base::TimeTicks values indicate the event has not been detected yet.
  struct {
    base::TimeTicks first_contentful_paint;
    base::TimeTicks dom_content_loaded_end;
    base::TimeTicks nav_start;
    // The timestamp of the first input that would invalidate a Time to
    // Interactive computation. This is used when reporting Time To Interactive
    // on a trace event.
    base::TimeTicks first_invalidating_input;
    base::Optional<base::TimeDelta> first_input_delay;
    base::Optional<base::TimeDelta> longest_input_delay;
    base::Optional<base::TimeTicks> first_input_timestamp;
    base::Optional<base::TimeTicks> longest_input_timestamp;
  } page_event_times_;

  struct VisibilityChangeEvent {
    base::TimeTicks timestamp;
    bool was_hidden;
  };

  // Stores sufficiently long quiet windows on the network.
  Vector<WTF::PODInterval<base::TimeTicks>> network_quiet_windows_;

  // Stores long tasks in order to compute Total Blocking Time (TBT) once Time
  // To Interactive (TTI) is known.
  Vector<WTF::PODInterval<base::TimeTicks>> long_tasks_;

  // Start time of currently active network quiet windows.
  // Null base::TimeTicks values indicate network is not quiet at the moment.
  base::TimeTicks active_network_quiet_window_start_;

  // Adds currently active quiet network quiet window to the
  // vector. Should be called before calling FindInteractiveCandidate.
  void AddCurrentlyActiveNetworkQuietInterval(base::TimeTicks current_time);
  // Undoes AddCurrentlyActiveNetworkQuietInterval.
  void RemoveCurrentlyActiveNetworkQuietInterval();

  std::unique_ptr<NetworkActivityChecker> network_activity_checker_;
  int ActiveConnections();
  void BeginNetworkQuietPeriod(base::TimeTicks current_time);
  void EndNetworkQuietPeriod(base::TimeTicks current_time);
  // Updates current network quietness tracking information. Opens and closes
  // network quiet windows as necessary.
  void UpdateNetworkQuietState(double request_count,
                               base::Optional<base::TimeTicks> current_time);

  TaskRunnerTimer<InteractiveDetector> time_to_interactive_timer_;
  base::TimeTicks time_to_interactive_timer_fire_time_;
  void StartOrPostponeCITimer(base::TimeTicks timer_fire_time);
  void TimeToInteractiveTimerFired(TimerBase*);
  void CheckTimeToInteractiveReached();
  void OnTimeToInteractiveDetected();
  std::unique_ptr<TracedValue> ComputeTimeToInteractiveTraceArgs();
  base::TimeDelta ComputeTotalBlockingTime();

  Vector<VisibilityChangeEvent> visibility_change_events_;
  bool initially_hidden_;
  // Returns true if page was ever backgrounded in the range
  // [event_time, base::TimeTicks::Now()].
  bool PageWasBackgroundedSinceEvent(base::TimeTicks event_time);

  // Finds a window of length kTimeToInteractiveWindowSeconds after lower_bound
  // such that both main thread and network are quiet. Returns the end of last
  // long task before that quiet window, or lower_bound, whichever is bigger -
  // this is called the Interactive Candidate. Returns 0.0 if no such quiet
  // window is found.
  base::TimeTicks FindInteractiveCandidate(base::TimeTicks lower_bound,
                                           base::TimeTicks current_time);

  // LongTaskObserver implementation
  void OnLongTaskDetected(base::TimeTicks start_time,
                          base::TimeTicks end_time) override;

  // The duration between the hardware timestamp and when we received the event
  // for the previous pointer down. Only non-zero if we've received a pointer
  // down event, and haven't yet reported the first input delay.
  base::TimeDelta pending_pointerdown_delay_;
  // The timestamp of a pending pointerdown event. Valid in the same cases as
  // pending_pointerdown_delay_.
  base::TimeTicks pending_pointerdown_timestamp_;

  ukm::UkmRecorder* ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(InteractiveDetector);
};

}  // namespace blink

#endif
