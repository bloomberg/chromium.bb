// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DocumentTiming.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/InteractiveDetector.h"
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
  dom_loading_ = CurrentTimeTicks();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "domLoading",
                                   dom_loading_, "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomInteractive() {
  dom_interactive_ = CurrentTimeTicks();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "domInteractive",
                                   dom_interactive_, "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomContentLoadedEventStart() {
  dom_content_loaded_event_start_ = CurrentTimeTicks();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing,rail", "domContentLoadedEventStart",
      dom_content_loaded_event_start_, "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomContentLoadedEventEnd() {
  dom_content_loaded_event_end_ = CurrentTimeTicks();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing,rail", "domContentLoadedEventEnd",
      dom_content_loaded_event_end_, "frame", GetFrame());
  InteractiveDetector* interactive_detector(
      InteractiveDetector::From(*document_));
  if (interactive_detector) {
    interactive_detector->OnDomContentLoadedEnd(dom_content_loaded_event_end_);
  }
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomComplete() {
  dom_complete_ = CurrentTimeTicks();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "domComplete",
                                   dom_complete_, "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkFirstLayout() {
  first_layout_ = CurrentTimeTicks();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "firstLayout",
                                   first_layout_, "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

}  // namespace blink
