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

#include "public/web/WebPerformance.h"

#include "core/timing/Performance.h"

namespace blink {

static double MillisecondsToSeconds(unsigned long long milliseconds) {
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

double WebPerformance::UnloadEventEnd() const {
  return MillisecondsToSeconds(private_->timing()->unloadEventEnd());
}

double WebPerformance::RedirectStart() const {
  return MillisecondsToSeconds(private_->timing()->redirectStart());
}

double WebPerformance::RedirectEnd() const {
  return MillisecondsToSeconds(private_->timing()->redirectEnd());
}

unsigned short WebPerformance::RedirectCount() const {
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

double WebPerformance::FirstLayout() const {
  return MillisecondsToSeconds(private_->timing()->FirstLayout());
}

double WebPerformance::FirstPaint() const {
  return MillisecondsToSeconds(private_->timing()->FirstPaint());
}

double WebPerformance::FirstTextPaint() const {
  return MillisecondsToSeconds(private_->timing()->FirstTextPaint());
}

double WebPerformance::FirstImagePaint() const {
  return MillisecondsToSeconds(private_->timing()->FirstImagePaint());
}

double WebPerformance::FirstContentfulPaint() const {
  return MillisecondsToSeconds(private_->timing()->FirstContentfulPaint());
}

double WebPerformance::FirstMeaningfulPaint() const {
  return MillisecondsToSeconds(private_->timing()->FirstMeaningfulPaint());
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

double WebPerformance::AuthorStyleSheetParseDurationBeforeFCP() const {
  return MillisecondsToSeconds(
      private_->timing()->AuthorStyleSheetParseDurationBeforeFCP());
}

double WebPerformance::UpdateStyleDurationBeforeFCP() const {
  return MillisecondsToSeconds(
      private_->timing()->UpdateStyleDurationBeforeFCP());
}

WebPerformance::WebPerformance(Performance* performance)
    : private_(performance) {}

WebPerformance& WebPerformance::operator=(Performance* performance) {
  private_ = performance;
  return *this;
}

}  // namespace blink
