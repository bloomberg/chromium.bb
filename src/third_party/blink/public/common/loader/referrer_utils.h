// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_REFERRER_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_REFERRER_UTILS_H_

#include "base/optional.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

class ReferrerUtils {
 public:
  static BLINK_COMMON_EXPORT network::mojom::ReferrerPolicy
  NetToMojoReferrerPolicy(net::ReferrerPolicy net_policy);

  static BLINK_COMMON_EXPORT net::ReferrerPolicy GetDefaultNetReferrerPolicy();

  // The ReferrerPolicy enum contains a member kDefault, which is not a real
  // referrer policy, but instead Blink fallbacks to kNoReferrerWhenDowngrade
  // when a certain condition is met. The function below is provided so that a
  // referrer policy which may be kDefault can be resolved to a valid value.
  static BLINK_COMMON_EXPORT network::mojom::ReferrerPolicy
      MojoReferrerPolicyResolveDefault(network::mojom::ReferrerPolicy);

  // Configures retaining the pre-M80 default referrer
  // policy of no-referrer-when-downgrade.
  // TODO(crbug.com/1016541): After M88, remove when the corresponding
  // enterprise policy has been deleted.
  static BLINK_COMMON_EXPORT void SetForceLegacyDefaultReferrerPolicy(
      bool force);
  static BLINK_COMMON_EXPORT bool ShouldForceLegacyDefaultReferrerPolicy();

  static BLINK_COMMON_EXPORT bool IsReducedReferrerGranularityEnabled();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_REFERRER_UTILS_H_
