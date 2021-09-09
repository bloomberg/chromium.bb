// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_manager_delegate.h"

namespace prerender {

NoStatePrefetchManagerDelegate::NoStatePrefetchManagerDelegate() = default;

void NoStatePrefetchManagerDelegate::MaybePreconnect(const GURL& url) {}

bool NoStatePrefetchManagerDelegate::IsNetworkPredictionPreferenceEnabled() {
  return true;
}

bool NoStatePrefetchManagerDelegate::IsPredictionDisabledDueToNetwork(
    Origin origin) {
  return false;
}

std::string NoStatePrefetchManagerDelegate::GetReasonForDisablingPrediction() {
  return std::string();
}

}  // namespace prerender