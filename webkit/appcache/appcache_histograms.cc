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

}  // namespace appcache
