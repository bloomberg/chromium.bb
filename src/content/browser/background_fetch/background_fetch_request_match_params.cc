// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_request_match_params.h"

namespace content {

BackgroundFetchRequestMatchParams::BackgroundFetchRequestMatchParams(
    base::Optional<ServiceWorkerFetchRequest> request_to_match,
    blink::mojom::QueryParamsPtr cache_query_params,
    bool match_all)
    : request_to_match_(std::move(request_to_match)),
      cache_query_params_(std::move(cache_query_params)),
      match_all_(match_all) {}

BackgroundFetchRequestMatchParams::BackgroundFetchRequestMatchParams() =
    default;
BackgroundFetchRequestMatchParams::~BackgroundFetchRequestMatchParams() =
    default;

}  // namespace content