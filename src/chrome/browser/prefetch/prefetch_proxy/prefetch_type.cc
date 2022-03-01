// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_type.h"

#include <tuple>

#include "base/check.h"

PrefetchType::PrefetchType(bool use_isolated_network_context,
                           bool use_prefetch_proxy,
                           bool can_prefetch_subresources)
    : use_isolated_network_context_(use_isolated_network_context),
      use_prefetch_proxy_(use_prefetch_proxy),
      can_prefetch_subresources_(can_prefetch_subresources) {
  // Checks that the given dimensions are a supported prefetch type.
  DCHECK((use_isolated_network_context_ && use_prefetch_proxy_ &&
          can_prefetch_subresources_) ||
         (use_isolated_network_context_ && use_prefetch_proxy_ &&
          !can_prefetch_subresources_) ||
         (use_isolated_network_context_ && !use_prefetch_proxy_ &&
          !can_prefetch_subresources_) ||
         (!use_isolated_network_context_ && !use_prefetch_proxy_ &&
          !can_prefetch_subresources_));
}

PrefetchType::~PrefetchType() = default;
PrefetchType::PrefetchType(const PrefetchType& prefetch_type) = default;
PrefetchType& PrefetchType::operator=(const PrefetchType& prefetch_type) =
    default;

bool operator==(const PrefetchType& prefetch_type_1,
                const PrefetchType& prefetch_type_2) {
  return std::tie(prefetch_type_1.use_isolated_network_context_,
                  prefetch_type_1.use_prefetch_proxy_,
                  prefetch_type_1.can_prefetch_subresources_) ==
         std::tie(prefetch_type_2.use_isolated_network_context_,
                  prefetch_type_2.use_prefetch_proxy_,
                  prefetch_type_2.can_prefetch_subresources_);
}

bool operator!=(const PrefetchType& prefetch_type_1,
                const PrefetchType& prefetch_type_2) {
  return !(prefetch_type_1 == prefetch_type_2);
}
