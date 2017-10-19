// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DocumentTiming.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

DocumentTiming::DocumentTiming(Document& document) : document_(document) {}

void DocumentTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
}

LocalFrame* DocumentTiming::GetFrame() const {
  return document_ ? document_->GetFrame() : nullptr;
}

void DocumentTiming::NotifyDocumentTimingChanged() {
  if (document_ && document_->Loader())
    document_->Loader()->DidChangePerformanceTiming();
}

void DocumentTiming::MarkDomLoading() {
  dom_loading_ = MonotonicallyIncreasingTime();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "domLoading",
                                   TraceEvent::ToTraceTimestamp(dom_loading_),
                                   "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomInteractive() {
  dom_interactive_ = MonotonicallyIncreasingTime();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing,rail", "domInteractive",
      TraceEvent::ToTraceTimestamp(dom_interactive_), "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomContentLoadedEventStart() {
  dom_content_loaded_event_start_ = MonotonicallyIncreasingTime();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing,rail", "domContentLoadedEventStart",
      TraceEvent::ToTraceTimestamp(dom_content_loaded_event_start_), "frame",
      GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomContentLoadedEventEnd() {
  dom_content_loaded_event_end_ = MonotonicallyIncreasingTime();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing,rail", "domContentLoadedEventEnd",
      TraceEvent::ToTraceTimestamp(dom_content_loaded_event_end_), "frame",
      GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomComplete() {
  dom_complete_ = MonotonicallyIncreasingTime();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "domComplete",
                                   TraceEvent::ToTraceTimestamp(dom_complete_),
                                   "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkFirstLayout() {
  first_layout_ = MonotonicallyIncreasingTime();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "firstLayout",
                                   TraceEvent::ToTraceTimestamp(first_layout_),
                                   "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

}  // namespace blink
