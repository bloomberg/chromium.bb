// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/test_fetcher.h"

#include <utility>

#include "chrome/browser/apps/app_discovery_service/result.h"

namespace apps {

TestFetcher::TestFetcher() = default;

TestFetcher::~TestFetcher() = default;

void TestFetcher::SetResults(std::vector<Result> results) {
  results_ = std::move(results);
}

void TestFetcher::GetApps(ResultCallback callback) {
  if (!results_.empty()) {
    std::move(callback).Run(std::move(results_));
    return;
  }
}

}  // namespace apps
