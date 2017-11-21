// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/InteractiveDetector.h"

#include "core/dom/Document.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/wtf/Time.h"

namespace blink {

static constexpr char kSupplementName[] = "InteractiveDetector";

InteractiveDetector* InteractiveDetector::From(Document& document) {
  InteractiveDetector* detector = static_cast<InteractiveDetector*>(
      Supplement<Document>::From(document, kSupplementName));
  if (!detector) {
    if (!document.IsInMainFrame()) {
      return nullptr;
    }
    detector = new InteractiveDetector(document,
                                       new NetworkActivityChecker(&document));
    Supplement<Document>::ProvideTo(document, kSupplementName, detector);
  }
  return detector;
}

InteractiveDetector::InteractiveDetector(
    Document& document,
    NetworkActivityChecker* network_activity_checker)
    : Supplement<Document>(document),
      network_activity_checker_(network_activity_checker),
      time_to_interactive_timer_(
          document.GetTaskRunner(TaskType::kUnspecedTimer),
          this,
          &InteractiveDetector::TimeToInteractiveTimerFired) {}

InteractiveDetector::~InteractiveDetector() {
  LongTaskDetector::Instance().UnregisterObserver(this);
}

void InteractiveDetector::SetNavigationStartTime(double navigation_start_time) {
  // Should not set nav start twice.
  DCHECK(page_event_times_.nav_start == 0.0);

  LongTaskDetector::Instance().RegisterObserver(this);
  page_event_times_.nav_start = navigation_start_time;
  double initial_timer_fire_time =
      navigation_start_time + kTimeToInteractiveWindowSeconds;

  active_main_thread_quiet_window_start_ = navigation_start_time;
  active_network_quiet_window_start_ = navigation_start_time;
  StartOrPostponeCITimer(initial_timer_fire_time);
}

int InteractiveDetector::NetworkActivityChecker::GetActiveConnections() {
  DCHECK(document_);
  ResourceFetcher* fetcher = document_->Fetcher();
  return fetcher->BlockingRequestCount() + fetcher->NonblockingRequestCount();
}

int InteractiveDetector::ActiveConnections() {
  return network_activity_checker_->GetActiveConnections();
}

void InteractiveDetector::StartOrPostponeCITimer(double timer_fire_time) {
  // This function should never be called after Time To Interactive is
  // reached.
  DCHECK(interactive_time_ == 0.0);

  // We give 1ms extra padding to the timer fire time to prevent floating point
  // arithmetic pitfalls when comparing window sizes.
  timer_fire_time += 0.001;

  // Return if there is an active timer scheduled to fire later than
  // |timer_fire_time|.
  if (timer_fire_time < time_to_interactive_timer_fire_time_)
    return;

  double delay = timer_fire_time - MonotonicallyIncreasingTime();
  time_to_interactive_timer_fire_time_ = timer_fire_time;

  if (delay <= 0.0) {
    // This argument of this function is never used and only there to fulfill
    // the API contract. nullptr should work fine.
    TimeToInteractiveTimerFired(nullptr);
  } else {
    time_to_interactive_timer_.StartOneShot(delay, BLINK_FROM_HERE);
  }
}

double InteractiveDetector::GetInteractiveTime() {
  return interactive_time_;
}

void InteractiveDetector::BeginNetworkQuietPeriod(double current_time) {
  // Value of 0.0 indicates there is no currently actively network quiet window.
  DCHECK(active_network_quiet_window_start_ == 0.0);
  active_network_quiet_window_start_ = current_time;

  StartOrPostponeCITimer(current_time + kTimeToInteractiveWindowSeconds);
}

void InteractiveDetector::EndNetworkQuietPeriod(double current_time) {
  DCHECK(active_network_quiet_window_start_ != 0.0);

  if (current_time - active_network_quiet_window_start_ >=
      kTimeToInteractiveWindowSeconds) {
    network_quiet_windows_.emplace_back(active_network_quiet_window_start_,
                                        current_time);
  }
  active_network_quiet_window_start_ = 0.0;
}

// The optional opt_current_time, if provided, saves us a call to
// MonotonicallyIncreasingTime.
void InteractiveDetector::UpdateNetworkQuietState(
    double request_count,
    WTF::Optional<double> opt_current_time) {
  if (request_count <= kNetworkQuietMaximumConnections &&
      active_network_quiet_window_start_ == 0.0) {
    // Not using `value_or(MonotonicallyIncreasingTime())` here because
    // arguments to functions are eagerly evaluated, which always call
    // MonotonicallyIncreasingTime.
    double current_time = opt_current_time ? opt_current_time.value()
                                           : MonotonicallyIncreasingTime();
    BeginNetworkQuietPeriod(current_time);
  } else if (request_count > kNetworkQuietMaximumConnections &&
             active_network_quiet_window_start_ != 0.0) {
    double current_time = opt_current_time ? opt_current_time.value()
                                           : MonotonicallyIncreasingTime();
    EndNetworkQuietPeriod(current_time);
  }
}

void InteractiveDetector::OnResourceLoadBegin(
    WTF::Optional<double> load_begin_time) {
  if (!GetSupplementable())
    return;
  if (interactive_time_ != 0.0)
    return;
  // The request that is about to begin is not counted in ActiveConnections(),
  // so we add one to it.
  UpdateNetworkQuietState(ActiveConnections() + 1, load_begin_time);
}

// The optional load_finish_time, if provided, saves us a call to
// MonotonicallyIncreasingTime.
void InteractiveDetector::OnResourceLoadEnd(
    WTF::Optional<double> load_finish_time) {
  if (!GetSupplementable())
    return;
  if (interactive_time_ != 0.0)
    return;
  UpdateNetworkQuietState(ActiveConnections(), load_finish_time);
}

void InteractiveDetector::OnLongTaskDetected(double start_time,
                                             double end_time) {
  // We should not be receiving long task notifications after Time to
  // Interactive has already been reached.
  DCHECK(interactive_time_ == 0.0);
  double quiet_window_length =
      start_time - active_main_thread_quiet_window_start_;
  if (quiet_window_length >= kTimeToInteractiveWindowSeconds) {
    main_thread_quiet_windows_.emplace_back(
        active_main_thread_quiet_window_start_, start_time);
  }
  active_main_thread_quiet_window_start_ = end_time;
  StartOrPostponeCITimer(end_time + kTimeToInteractiveWindowSeconds);
}

void InteractiveDetector::OnFirstMeaningfulPaintDetected(double fmp_time) {
  DCHECK(page_event_times_.first_meaningful_paint ==
         0.0);  // Should not set FMP twice.
  page_event_times_.first_meaningful_paint = fmp_time;
  if (MonotonicallyIncreasingTime() - fmp_time >=
      kTimeToInteractiveWindowSeconds) {
    // We may have reached TTCI already. Check right away.
    CheckTimeToInteractiveReached();
  } else {
    StartOrPostponeCITimer(page_event_times_.first_meaningful_paint +
                           kTimeToInteractiveWindowSeconds);
  }
}

void InteractiveDetector::OnDomContentLoadedEnd(double dcl_end_time) {
  // InteractiveDetector should only receive the first DCL event.
  DCHECK(page_event_times_.dom_content_loaded_end == 0.0);
  page_event_times_.dom_content_loaded_end = dcl_end_time;
  CheckTimeToInteractiveReached();
}

void InteractiveDetector::TimeToInteractiveTimerFired(TimerBase*) {
  if (!GetSupplementable() || interactive_time_ != 0.0)
    return;

  // Value of 0.0 indicates there is currently no active timer.
  time_to_interactive_timer_fire_time_ = 0.0;
  CheckTimeToInteractiveReached();
}

void InteractiveDetector::AddCurrentlyActiveQuietIntervals(
    double current_time) {
  // Network is currently quiet.
  if (active_network_quiet_window_start_ != 0.0) {
    if (current_time - active_network_quiet_window_start_ >=
        kTimeToInteractiveWindowSeconds) {
      network_quiet_windows_.emplace_back(active_network_quiet_window_start_,
                                          current_time);
    }
  }

  // Since this code executes on the main thread, we know that no task is
  // currently running on the main thread. We can therefore skip checking.
  // main_thread_quiet_window_being != 0.0.
  if (current_time - active_main_thread_quiet_window_start_ >=
      kTimeToInteractiveWindowSeconds) {
    main_thread_quiet_windows_.emplace_back(
        active_main_thread_quiet_window_start_, current_time);
  }
}

void InteractiveDetector::RemoveCurrentlyActiveQuietIntervals() {
  if (!network_quiet_windows_.empty() &&
      network_quiet_windows_.back().Low() ==
          active_network_quiet_window_start_) {
    network_quiet_windows_.pop_back();
  }

  if (!main_thread_quiet_windows_.empty() &&
      main_thread_quiet_windows_.back().Low() ==
          active_main_thread_quiet_window_start_) {
    main_thread_quiet_windows_.pop_back();
  }
}

double InteractiveDetector::FindInteractiveCandidate(double lower_bound) {
  // Main thread iterator.
  auto it_mt = main_thread_quiet_windows_.begin();
  // Network iterator.
  auto it_net = network_quiet_windows_.begin();

  while (it_mt < main_thread_quiet_windows_.end() &&
         it_net < network_quiet_windows_.end()) {
    if (it_mt->High() <= lower_bound) {
      it_mt++;
      continue;
    }
    if (it_net->High() <= lower_bound) {
      it_net++;
      continue;
    }

    // First handling the no overlap cases.
    // [ main thread interval ]
    //                                     [ network interval ]
    if (it_mt->High() <= it_net->Low()) {
      it_mt++;
      continue;
    }
    //                                     [ main thread interval ]
    // [   network interval   ]
    if (it_net->High() <= it_mt->Low()) {
      it_net++;
      continue;
    }

    // At this point we know we have a non-empty overlap after lower_bound.
    double overlap_start = std::max({it_mt->Low(), it_net->Low(), lower_bound});
    double overlap_end = std::min(it_mt->High(), it_net->High());
    double overlap_duration = overlap_end - overlap_start;
    if (overlap_duration >= kTimeToInteractiveWindowSeconds) {
      return std::max(lower_bound, it_mt->Low());
    }

    // The interval with earlier end time will not produce any more overlap, so
    // we move on from it.
    if (it_mt->High() <= it_net->High()) {
      it_mt++;
    } else {
      it_net++;
    }
  }

  // Time To Interactive candidate not found.
  return 0.0;
}

void InteractiveDetector::CheckTimeToInteractiveReached() {
  // Already detected Time to Interactive.
  if (interactive_time_ != 0.0)
    return;

  // FMP and DCL have not been detected yet.
  if (page_event_times_.first_meaningful_paint == 0.0 ||
      page_event_times_.dom_content_loaded_end == 0.0)
    return;

  const double current_time = MonotonicallyIncreasingTime();
  if (current_time - page_event_times_.first_meaningful_paint <
      kTimeToInteractiveWindowSeconds) {
    // Too close to FMP to determine Time to Interactive.
    return;
  }

  AddCurrentlyActiveQuietIntervals(current_time);
  const double interactive_candidate =
      FindInteractiveCandidate(page_event_times_.first_meaningful_paint);
  RemoveCurrentlyActiveQuietIntervals();

  // No Interactive Candidate found.
  if (interactive_candidate == 0.0)
    return;

  interactive_time_ = std::max(
      {interactive_candidate, page_event_times_.dom_content_loaded_end});
  OnTimeToInteractiveDetected();
}

void InteractiveDetector::OnTimeToInteractiveDetected() {
  LongTaskDetector::Instance().UnregisterObserver(this);
  main_thread_quiet_windows_.clear();
  network_quiet_windows_.clear();

  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "loading,rail", "InteractiveTime",
      TraceEvent::ToTraceTimestamp(interactive_time_), "frame",
      GetSupplementable()->GetFrame());
}

void InteractiveDetector::Trace(Visitor* visitor) {
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
