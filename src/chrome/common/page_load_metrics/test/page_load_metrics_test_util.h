// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PAGE_LOAD_METRICS_TEST_PAGE_LOAD_METRICS_TEST_UTIL_H_
#define CHROME_COMMON_PAGE_LOAD_METRICS_TEST_PAGE_LOAD_METRICS_TEST_UTIL_H_

#include <stdint.h>

#include <vector>

#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"

// Helper that fills in any timing fields that page load metrics requires but
// that are currently missing.
void PopulateRequiredTimingFields(
    page_load_metrics::mojom::PageLoadTiming* inout_timing);

// Helper that create a resource update mojo.
page_load_metrics::mojom::ResourceDataUpdatePtr CreateResource(
    bool was_cached,
    int64_t delta_bytes,
    int64_t encoded_body_length,
    bool is_complete);

// Helper that returns a sample resource data update using a variety of
// configurations.
std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>
GetSampleResourceDataUpdateForTesting(int64_t resource_size);

#endif  // CHROME_COMMON_PAGE_LOAD_METRICS_TEST_PAGE_LOAD_METRICS_TEST_UTIL_H_
