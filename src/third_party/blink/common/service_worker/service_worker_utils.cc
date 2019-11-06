// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

const char kServiceWorkerEagerCodeCacheStrategy[] = "sw_cache_strategy";

namespace {

constexpr base::FeatureParam<EagerCodeCacheStrategy>::Option
    kEagerCodeCacheStrategyOptions[] = {
        {EagerCodeCacheStrategy::kDontGenerate, "dontgenerate"},
        {EagerCodeCacheStrategy::kDuringInstallEvent, "installevent"},
        {EagerCodeCacheStrategy::kOnIdleTask, "idletask"},
};

constexpr base::FeatureParam<EagerCodeCacheStrategy>
    kEagerCodeCacheStrategyParam{&features::kServiceWorkerAggressiveCodeCache,
                                 "sw_cache_strategy",
                                 EagerCodeCacheStrategy::kDuringInstallEvent,
                                 &kEagerCodeCacheStrategyOptions};

}  // namespace

bool ServiceWorkerUtils::IsImportedScriptUpdateCheckEnabled() {
  return base::FeatureList::IsEnabled(
      blink::features::kServiceWorkerImportedScriptUpdateCheck);
}

EagerCodeCacheStrategy ServiceWorkerUtils::GetEagerCodeCacheStrategy() {
  return kEagerCodeCacheStrategyParam.Get();
}

}  // namespace blink
