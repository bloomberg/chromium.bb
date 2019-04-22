// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/server_forbidden_check_request.h"

namespace offline_pages {

namespace {
void OnGeneratePageBundleResponse(PrefService* pref_service,
                                  PrefetchRequestStatus status,
                                  const std::string& operation_name,
                                  const std::vector<RenderPageInfo>& pages) {
  if (status == PrefetchRequestStatus::kSuccess ||
      status == PrefetchRequestStatus::kEmptyRequestSuccess) {
    // Request succeeded; enable prefetching.
    prefetch_prefs::SetEnabledByServer(pref_service, true);
  }
  // In the case of some error that isn't ForbiddenByOPS, do nothing and allow
  // the check to be run again.
}
}  // namespace

void CheckIfEnabledByServer(PrefetchNetworkRequestFactory* request_factory,
                            PrefService* pref_service) {
  // Make a GeneratePageBundle request for no pages.
  request_factory->MakeGeneratePageBundleRequest(
      std::vector<std::string>(), std::string(),
      base::BindOnce(&OnGeneratePageBundleResponse, pref_service));
}

}  // namespace offline_pages
