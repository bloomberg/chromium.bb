// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/appcache/appcache_histograms.h"

#include "base/metrics/histogram.h"

namespace appcache {

static std::string OriginToCustomHistogramSuffix(const GURL& origin_url) {
  if (origin_url.host() == "docs.google.com")
    return ".Docs";
  return std::string();
}

void AppCacheHistograms::CountInitResult(InitResultType init_result) {
  UMA_HISTOGRAM_ENUMERATION(
       "appcache.InitResult",
       init_result, NUM_INIT_RESULT_TYPES);
}

void AppCacheHistograms::CountReinitAttempt(bool repeated_attempt) {
  UMA_HISTOGRAM_BOOLEAN("appcache.ReinitAttempt", repeated_attempt);
}

void AppCacheHistograms::CountCorruptionDetected() {
  UMA_HISTOGRAM_BOOLEAN("appcache.CorruptionDetected", true);
}

void AppCacheHistograms::CountUpdateJobResult(
    AppCacheUpdateJob::ResultType result,
    const GURL& origin_url) {
  UMA_HISTOGRAM_ENUMERATION(
       "appcache.UpdateJobResult",
       result, AppCacheUpdateJob::NUM_UPDATE_JOB_RESULT_TYPES);

  const std::string suffix = OriginToCustomHistogramSuffix(origin_url);
  if (!suffix.empty()) {
    base::LinearHistogram::FactoryGet(
        "appcache.UpdateJobResult" + suffix,
        1,
        AppCacheUpdateJob::NUM_UPDATE_JOB_RESULT_TYPES,
        AppCacheUpdateJob::NUM_UPDATE_JOB_RESULT_TYPES + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag)->Add(result);
  }
}

void AppCacheHistograms::CountCheckResponseResult(
    CheckResponseResultType result) {
  UMA_HISTOGRAM_ENUMERATION(
       "appcache.CheckResponseResult",
       result, NUM_CHECK_RESPONSE_RESULT_TYPES);
}

void AppCacheHistograms::AddTaskQueueTimeSample(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("appcache.TaskQueueTime", duration);
}

void AppCacheHistograms::AddTaskRunTimeSample(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("appcache.TaskRunTime", duration);
}

void AppCacheHistograms::AddCompletionQueueTimeSample(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("appcache.CompletionQueueTime", duration);
}

void AppCacheHistograms::AddCompletionRunTimeSample(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("appcache.CompletionRunTime", duration);
}

void AppCacheHistograms::AddNetworkJobStartDelaySample(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("appcache.JobStartDelay.Network", duration);
}

void AppCacheHistograms::AddErrorJobStartDelaySample(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("appcache.JobStartDelay.Error", duration);
}

void AppCacheHistograms::AddAppCacheJobStartDelaySample(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("appcache.JobStartDelay.AppCache", duration);
}

void AppCacheHistograms::AddMissingManifestEntrySample() {
  UMA_HISTOGRAM_BOOLEAN("appcache.MissingManifestEntry", true);
}

void AppCacheHistograms::AddMissingManifestDetectedAtCallsite(
    MissingManifestCallsiteType callsite) {
  UMA_HISTOGRAM_ENUMERATION(
       "appcache.MissingManifestDetectedAtCallsite",
       callsite, NUM_MISSING_MANIFEST_CALLSITE_TYPES);
}

}  // namespace appcache
