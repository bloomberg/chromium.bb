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

#include "third_party/blink/renderer/core/timing/window_performance.h"

#include "base/trace_event/common/trace_event_common.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/input_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_hidden_state.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/layout_shift.h"
#include "third_party/blink/renderer/core/timing/performance_element_timing.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_observer.h"
#include "third_party/blink/renderer/core/timing/performance_timing.h"
#include "third_party/blink/renderer/core/timing/responsiveness_metrics.h"
#include "third_party/blink/renderer/core/timing/visibility_state_entry.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

static constexpr base::TimeDelta kLongTaskObserverThreshold =
    base::Milliseconds(50);

namespace blink {

namespace {
// Minimum potential value for the first Interaction ID.
constexpr uint32_t kMinFirstInteractionID = 100;
// Maximum potential value for the first Interaction ID.
constexpr uint32_t kMaxFirstInteractionID = 10000;
// Interaction ID increment.
constexpr uint32_t kInteractionIdIncrement = 7;
// The maximum number of pending pointer down.
constexpr WTF::wtf_size_t kMaxPendingPointerDownNumber = 20;
// The maximum tap delay we can handle for assigning interaction id.
constexpr blink::DOMHighResTimeStamp kMaxDelayForEntries =
    blink::DOMHighResTimeStamp(500);

AtomicString GetFrameAttribute(HTMLFrameOwnerElement* frame_owner,
                               const QualifiedName& attr_name) {
  AtomicString attr_value;
  if (frame_owner->hasAttribute(attr_name)) {
    attr_value = frame_owner->getAttribute(attr_name);
  }
  return attr_value;
}

AtomicString GetFrameOwnerType(HTMLFrameOwnerElement* frame_owner) {
  switch (frame_owner->OwnerType()) {
    case FrameOwnerElementType::kNone:
      return "window";
    case FrameOwnerElementType::kIframe:
      return "iframe";
    case FrameOwnerElementType::kObject:
      return "object";
    case FrameOwnerElementType::kEmbed:
      return "embed";
    case FrameOwnerElementType::kFrame:
      return "frame";
    case FrameOwnerElementType::kPortal:
      return "portal";
    case FrameOwnerElementType::kFencedframe:
      return "fencedframe";
  }
  NOTREACHED();
  return "";
}

AtomicString GetFrameSrc(HTMLFrameOwnerElement* frame_owner) {
  switch (frame_owner->OwnerType()) {
    case FrameOwnerElementType::kObject:
      return GetFrameAttribute(frame_owner, html_names::kDataAttr);
    default:
      return GetFrameAttribute(frame_owner, html_names::kSrcAttr);
  }
}

const AtomicString& SelfKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, kSelfAttribution, ("self"));
  return kSelfAttribution;
}

const AtomicString& SameOriginAncestorKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, kSameOriginAncestorAttribution,
                      ("same-origin-ancestor"));
  return kSameOriginAncestorAttribution;
}

const AtomicString& SameOriginDescendantKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, kSameOriginDescendantAttribution,
                      ("same-origin-descendant"));
  return kSameOriginDescendantAttribution;
}

const AtomicString& SameOriginKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, kSameOriginAttribution,
                      ("same-origin"));
  return kSameOriginAttribution;
}

AtomicString SameOriginAttribution(Frame* observer_frame,
                                   Frame* culprit_frame) {
  DCHECK(IsMainThread());
  if (observer_frame == culprit_frame)
    return SelfKeyword();
  if (observer_frame->Tree().IsDescendantOf(culprit_frame))
    return SameOriginAncestorKeyword();
  if (culprit_frame->Tree().IsDescendantOf(observer_frame))
    return SameOriginDescendantKeyword();
  return SameOriginKeyword();
}

}  // namespace

constexpr size_t kDefaultVisibilityStateEntrySize = 50;

static base::TimeTicks ToTimeOrigin(LocalDOMWindow* window) {
  DocumentLoader* loader = window->GetFrame()->Loader().GetDocumentLoader();
  return loader->GetTiming().ReferenceMonotonicTime();
}

WindowPerformance::WindowPerformance(LocalDOMWindow* window)
    : Performance(ToTimeOrigin(window),
                  window->CrossOriginIsolatedCapability(),
                  window->GetTaskRunner(TaskType::kPerformanceTimeline),
                  window),
      ExecutionContextClient(window),
      PageVisibilityObserver(window->GetFrame()->GetPage()),
      current_interaction_id_for_event_timing_(
          base::RandInt(kMinFirstInteractionID, kMaxFirstInteractionID)) {
  DCHECK(window);
  DCHECK(window->GetFrame()->GetPerformanceMonitor());
  window->GetFrame()->GetPerformanceMonitor()->Subscribe(
      PerformanceMonitor::kLongTask, kLongTaskObserverThreshold, this);
  if (RuntimeEnabledFeatures::VisibilityStateEntryEnabled()) {
    DCHECK(GetPage());
    AddVisibilityStateEntry(GetPage()->IsPageVisible(), base::TimeTicks());
  }
}

void WindowPerformance::EventData::Trace(Visitor* visitor) const {
  visitor->Trace(event_timing_);
}

void WindowPerformance::KeyboardEntryAndTimestamps::Trace(
    Visitor* visitor) const {
  visitor->Trace(entry_);
}

WindowPerformance::~WindowPerformance() = default;

ExecutionContext* WindowPerformance::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

PerformanceTiming* WindowPerformance::timing() const {
  if (!timing_)
    timing_ = MakeGarbageCollected<PerformanceTiming>(DomWindow());

  return timing_.Get();
}

PerformanceNavigation* WindowPerformance::navigation() const {
  if (!navigation_)
    navigation_ = MakeGarbageCollected<PerformanceNavigation>(DomWindow());

  return navigation_.Get();
}

MemoryInfo* WindowPerformance::memory(ScriptState* script_state) const {
  // The performance.memory() API has been improved so that we report precise
  // values when the process is locked to a site. The intent (which changed
  // course over time about what changes would be implemented) can be found at
  // https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/no00RdMnGio,
  // and the relevant bug is https://crbug.com/807651.
  auto* memory_info =
      MakeGarbageCollected<MemoryInfo>(Platform::Current()->IsLockedToSite()
                                           ? MemoryInfo::Precision::Precise
                                           : MemoryInfo::Precision::Bucketized);
  // Record Web Memory UKM.
  const uint64_t kBytesInKB = 1024;
  auto* execution_context = ExecutionContext::From(script_state);
  ukm::builders::PerformanceAPI_Memory_Legacy(execution_context->UkmSourceID())
      .SetJavaScript(memory_info->usedJSHeapSize() / kBytesInKB)
      .Record(execution_context->UkmRecorder());
  return memory_info;
}

PerformanceNavigationTiming*
WindowPerformance::CreateNavigationTimingInstance() {
  if (!DomWindow())
    return nullptr;
  DocumentLoader* document_loader = DomWindow()->document()->Loader();
  // TODO(npm): figure out when |document_loader| can be null and add tests.
  DCHECK(document_loader);
  if (!document_loader)
    return nullptr;
  ResourceTimingInfo* info = document_loader->GetNavigationTimingInfo();
  if (!info)
    return nullptr;
  HeapVector<Member<PerformanceServerTiming>> server_timing =
      PerformanceServerTiming::ParseServerTiming(*info);
  if (!server_timing.IsEmpty())
    document_loader->CountUse(WebFeature::kPerformanceServerTiming);

  return MakeGarbageCollected<PerformanceNavigationTiming>(
      DomWindow(), info, time_origin_,
      DomWindow()->CrossOriginIsolatedCapability(), std::move(server_timing));
}

void WindowPerformance::BuildJSONValue(V8ObjectBuilder& builder) const {
  Performance::BuildJSONValue(builder);
  builder.Add("timing", timing());
  builder.Add("navigation", navigation());
}

void WindowPerformance::Trace(Visitor* visitor) const {
  visitor->Trace(events_data_);
  visitor->Trace(first_pointer_down_event_timing_);
  visitor->Trace(event_counts_);
  visitor->Trace(navigation_);
  visitor->Trace(timing_);
  visitor->Trace(current_event_);
  visitor->Trace(pointer_id_pointer_down_map_);
  visitor->Trace(key_code_entry_map_);
  Performance::Trace(visitor);
  PerformanceMonitor::Client::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
}

static bool CanAccessOrigin(Frame* frame1, Frame* frame2) {
  const SecurityOrigin* security_origin1 =
      frame1->GetSecurityContext()->GetSecurityOrigin();
  const SecurityOrigin* security_origin2 =
      frame2->GetSecurityContext()->GetSecurityOrigin();
  return security_origin1->CanAccess(security_origin2);
}

/**
 * Report sanitized name based on cross-origin policy.
 * See detailed Security doc here: http://bit.ly/2duD3F7
 */
// static
std::pair<AtomicString, DOMWindow*> WindowPerformance::SanitizedAttribution(
    ExecutionContext* task_context,
    bool has_multiple_contexts,
    LocalFrame* observer_frame) {
  DCHECK(IsMainThread());
  if (has_multiple_contexts) {
    // Unable to attribute, multiple script execution contents were involved.
    DEFINE_STATIC_LOCAL(const AtomicString, kAmbiguousAttribution,
                        ("multiple-contexts"));
    return std::make_pair(kAmbiguousAttribution, nullptr);
  }

  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(task_context);
  if (!window || !window->GetFrame()) {
    // Unable to attribute as no script was involved.
    DEFINE_STATIC_LOCAL(const AtomicString, kUnknownAttribution, ("unknown"));
    return std::make_pair(kUnknownAttribution, nullptr);
  }

  // Exactly one culprit location, attribute based on origin boundary.
  Frame* culprit_frame = window->GetFrame();
  DCHECK(culprit_frame);
  if (CanAccessOrigin(observer_frame, culprit_frame)) {
    // From accessible frames or same origin, return culprit location URL.
    return std::make_pair(SameOriginAttribution(observer_frame, culprit_frame),
                          culprit_frame->DomWindow());
  }
  // For cross-origin, if the culprit is the descendant or ancestor of
  // observer then indicate the *closest* cross-origin frame between
  // the observer and the culprit, in the corresponding direction.
  if (culprit_frame->Tree().IsDescendantOf(observer_frame)) {
    // If the culprit is a descendant of the observer, then walk up the tree
    // from culprit to observer, and report the *last* cross-origin (from
    // observer) frame.  If no intermediate cross-origin frame is found, then
    // report the culprit directly.
    Frame* last_cross_origin_frame = culprit_frame;
    for (Frame* frame = culprit_frame; frame != observer_frame;
         frame = frame->Tree().Parent()) {
      if (!CanAccessOrigin(observer_frame, frame)) {
        last_cross_origin_frame = frame;
      }
    }
    DEFINE_STATIC_LOCAL(const AtomicString, kCrossOriginDescendantAttribution,
                        ("cross-origin-descendant"));
    return std::make_pair(kCrossOriginDescendantAttribution,
                          last_cross_origin_frame->DomWindow());
  }
  if (observer_frame->Tree().IsDescendantOf(culprit_frame)) {
    DEFINE_STATIC_LOCAL(const AtomicString, kCrossOriginAncestorAttribution,
                        ("cross-origin-ancestor"));
    return std::make_pair(kCrossOriginAncestorAttribution, nullptr);
  }
  DEFINE_STATIC_LOCAL(const AtomicString, kCrossOriginAttribution,
                      ("cross-origin-unreachable"));
  return std::make_pair(kCrossOriginAttribution, nullptr);
}

void WindowPerformance::ReportLongTask(base::TimeTicks start_time,
                                       base::TimeTicks end_time,
                                       ExecutionContext* task_context,
                                       bool has_multiple_contexts) {
  if (!DomWindow())
    return;
  std::pair<AtomicString, DOMWindow*> attribution =
      WindowPerformance::SanitizedAttribution(
          task_context, has_multiple_contexts, DomWindow()->GetFrame());
  DOMWindow* culprit_dom_window = attribution.second;
  if (!culprit_dom_window || !culprit_dom_window->GetFrame() ||
      !culprit_dom_window->GetFrame()->DeprecatedLocalOwner()) {
    AddLongTaskTiming(start_time, end_time, attribution.first, "window",
                      g_empty_atom, g_empty_atom, g_empty_atom);
  } else {
    HTMLFrameOwnerElement* frame_owner =
        culprit_dom_window->GetFrame()->DeprecatedLocalOwner();
    AddLongTaskTiming(start_time, end_time, attribution.first,
                      GetFrameOwnerType(frame_owner), GetFrameSrc(frame_owner),
                      GetFrameAttribute(frame_owner, html_names::kIdAttr),
                      GetFrameAttribute(frame_owner, html_names::kNameAttr));
  }
}

void WindowPerformance::RegisterEventTiming(const Event& event,
                                            base::TimeTicks start_time,
                                            base::TimeTicks processing_start,
                                            base::TimeTicks processing_end) {
  // |start_time| could be null in some tests that inject input.
  DCHECK(!processing_start.is_null());
  DCHECK(!processing_end.is_null());
  DCHECK_GE(processing_end, processing_start);
  if (!DomWindow())
    return;

  const AtomicString& event_type = event.type();
  if (event_type == event_type_names::kPointermove) {
    NotifyPotentialDrag();
    SetCurrentEventTimingEvent(nullptr);
    return;
  }
  eventCounts()->Add(event_type);
  absl::optional<PointerId> pointer_id;
  const PointerEvent* pointer_event = DynamicTo<PointerEvent>(event);
  if (pointer_event)
    pointer_id = pointer_event->pointerId();
  PerformanceEventTiming* entry = PerformanceEventTiming::Create(
      event_type, MonotonicTimeToDOMHighResTimeStamp(start_time),
      MonotonicTimeToDOMHighResTimeStamp(processing_start),
      MonotonicTimeToDOMHighResTimeStamp(processing_end), event.cancelable(),
      event.target() ? event.target()->ToNode() : nullptr);
  absl::optional<int> key_code;
  if (event.IsKeyboardEvent())
    key_code = DynamicTo<KeyboardEvent>(event)->keyCode();
  // Add |entry| to the end of the queue along with the frame index at which is
  // is being queued to know when to queue a presentation promise for it.
  events_data_.push_back(
      EventData::Create(entry, frame_index_, start_time, key_code, pointer_id));
  bool should_queue_presentation_promise = false;
  // If there are no pending presentation promises, we should queue one. This
  // ensures that |event_timings_| are processed even if the Blink lifecycle
  // does not occur due to no DOM updates.
  if (pending_presentation_promise_count_ == 0u) {
    should_queue_presentation_promise = true;
  } else {
    // There are pending presentation promises, so only queue one if the event
    // corresponds to a later frame than the one of the latest queued
    // presentation promise.
    should_queue_presentation_promise =
        frame_index_ > last_registered_frame_index_;
  }
  if (should_queue_presentation_promise) {
    DomWindow()->GetFrame()->GetChromeClient().NotifyPresentationTime(
        *DomWindow()->GetFrame(),
        CrossThreadBindOnce(&WindowPerformance::ReportEventTimings,
                            WrapCrossThreadWeakPersistent(this), frame_index_));
    last_registered_frame_index_ = frame_index_;
    ++pending_presentation_promise_count_;
  }
  SetCurrentEventTimingEvent(nullptr);
}

void WindowPerformance::ReportEventTimings(
    uint64_t frame_index,
    WebSwapResult result,
    base::TimeTicks presentation_timestamp) {
  DCHECK(pending_presentation_promise_count_);
  --pending_presentation_promise_count_;
  if (events_data_.IsEmpty())
    return;

  if (!DomWindow() || !DomWindow()->document())
    return;
  InteractiveDetector* interactive_detector =
      InteractiveDetector::From(*(DomWindow()->document()));
  DOMHighResTimeStamp end_time =
      MonotonicTimeToDOMHighResTimeStamp(presentation_timestamp);
  while (!events_data_.IsEmpty()) {
    auto event_data = events_data_.front();
    PerformanceEventTiming* entry = event_data->GetEventTiming();
    uint64_t entry_frame_index = event_data->GetFrameIndex();
    base::TimeTicks event_timestamp = event_data->GetEventTimestamp();
    absl::optional<int> key_code = event_data->GetKeyCode();
    absl::optional<PointerId> pointer_id = event_data->GetPointerId();
    // If the entry was queued at a frame index that is larger than
    // |frame_index|, then we've reached the end of the entries that we can
    // process during this callback.
    if (entry_frame_index > frame_index)
      break;

    events_data_.pop_front();

    int duration_in_ms = std::round((end_time - entry->startTime()) / 8) * 8;
    base::TimeDelta input_delay =
        base::Milliseconds(entry->processingStart() - entry->startTime());
    base::TimeDelta processing_time =
        base::Milliseconds(entry->processingEnd() - entry->processingStart());
    base::TimeDelta time_to_next_paint =
        base::Milliseconds(end_time - entry->processingEnd());
    entry->SetDuration(duration_in_ms);
    TRACE_EVENT2("devtools.timeline", "EventTiming", "data",
                 entry->ToTracedValue(), "frame",
                 ToTraceValue(DomWindow()->GetFrame()));
    if (entry->name() == "pointerdown") {
      pending_pointer_down_input_delay_ = input_delay;
      pending_pointer_down_processing_time_ = processing_time;
      pending_pointer_down_time_to_next_paint_ = time_to_next_paint;
    } else if (entry->name() == "pointerup") {
      if (pending_pointer_down_time_to_next_paint_.has_value() &&
          interactive_detector) {
        interactive_detector->RecordInputEventTimingUKM(
            pending_pointer_down_input_delay_.value(),
            pending_pointer_down_processing_time_.value(),
            pending_pointer_down_time_to_next_paint_.value(), entry->name());
      }
    } else if ((entry->name() == "click" || entry->name() == "keydown" ||
                entry->name() == "mousedown") &&
               interactive_detector) {
      interactive_detector->RecordInputEventTimingUKM(
          input_delay, processing_time, time_to_next_paint, entry->name());
    }

    if (!first_input_timing_) {
      if (entry->name() == "pointerdown") {
        first_pointer_down_event_timing_ =
            PerformanceEventTiming::CreateFirstInputTiming(entry);
      } else if (entry->name() == "pointerup") {
        DispatchFirstInputTiming(first_pointer_down_event_timing_);
      } else if (entry->name() == "click" || entry->name() == "keydown" ||
                 entry->name() == "mousedown") {
        DispatchFirstInputTiming(
            PerformanceEventTiming::CreateFirstInputTiming(entry));
      }
    }
    ResponsivenessMetrics::EventTimestamps event_timestamps = {
        event_timestamp, presentation_timestamp};
    // The page visibility was changed. In this case, we don't care about
    // the time to next paint.
    if (last_visibility_change_timestamp_ > event_timestamp &&
        last_visibility_change_timestamp_ <= presentation_timestamp) {
      event_timestamps.end_time -= time_to_next_paint;
    }
    if (!SetInteractionIdAndRecordLatency(entry, key_code, pointer_id,
                                          event_timestamps)) {
      continue;
    }

    NotifyAndAddEventTimingBuffer(entry);
  }

  if (!RuntimeEnabledFeatures::InteractionIdEnabled(GetExecutionContext()))
    return;
  // Clear the map if there are too many pending pointer down entries.
  if (pointer_id_pointer_down_map_.size() > kMaxPendingPointerDownNumber) {
    blink::DOMHighResTimeStamp current_time =
        MonotonicTimeToDOMHighResTimeStamp(base::TimeTicks::Now());
    // We cannot delete from a HashMap while iterating.
    Vector<PointerId> pointer_ids_to_remove;
    for (const auto& pointer_id_pointer_down : pointer_id_pointer_down_map_) {
      PerformanceEventTiming* pointer_down = pointer_id_pointer_down.value;
      if ((current_time - pointer_down->startTime() -
           pointer_down->duration()) > kMaxDelayForEntries) {
        pointer_ids_to_remove.push_back(pointer_id_pointer_down.key);
      }
    }
    pointer_id_pointer_down_map_.RemoveAll(pointer_ids_to_remove);
  }
  // We cannot delete from a HashMap while iterating.
  Vector<int> key_codes_to_remove;
  for (const auto& entry : key_code_entry_map_) {
    PerformanceEventTiming* key_down = entry.value->GetEntry();
    // Use |end_time| as a proxy for the current timestamp.
    if (end_time - key_down->processingEnd() > kMaxDelayForEntries) {
      NotifyAndAddEventTimingBuffer(key_down);
      key_codes_to_remove.push_back(entry.key);
    }
  }
  key_code_entry_map_.RemoveAll(key_codes_to_remove);
}

void WindowPerformance::NotifyAndAddEventTimingBuffer(
    PerformanceEventTiming* entry) {
  if (HasObserverFor(PerformanceEntry::kEvent)) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kEventTimingExplicitlyRequested);
    NotifyObserversOfEntry(*entry);
  }
  // TODO(npm): is 104 a reasonable buffering threshold or should it be
  // relaxed?
  if (entry->duration() >= PerformanceObserver::kDefaultDurationThreshold &&
      !IsEventTimingBufferFull()) {
    AddEventTimingBuffer(*entry);
  }
}

void WindowPerformance::MaybeNotifyInteractionAndAddEventTimingBuffer(
    PerformanceEventTiming* entry) {
  if (!RuntimeEnabledFeatures::InteractionIdEnabled(GetExecutionContext())) {
    return;
  }

  NotifyAndAddEventTimingBuffer(entry);
}

void WindowPerformance::UpdateInteractionId() {
  current_interaction_id_for_event_timing_ += kInteractionIdIncrement;
}

uint32_t WindowPerformance::GetCurrentInteractionId() const {
  return current_interaction_id_for_event_timing_;
}

bool WindowPerformance::SetInteractionIdAndRecordLatency(
    PerformanceEventTiming* entry,
    absl::optional<int> key_code,
    absl::optional<PointerId> pointer_id,
    ResponsivenessMetrics::EventTimestamps event_timestamps) {
  if (pointer_id.has_value()) {
    return SetPointerIdAndRecordLatency(entry, *pointer_id, event_timestamps);
  }
  // For keyboard events, we set the interactionId and record the metric in the
  // same logic, so we need to ignore the return value when InteractionId is
  // disabled.
  return SetKeyIdAndRecordLatency(entry, key_code, event_timestamps) ||
         !RuntimeEnabledFeatures::InteractionIdEnabled(GetExecutionContext());
}

bool WindowPerformance::SetPointerIdAndRecordLatency(
    PerformanceEventTiming* entry,
    PointerId pointer_id,
    ResponsivenessMetrics::EventTimestamps event_timestamps) {
  // TODO(npm): merge the logic on RecordTapOrClickOrDrag() with the logic
  // computing interactionIds for pointer events once interactionId ships and is
  // not behind a flag.
  responsiveness_metrics_.RecordTapOrClickOrDrag(DomWindow(), entry->name(),
                                                 event_timestamps);
  if (!RuntimeEnabledFeatures::InteractionIdEnabled(GetExecutionContext())) {
    return true;
  }
  auto event_type = entry->name();
  if (event_type == event_type_names::kPointercancel &&
      pointer_id_pointer_down_map_.Contains(pointer_id)) {
    // Flush the pointer down entry.
    NotifyAndAddEventTimingBuffer(pointer_id_pointer_down_map_.at(pointer_id));
    // The pointer id of the pointerdown is no longer needed.
    pointer_id_pointer_down_map_.erase(pointer_id);
  } else if (event_type == event_type_names::kPointerdown) {
    if (pointer_id_pointer_down_map_.Contains(pointer_id)) {
      NotifyAndAddEventTimingBuffer(
          pointer_id_pointer_down_map_.at(pointer_id));
    }
    pointer_id_pointer_down_map_.Set(pointer_id, entry);
    // Waiting to see if we get a pointercancel or pointerup.
    return false;
  } else if (event_type == event_type_names::kPointerup &&
             pointer_id_pointer_down_map_.Contains(pointer_id)) {
    PerformanceEventTiming* pointer_down_entry =
        pointer_id_pointer_down_map_.at(pointer_id);
    // Generate a new interaction id.
    UpdateInteractionId();
    pointer_down_entry->SetInteractionId(GetCurrentInteractionId());
    entry->SetInteractionId(pointer_down_entry->interactionId());
    // Flush the pointer down entry.
    NotifyAndAddEventTimingBuffer(pointer_down_entry);
  } else if (event_type == event_type_names::kClick &&
             pointer_id_pointer_down_map_.Contains(pointer_id)) {
    PerformanceEventTiming* pointer_down_entry =
        pointer_id_pointer_down_map_.at(pointer_id);
    entry->SetInteractionId(pointer_down_entry->interactionId());
    // The pointer id of the pointerdown is no longer needed.
    pointer_id_pointer_down_map_.erase(pointer_id);
  }
  return true;
}

bool WindowPerformance::SetKeyIdAndRecordLatency(
    PerformanceEventTiming* entry,
    absl::optional<int> key_code,
    ResponsivenessMetrics::EventTimestamps event_timestamps) {
  auto event_type = entry->name();
  if (event_type == event_type_names::kKeydown) {
    DCHECK(key_code.has_value());
    // During compositions, we ignore keydowns/keyups and look at input events.
    if (composition_started_)
      return true;

    if (key_code_entry_map_.Contains(*key_code)) {
      auto* previous_entry = key_code_entry_map_.at(*key_code);
      // Ignore repeat IME keydowns. See
      // https://w3c.github.io/uievents/#determine-keydown-keyup-keyCode.
      // Reasoning: we cannot ignore all IME keydowns because on Android in some
      // languages the events received are 'keydown', 'input', 'keyup', and
      // since we are not composing then the 'input' event is ignored, so we
      // must consider the key events with 229 keyCode as the user interaction.
      // Besides this, we cannot consider repeat 229 keydowns because we may get
      // those on ChromeOS when we should ignore them. This may be related to
      // crbug.com/1252856.
      if (*key_code != 229) {
        // Generate a new interaction id for |previous_entry|. This case could
        // be caused by keeping a key pressed for a while.
        UpdateInteractionId();
        previous_entry->GetEntry()->SetInteractionId(GetCurrentInteractionId());
        responsiveness_metrics_.RecordKeyboardInteractions(
            DomWindow(), {previous_entry->GetTimeStamps()});
      }
      MaybeNotifyInteractionAndAddEventTimingBuffer(previous_entry->GetEntry());
    }
    key_code_entry_map_.Set(
        *key_code, KeyboardEntryAndTimestamps::Create(entry, event_timestamps));
    // Similar to pointerdown, we need to wait a bit before knowing the
    // interactionId of keydowns.
    return false;
  } else if (event_type == event_type_names::kKeyup) {
    DCHECK(key_code.has_value());
    if (composition_started_ || !key_code_entry_map_.Contains(*key_code))
      return true;

    auto* previous_entry = key_code_entry_map_.at(*key_code);
    // Generate a new interaction id for the keydown-keyup pair.
    UpdateInteractionId();
    previous_entry->GetEntry()->SetInteractionId(GetCurrentInteractionId());
    MaybeNotifyInteractionAndAddEventTimingBuffer(previous_entry->GetEntry());
    key_code_entry_map_.erase(*key_code);
    entry->SetInteractionId(GetCurrentInteractionId());
    responsiveness_metrics_.RecordKeyboardInteractions(
        DomWindow(), {previous_entry->GetTimeStamps(), event_timestamps});
  } else if (event_type == event_type_names::kCompositionstart) {
    composition_started_ = true;
    for (auto key_entry : key_code_entry_map_.Values()) {
      MaybeNotifyInteractionAndAddEventTimingBuffer(key_entry->GetEntry());
    }
    key_code_entry_map_.clear();
  } else if (event_type == event_type_names::kCompositionend) {
    composition_started_ = false;
  } else if (event_type == event_type_names::kInput) {
    if (!composition_started_) {
      return true;
    }
    // We are in the case of a text input event while compositing with
    // non-trivial data, so we want to increase interactionId.
    // TODO(crbug.com/1252856): fix counts in ChromeOS due to duplicate events.
    UpdateInteractionId();
    entry->SetInteractionId(GetCurrentInteractionId());
    responsiveness_metrics_.RecordKeyboardInteractions(DomWindow(),
                                                       {event_timestamps});
  }
  return true;
}

void WindowPerformance::AddElementTiming(const AtomicString& name,
                                         const String& url,
                                         const FloatRect& rect,
                                         base::TimeTicks start_time,
                                         base::TimeTicks load_time,
                                         const AtomicString& identifier,
                                         const IntSize& intrinsic_size,
                                         const AtomicString& id,
                                         Element* element) {
  if (!DomWindow())
    return;
  PerformanceElementTiming* entry = PerformanceElementTiming::Create(
      name, url, rect, MonotonicTimeToDOMHighResTimeStamp(start_time),
      MonotonicTimeToDOMHighResTimeStamp(load_time), identifier,
      intrinsic_size.Width(), intrinsic_size.Height(), id, element);
  TRACE_EVENT2("loading", "PerformanceElementTiming", "data",
               entry->ToTracedValue(), "frame",
               ToTraceValue(DomWindow()->GetFrame()));
  if (HasObserverFor(PerformanceEntry::kElement))
    NotifyObserversOfEntry(*entry);
  if (!IsElementTimingBufferFull())
    AddElementTimingBuffer(*entry);
}

void WindowPerformance::DispatchFirstInputTiming(
    PerformanceEventTiming* entry) {
  if (!entry)
    return;
  DCHECK_EQ("first-input", entry->entryType());
  if (HasObserverFor(PerformanceEntry::kFirstInput)) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kEventTimingExplicitlyRequested);
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kEventTimingFirstInputExplicitlyRequested);
    NotifyObserversOfEntry(*entry);
  }

  DCHECK(!first_input_timing_);
  first_input_timing_ = entry;
}

void WindowPerformance::AddLayoutShiftEntry(LayoutShift* entry) {
  if (HasObserverFor(PerformanceEntry::kLayoutShift))
    NotifyObserversOfEntry(*entry);
  AddLayoutShiftBuffer(*entry);
}

void WindowPerformance::AddVisibilityStateEntry(bool is_visible,
                                                base::TimeTicks timestamp) {
  DCHECK(RuntimeEnabledFeatures::VisibilityStateEntryEnabled());
  VisibilityStateEntry* entry = MakeGarbageCollected<VisibilityStateEntry>(
      PageHiddenStateString(!is_visible),
      MonotonicTimeToDOMHighResTimeStamp(timestamp));
  if (HasObserverFor(PerformanceEntry::kVisibilityState))
    NotifyObserversOfEntry(*entry);

  if (visibility_state_buffer_.size() < kDefaultVisibilityStateEntrySize)
    visibility_state_buffer_.push_back(entry);
}

void WindowPerformance::PageVisibilityChanged() {
  last_visibility_change_timestamp_ = base::TimeTicks::Now();
  if (!RuntimeEnabledFeatures::VisibilityStateEntryEnabled())
    return;

  AddVisibilityStateEntry(GetPage()->IsPageVisible(),
                          last_visibility_change_timestamp_);
}

EventCounts* WindowPerformance::eventCounts() {
  if (!event_counts_)
    event_counts_ = MakeGarbageCollected<EventCounts>();
  return event_counts_;
}

void WindowPerformance::OnLargestContentfulPaintUpdated(
    base::TimeTicks paint_time,
    uint64_t paint_size,
    base::TimeTicks load_time,
    const AtomicString& id,
    const String& url,
    Element* element) {
  base::TimeDelta render_timestamp = MonotonicTimeToTimeDelta(paint_time);
  base::TimeDelta load_timestamp = MonotonicTimeToTimeDelta(load_time);
  base::TimeDelta start_timestamp =
      render_timestamp.is_zero() ? load_timestamp : render_timestamp;
  auto* entry = MakeGarbageCollected<LargestContentfulPaint>(
      start_timestamp.InMillisecondsF(), render_timestamp, paint_size,
      load_timestamp, id, url, element);
  if (HasObserverFor(PerformanceEntry::kLargestContentfulPaint))
    NotifyObserversOfEntry(*entry);
  AddLargestContentfulPaint(entry);
}

void WindowPerformance::OnPaintFinished() {
  ++frame_index_;
}

void WindowPerformance::NotifyPotentialDrag() {
  responsiveness_metrics_.NotifyPotentialDrag();
}

}  // namespace blink
