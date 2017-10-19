/*
 * Copyright (C) 2011 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/loader/DocumentLoadTiming.h"

#include "core/loader/DocumentLoader.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

DocumentLoadTiming::DocumentLoadTiming(DocumentLoader& document_loader)
    : reference_monotonic_time_(0.0),
      reference_wall_time_(0.0),
      navigation_start_(0.0),
      unload_event_start_(0.0),
      unload_event_end_(0.0),
      redirect_start_(0.0),
      redirect_end_(0.0),
      redirect_count_(0),
      fetch_start_(0.0),
      response_end_(0.0),
      load_event_start_(0.0),
      load_event_end_(0.0),
      has_cross_origin_redirect_(false),
      has_same_origin_as_previous_document_(false),
      document_loader_(document_loader) {}

void DocumentLoadTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_loader_);
}

// TODO(csharrison): Remove the null checking logic in a later patch.
LocalFrame* DocumentLoadTiming::GetFrame() const {
  return document_loader_ ? document_loader_->GetFrame() : nullptr;
}

void DocumentLoadTiming::NotifyDocumentTimingChanged() {
  if (document_loader_)
    document_loader_->DidChangePerformanceTiming();
}

void DocumentLoadTiming::EnsureReferenceTimesSet() {
  if (!reference_wall_time_)
    reference_wall_time_ = CurrentTime();
  if (!reference_monotonic_time_)
    reference_monotonic_time_ = MonotonicallyIncreasingTime();
}

double DocumentLoadTiming::MonotonicTimeToZeroBasedDocumentTime(
    double monotonic_time) const {
  if (!monotonic_time || !reference_monotonic_time_)
    return 0.0;
  return monotonic_time - reference_monotonic_time_;
}

double DocumentLoadTiming::MonotonicTimeToPseudoWallTime(
    double monotonic_time) const {
  if (!monotonic_time || !reference_monotonic_time_)
    return 0.0;
  return reference_wall_time_ + monotonic_time - reference_monotonic_time_;
}

double DocumentLoadTiming::PseudoWallTimeToMonotonicTime(
    double pseudo_wall_time) const {
  if (!pseudo_wall_time)
    return 0.0;
  return reference_monotonic_time_ + pseudo_wall_time - reference_wall_time_;
}

void DocumentLoadTiming::MarkNavigationStart() {
  // Allow the embedder to override navigationStart before we record it if
  // they have a more accurate timestamp.
  if (navigation_start_) {
    DCHECK(reference_monotonic_time_);
    DCHECK(reference_wall_time_);
    return;
  }
  DCHECK(!navigation_start_);
  DCHECK(!reference_monotonic_time_);
  DCHECK(!reference_wall_time_);
  EnsureReferenceTimesSet();
  navigation_start_ = reference_monotonic_time_;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing", "navigationStart",
      TraceEvent::ToTraceTimestamp(navigation_start_), "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::SetNavigationStart(double navigation_start) {
  // |m_referenceMonotonicTime| and |m_referenceWallTime| represent
  // navigationStart. We must set these to the current time if they haven't
  // been set yet in order to have a valid reference time in both units.
  EnsureReferenceTimesSet();
  navigation_start_ = navigation_start;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing", "navigationStart",
      TraceEvent::ToTraceTimestamp(navigation_start_), "frame", GetFrame());

  // The reference times are adjusted based on the embedder's navigationStart.
  DCHECK(reference_monotonic_time_);
  DCHECK(reference_wall_time_);
  reference_wall_time_ = MonotonicTimeToPseudoWallTime(navigation_start);
  reference_monotonic_time_ = navigation_start;
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::AddRedirect(const KURL& redirecting_url,
                                     const KURL& redirected_url) {
  redirect_count_++;
  if (!redirect_start_) {
    SetRedirectStart(fetch_start_);
  }
  MarkRedirectEnd();
  MarkFetchStart();

  // Check if the redirected url is allowed to access the redirecting url's
  // timing information.
  RefPtr<SecurityOrigin> redirected_security_origin =
      SecurityOrigin::Create(redirected_url);
  has_cross_origin_redirect_ |=
      !redirected_security_origin->CanRequest(redirecting_url);
}

void DocumentLoadTiming::SetRedirectStart(double redirect_start) {
  redirect_start_ = redirect_start;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing", "redirectStart",
      TraceEvent::ToTraceTimestamp(redirect_start_), "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::SetRedirectEnd(double redirect_end) {
  redirect_end_ = redirect_end;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "redirectEnd",
                                   TraceEvent::ToTraceTimestamp(redirect_end_),
                                   "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::MarkUnloadEventStart(double start_time) {
  unload_event_start_ = start_time;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "unloadEventStart",
                                   TraceEvent::ToTraceTimestamp(start_time),
                                   "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::MarkUnloadEventEnd(double end_time) {
  unload_event_end_ = end_time;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "unloadEventEnd",
                                   TraceEvent::ToTraceTimestamp(end_time),
                                   "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::MarkFetchStart() {
  SetFetchStart(MonotonicallyIncreasingTime());
}

void DocumentLoadTiming::SetFetchStart(double fetch_start) {
  fetch_start_ = fetch_start;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "fetchStart",
                                   TraceEvent::ToTraceTimestamp(fetch_start_),
                                   "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::SetResponseEnd(double response_end) {
  response_end_ = response_end;
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "responseEnd",
                                   TraceEvent::ToTraceTimestamp(response_end_),
                                   "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::MarkLoadEventStart() {
  load_event_start_ = MonotonicallyIncreasingTime();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing", "loadEventStart",
      TraceEvent::ToTraceTimestamp(load_event_start_), "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::MarkLoadEventEnd() {
  load_event_end_ = MonotonicallyIncreasingTime();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing", "loadEventEnd",
      TraceEvent::ToTraceTimestamp(load_event_end_), "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

void DocumentLoadTiming::MarkRedirectEnd() {
  redirect_end_ = MonotonicallyIncreasingTime();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "redirectEnd",
                                   TraceEvent::ToTraceTimestamp(redirect_end_),
                                   "frame", GetFrame());
  NotifyDocumentTimingChanged();
}

}  // namespace blink
