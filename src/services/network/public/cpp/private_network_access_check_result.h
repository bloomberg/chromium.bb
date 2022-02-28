// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PRIVATE_NETWORK_ACCESS_CHECK_RESULT_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PRIVATE_NETWORK_ACCESS_CHECK_RESULT_H_

#include "base/component_export.h"
#include "services/network/public/mojom/cors.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PrivateNetworkAccessCheckResult {
  // Request is allowed because it is missing a client security state.
  kAllowedMissingClientSecurityState = 0,

  // Not a private network request: the resource address space is no less
  // public than the client's.
  kAllowedNoLessPublic = 1,

  // Private network request: allowed because policy is `kAllow`.
  kAllowedByPolicyAllow = 2,

  // Private network request: allowed because policy is `kWarn`.
  kAllowedByPolicyWarn = 3,

  // URL loader options include `kURLLoadOptionBlockLocalRequest` and the
  // resource address space is not `kPublic`.
  kBlockedByLoadOption = 4,

  // Private network request: blocked because policy is `kBlock`.
  kBlockedByPolicyBlock = 5,

  // Request carries a `target_ip_address_space` that matches the resource
  // address space.
  kAllowedByTargetIpAddressSpace = 6,

  // Request carries a `target_ip_address_space` that differs from the actual
  // resource address space. This may be indicative of a DNS rebinding attack.
  kBlockedByTargetIpAddressSpace = 7,

  // Private network request: blocked because `target_ip_address_space` is
  // `kUnknown` and policy is `kPreflightWarn`.
  kBlockedByPolicyPreflightWarn = 8,

  // Private network request: blocked because `target_ip_address_space` is
  // `kUnknown` and policy is `kPreflightBlock`.
  kBlockedByPolicyPreflightBlock = 9,

  // Required for UMA histogram logging.
  kMaxValue = kBlockedByPolicyPreflightBlock,
};

// If `result` indicates that the request should be blocked, returns the
// corresponding `CorsError` enum value. Otherwise returns `nullopt`.
absl::optional<mojom::CorsError> COMPONENT_EXPORT(NETWORK_CPP)
    PrivateNetworkAccessCheckResultToCorsError(
        PrivateNetworkAccessCheckResult result);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PRIVATE_NETWORK_ACCESS_CHECK_RESULT_H_
