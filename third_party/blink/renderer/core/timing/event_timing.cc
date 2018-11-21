// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/event_timing.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

EventTiming::EventTiming(LocalDOMWindow* window) {
  performance_ = DOMWindowPerformance::performance(*window);
}

bool EventTiming::ShouldReportForEventTiming(const Event& event) const {
  return (event.IsMouseEvent() || event.IsPointerEvent() ||
          event.IsTouchEvent() || event.IsKeyboardEvent() ||
          event.IsWheelEvent() || event.IsInputEvent() ||
          event.IsCompositionEvent()) &&
         event.isTrusted();
}

bool EventTiming::ShouldLogEvent(const Event& event) const {
  return event.type() == event_type_names::kPointerdown ||
         event.type() == event_type_names::kPointerup ||
         event.type() == event_type_names::kClick ||
         event.type() == event_type_names::kKeydown ||
         event.type() == event_type_names::kMousedown;
}

void EventTiming::WillDispatchEvent(const Event& event) {
  // Assume each event can be dispatched only once.
  DCHECK(!finished_will_dispatch_event_);
  if (!performance_ || !ShouldReportForEventTiming(event))
    return;

  // |event_is_important| is used to keep track of whether this event merits
  // computing its processing start time. This should be the case in the
  // following scenarios: 1. When the event is relevant to the logs sent to
  // UMA/UKM/CrUX, as determined by ShouldLogEvent(). 2. When the EventTiming
  // API is enabled, the buffer is not full, and we're still buffering entries
  // (currently, before onload). 3. When the EventTiming API is enabled and
  // there is a PerformanceObserver that could listen to an entry caused by this
  // event.
  bool event_is_important = ShouldLogEvent(event);
  if (origin_trials::EventTimingEnabled(performance_->GetExecutionContext())) {
    // TODO(npm): Get rid of this counter or at least rename it once origin
    // trial is done.
    UseCounter::Count(performance_->GetExecutionContext(),
                      WebFeature::kPerformanceEventTimingConstructor);
    event_is_important |= performance_->ShouldBufferEventTiming() &&
                          !performance_->IsEventTimingBufferFull();
    event_is_important |=
        performance_->HasObserverFor(PerformanceEntry::kEvent);
    event_is_important |=
        performance_->HasObserverFor(PerformanceEntry::kFirstInput) &&
        !performance_->FirstInputDetected();
  }

  if (event_is_important) {
    processing_start_ = CurrentTimeTicks();
    finished_will_dispatch_event_ = true;
  }
}

void EventTiming::DidDispatchEvent(const Event& event) {
  if (!finished_will_dispatch_event_)
    return;

  TimeTicks event_timestamp;
  if (event.IsPointerEvent())
    event_timestamp = ToPointerEvent(&event)->OldestPlatformTimeStamp();
  else
    event_timestamp = event.PlatformTimeStamp();

  if (origin_trials::EventTimingEnabled(performance_->GetExecutionContext())) {
    performance_->RegisterEventTiming(event.type(), event_timestamp,
                                      processing_start_, CurrentTimeTicks(),
                                      event.cancelable());
  }

  Document* document = DynamicTo<Document>(performance_->GetExecutionContext());
  if (!document)
    return;

  InteractiveDetector* interactive_detector =
      InteractiveDetector::From(*document);
  if (interactive_detector) {
    interactive_detector->HandleForInputDelay(event, event_timestamp,
                                              processing_start_);
  }
}

}  // namespace blink
