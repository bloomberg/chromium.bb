/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PerformanceBase_h
#define PerformanceBase_h

#include "core/CoreExport.h"
#include "core/dom/DOMHighResTimeStamp.h"
#include "core/events/EventTarget.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/timing/PerformanceEntry.h"
#include "core/timing/PerformanceNavigationTiming.h"
#include "core/timing/PerformancePaintTiming.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/ListHashSet.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ExceptionState;
class PerformanceObserver;
class PerformanceTiming;
class ResourceResponse;
class ResourceTimingInfo;
class SecurityOrigin;
class UserTiming;

using PerformanceEntryVector = HeapVector<Member<PerformanceEntry>>;
using PerformanceObservers = HeapListHashSet<Member<PerformanceObserver>>;

class CORE_EXPORT PerformanceBase : public EventTargetWithInlineData {

 public:
  ~PerformanceBase() override;

  const AtomicString& InterfaceName() const override;

  virtual PerformanceTiming* timing() const;

  virtual void UpdateLongTaskInstrumentation() {}

  // Reduce the resolution to 5µs to prevent timing attacks. See:
  // http://www.w3.org/TR/hr-time-2/#privacy-security
  static double ClampTimeResolution(double time_seconds);

  static DOMHighResTimeStamp MonotonicTimeToDOMHighResTimeStamp(
      double time_origin,
      double monotonic_time,
      bool allow_negative_value);

  // Translate given platform monotonic time in seconds into a high resolution
  // DOMHighResTimeStamp in milliseconds. The result timestamp is relative to
  // document's time origin and has a time resolution that is safe for
  // exposing to web.
  DOMHighResTimeStamp MonotonicTimeToDOMHighResTimeStamp(double) const;
  DOMHighResTimeStamp now() const;

  double TimeOrigin() const { return time_origin_; }

  PerformanceEntryVector getEntries();
  PerformanceEntryVector getEntriesByType(const String& entry_type);
  PerformanceEntryVector getEntriesByName(const String& name,
                                          const String& entry_type);

  void clearResourceTimings();
  void setResourceTimingBufferSize(unsigned);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(resourcetimingbufferfull);

  void AddLongTaskTiming(double start_time,
                         double end_time,
                         const String& name,
                         const String& culprit_frame_src,
                         const String& culprit_frame_id,
                         const String& culprit_frame_name);

  void AddResourceTiming(const ResourceTimingInfo&);

  void NotifyNavigationTimingToObservers();

  void AddFirstPaintTiming(double start_time);

  void AddFirstContentfulPaintTiming(double start_time);

  void mark(const String& mark_name, ExceptionState&);
  void clearMarks(const String& mark_name);

  void measure(const String& measure_name,
               const String& start_mark,
               const String& end_mark,
               ExceptionState&);
  void clearMeasures(const String& measure_name);

  void UnregisterPerformanceObserver(PerformanceObserver&);
  void RegisterPerformanceObserver(PerformanceObserver&);
  void UpdatePerformanceObserverFilterOptions();
  void ActivateObserver(PerformanceObserver&);
  void ResumeSuspendedObservers();

  static bool AllowsTimingRedirect(const Vector<ResourceResponse>&,
                                   const ResourceResponse&,
                                   const SecurityOrigin&,
                                   ExecutionContext*);

  DECLARE_VIRTUAL_TRACE();

 private:
  static bool PassesTimingAllowCheck(const ResourceResponse&,
                                     const SecurityOrigin&,
                                     const AtomicString&,
                                     ExecutionContext*);

  void AddPaintTiming(PerformancePaintTiming::PaintType, double start_time);

 protected:
  explicit PerformanceBase(double time_origin, RefPtr<WebTaskRunner>);

  // Expect Performance to override this method,
  // WorkerPerformance doesn't have to override this.
  virtual PerformanceNavigationTiming* CreateNavigationTimingInstance() {
    return nullptr;
  }

  bool IsResourceTimingBufferFull();
  void AddResourceTimingBuffer(PerformanceEntry&);

  void NotifyObserversOfEntry(PerformanceEntry&);
  void NotifyObserversOfEntries(PerformanceEntryVector&);
  bool HasObserverFor(PerformanceEntry::EntryType) const;

  void DeliverObservationsTimerFired(TimerBase*);

  PerformanceEntryVector frame_timing_buffer_;
  unsigned frame_timing_buffer_size_;
  PerformanceEntryVector resource_timing_buffer_;
  unsigned resource_timing_buffer_size_;
  Member<PerformanceEntry> navigation_timing_;
  Member<UserTiming> user_timing_;
  Member<PerformanceEntry> first_paint_timing_;
  Member<PerformanceEntry> first_contentful_paint_timing_;

  double time_origin_;

  PerformanceEntryTypeMask observer_filter_options_;
  PerformanceObservers observers_;
  PerformanceObservers active_observers_;
  PerformanceObservers suspended_observers_;
  TaskRunnerTimer<PerformanceBase> deliver_observations_timer_;
};

}  // namespace blink

#endif  // PerformanceBase_h
