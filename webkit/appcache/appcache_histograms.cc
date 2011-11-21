// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_histograms.h"

#include "base/metrics/histogram.h"

namespace appcache {

// static
void AppCacheHistograms::CountInitResult(InitResultType init_result) {
  UMA_HISTOGRAM_ENUMERATION(
       "appcache.InitResult",
       init_result, NUM_INIT_RESULT_TYPES);
}

// static
void AppCacheHistograms::CountCheckResponseResult(
    CheckResponseResultType result) {
  UMA_HISTOGRAM_ENUMERATION(
       "appcache.CheckResponseResult",
       result, NUM_CHECK_RESPONSE_RESULT_TYPES);
}

// static
void AppCacheHistograms::AddTaskQueueTimeSample(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("appcache.TaskQueueTime", duration);
}

// static
void AppCacheHistograms::AddTaskRunTimeSample(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("appcache.TaskRunTime", duration);
}

// static
void AppCacheHistograms::AddCompletionQueueTimeSample(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("appcache.CompletionQueueTime", duration);
}

// static
void AppCacheHistograms::AddCompletionRunTimeSample(
    const base::TimeDelta& duration) {
  UMA_HISTOGRAM_TIMES("appcache.CompletionRunTime", duration);
}

}  // namespace appcache
