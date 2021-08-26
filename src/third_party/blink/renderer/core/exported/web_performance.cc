/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/public/web/web_performance.h"

#include "third_party/blink/renderer/core/timing/window_performance.h"

namespace blink {

static double MillisecondsToSeconds(uint64_t milliseconds) {
  return static_cast<double>(milliseconds / 1000.0);
}

void WebPerformance::Reset() {
  private_.Reset();
}

void WebPerformance::Assign(const WebPerformance& other) {
  private_ = other.private_;
}

WebNavigationType WebPerformance::GetNavigationType() const {
  switch (private_->navigation()->type()) {
    case PerformanceNavigation::kTypeNavigate:
      return kWebNavigationTypeOther;
    case PerformanceNavigation::kTypeReload:
      return kWebNavigationTypeReload;
    case PerformanceNavigation::kTypeBackForward:
      return kWebNavigationTypeBackForward;
    case PerformanceNavigation::kTypeReserved:
      return kWebNavigationTypeOther;
  }
  NOTREACHED();
  return kWebNavigationTypeOther;
}

double WebPerformance::NavigationStart() const {
  return MillisecondsToSeconds(private_->timing()->navigationStart());
}

base::TimeTicks WebPerformance::NavigationStartAsMonotonicTime() const {
  return private_->timing()->NavigationStartAsMonotonicTime();
}

WebPerformance::BackForwardCacheRestoreTimings
WebPerformance::BackForwardCacheRestore() const {
  PerformanceTiming::BackForwardCacheRestoreTimings restore_timings =
      private_->timing()->BackForwardCacheRestore();

  WebVector<BackForwardCacheRestoreTiming> timings(restore_timings.size());
  for (wtf_size_t i = 0; i < restore_timings.size(); i++) {
    timings[i].navigation_start =
        MillisecondsToSeconds(restore_timings[i].navigation_start);
    timings[i].first_paint =
        MillisecondsToSeconds(restore_timings[i].first_paint);
    for (wtf_size_t j = 0;
         j < restore_timings[i].request_animation_frames.size(); j++) {
      timings[i].request_animation_frames[j] =
          MillisecondsToSeconds(restore_timings[i].request_animation_frames[j]);
    }
    timings[i].first_input_delay = restore_timings[i].first_input_delay;
  }
  return timings;
}

double WebPerformance::InputForNavigationStart() const {
  return MillisecondsToSeconds(private_->timing()->inputStart());
}

double WebPerformance::UnloadEventEnd() const {
  return MillisecondsToSeconds(private_->timing()->unloadEventEnd());
}

double WebPerformance::RedirectStart() const {
  return MillisecondsToSeconds(private_->timing()->redirectStart());
}

double WebPerformance::RedirectEnd() const {
  return MillisecondsToSeconds(private_->timing()->redirectEnd());
}

uint16_t WebPerformance::RedirectCount() const {
  return private_->navigation()->redirectCount();
}

double WebPerformance::FetchStart() const {
  return MillisecondsToSeconds(private_->timing()->fetchStart());
}

double WebPerformance::DomainLookupStart() const {
  return MillisecondsToSeconds(private_->timing()->domainLookupStart());
}

double WebPerformance::DomainLookupEnd() const {
  return MillisecondsToSeconds(private_->timing()->domainLookupEnd());
}

double WebPerformance::ConnectStart() const {
  return MillisecondsToSeconds(private_->timing()->connectStart());
}

double WebPerformance::ConnectEnd() const {
  return MillisecondsToSeconds(private_->timing()->connectEnd());
}

double WebPerformance::RequestStart() const {
  return MillisecondsToSeconds(private_->timing()->requestStart());
}

double WebPerformance::ResponseStart() const {
  return MillisecondsToSeconds(private_->timing()->responseStart());
}

double WebPerformance::ResponseEnd() const {
  return MillisecondsToSeconds(private_->timing()->responseEnd());
}

double WebPerformance::DomLoading() const {
  return MillisecondsToSeconds(private_->timing()->domLoading());
}

double WebPerformance::DomInteractive() const {
  return MillisecondsToSeconds(private_->timing()->domInteractive());
}

double WebPerformance::DomContentLoadedEventStart() const {
  return MillisecondsToSeconds(
      private_->timing()->domContentLoadedEventStart());
}

double WebPerformance::DomContentLoadedEventEnd() const {
  return MillisecondsToSeconds(private_->timing()->domContentLoadedEventEnd());
}

double WebPerformance::DomComplete() const {
  return MillisecondsToSeconds(private_->timing()->domComplete());
}

double WebPerformance::LoadEventStart() const {
  return MillisecondsToSeconds(private_->timing()->loadEventStart());
}

double WebPerformance::LoadEventEnd() const {
  return MillisecondsToSeconds(private_->timing()->loadEventEnd());
}

double WebPerformance::FirstPaint() const {
  return MillisecondsToSeconds(private_->timing()->FirstPaint());
}

double WebPerformance::FirstImagePaint() const {
  return MillisecondsToSeconds(private_->timing()->FirstImagePaint());
}

double WebPerformance::FirstContentfulPaint() const {
  return MillisecondsToSeconds(private_->timing()->FirstContentfulPaint());
}

base::TimeTicks WebPerformance::FirstContentfulPaintAsMonotonicTime() const {
  return private_->timing()->FirstContentfulPaintAsMonotonicTime();
}

double WebPerformance::FirstMeaningfulPaint() const {
  return MillisecondsToSeconds(private_->timing()->FirstMeaningfulPaint());
}

double WebPerformance::FirstMeaningfulPaintCandidate() const {
  return MillisecondsToSeconds(
      private_->timing()->FirstMeaningfulPaintCandidate());
}

double WebPerformance::LargestImagePaint() const {
  return MillisecondsToSeconds(private_->timing()->LargestImagePaint());
}

uint64_t WebPerformance::LargestImagePaintSize() const {
  return private_->timing()->LargestImagePaintSize();
}

double WebPerformance::LargestTextPaint() const {
  return MillisecondsToSeconds(private_->timing()->LargestTextPaint());
}

uint64_t WebPerformance::LargestTextPaintSize() const {
  return private_->timing()->LargestTextPaintSize();
}

base::TimeTicks WebPerformance::LargestContentfulPaintAsMonotonicTime() const {
  return private_->timing()->LargestContentfulPaintAsMonotonicTime();
}

double WebPerformance::ExperimentalLargestImagePaint() const {
  return MillisecondsToSeconds(
      private_->timing()->ExperimentalLargestImagePaint());
}

uint64_t WebPerformance::ExperimentalLargestImagePaintSize() const {
  return private_->timing()->ExperimentalLargestImagePaintSize();
}

double WebPerformance::ExperimentalLargestTextPaint() const {
  return MillisecondsToSeconds(
      private_->timing()->ExperimentalLargestTextPaint());
}

uint64_t WebPerformance::ExperimentalLargestTextPaintSize() const {
  return private_->timing()->ExperimentalLargestTextPaintSize();
}

double WebPerformance::FirstEligibleToPaint() const {
  return MillisecondsToSeconds(private_->timing()->FirstEligibleToPaint());
}

double WebPerformance::FirstInputOrScrollNotifiedTimestamp() const {
  return MillisecondsToSeconds(
      private_->timing()->FirstInputOrScrollNotifiedTimestamp());
}

absl::optional<base::TimeDelta> WebPerformance::FirstInputDelay() const {
  return private_->timing()->FirstInputDelay();
}

absl::optional<base::TimeDelta> WebPerformance::FirstInputTimestamp() const {
  return private_->timing()->FirstInputTimestamp();
}

absl::optional<base::TimeDelta> WebPerformance::LongestInputDelay() const {
  return private_->timing()->LongestInputDelay();
}

absl::optional<base::TimeDelta> WebPerformance::LongestInputTimestamp() const {
  return private_->timing()->LongestInputTimestamp();
}

absl::optional<base::TimeDelta> WebPerformance::FirstInputProcessingTime()
    const {
  return private_->timing()->FirstInputProcessingTime();
}

absl::optional<base::TimeDelta> WebPerformance::FirstScrollDelay() const {
  return private_->timing()->FirstScrollDelay();
}

absl::optional<base::TimeDelta> WebPerformance::FirstScrollTimestamp() const {
  return private_->timing()->FirstScrollTimestamp();
}

double WebPerformance::ParseStart() const {
  return MillisecondsToSeconds(private_->timing()->ParseStart());
}

double WebPerformance::ParseStop() const {
  return MillisecondsToSeconds(private_->timing()->ParseStop());
}

double WebPerformance::ParseBlockedOnScriptLoadDuration() const {
  return MillisecondsToSeconds(
      private_->timing()->ParseBlockedOnScriptLoadDuration());
}

double WebPerformance::ParseBlockedOnScriptLoadFromDocumentWriteDuration()
    const {
  return MillisecondsToSeconds(
      private_->timing()->ParseBlockedOnScriptLoadFromDocumentWriteDuration());
}

double WebPerformance::ParseBlockedOnScriptExecutionDuration() const {
  return MillisecondsToSeconds(
      private_->timing()->ParseBlockedOnScriptExecutionDuration());
}

double WebPerformance::ParseBlockedOnScriptExecutionFromDocumentWriteDuration()
    const {
  return MillisecondsToSeconds(
      private_->timing()
          ->ParseBlockedOnScriptExecutionFromDocumentWriteDuration());
}

absl::optional<base::TimeTicks> WebPerformance::LastPortalActivatedPaint()
    const {
  return private_->timing()->LastPortalActivatedPaint();
}

absl::optional<base::TimeDelta> WebPerformance::PrerenderActivationStart()
    const {
  return private_->timing()->PrerenderActivationStart();
}

absl::optional<base::TimeTicks> WebPerformance::UnloadStart() const {
  return private_->timing()->UnloadStart();
}

absl::optional<base::TimeTicks> WebPerformance::UnloadEnd() const {
  return private_->timing()->UnloadEnd();
}

absl::optional<base::TimeTicks> WebPerformance::CommitNavigationEnd() const {
  return private_->timing()->CommitNavigationEnd();
}

absl::optional<base::TimeDelta> WebPerformance::UserTimingMarkFullyLoaded()
    const {
  return private_->timing()->UserTimingMarkFullyLoaded();
}

absl::optional<base::TimeDelta> WebPerformance::UserTimingMarkFullyVisible()
    const {
  return private_->timing()->UserTimingMarkFullyVisible();
}

absl::optional<base::TimeDelta> WebPerformance::UserTimingMarkInteractive()
    const {
  return private_->timing()->UserTimingMarkInteractive();
}

WebPerformance::WebPerformance(WindowPerformance* performance)
    : private_(performance) {}

WebPerformance& WebPerformance::operator=(WindowPerformance* performance) {
  private_ = performance;
  return *this;
}

}  // namespace blink
