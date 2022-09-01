// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PRIVATE_NETWORK_ACCESS_CHECKER_H_
#define SERVICES_NETWORK_PRIVATE_NETWORK_ACCESS_CHECKER_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/private_network_access_check_result.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

struct TransportInfo;

}  // namespace net

namespace network {

struct ResourceRequest;

// Applies Private Network Access checks to a single fetch / URL load.
//
// Manages state used for the "Private Network Access check" algorithm from
// the Private Network Access spec:
// https://wicg.github.io/private-network-access/#private-network-access-check
//
// Helper class for `URLLoader`. Should be instantiated once per `URLLoader`.
//
// Thread-compatible.
class COMPONENT_EXPORT(NETWORK_SERVICE) PrivateNetworkAccessChecker {
 public:
  // `resource_request` and `url_load_options` correspond to `URLLoader`
  // constructor arguments.
  // `factory_params` should point to the parameters used by the factory that
  // built the owner `URLLoader`. Must not be nullptr and must outlive this
  // instance.
  PrivateNetworkAccessChecker(
      const ResourceRequest& resource_request,
      const mojom::URLLoaderFactoryParams* factory_params,
      int32_t url_load_options);

  // Instances of this class are neither copyable nor movable.
  PrivateNetworkAccessChecker(const PrivateNetworkAccessChecker&) = delete;
  PrivateNetworkAccessChecker& operator=(const PrivateNetworkAccessChecker&) =
      delete;

  ~PrivateNetworkAccessChecker();

  // Checks whether the client should be allowed to use the given transport.
  //
  // Implements the following "Private Network Access check" algorithm:
  // https://wicg.github.io/private-network-access/#private-network-access-check
  PrivateNetworkAccessCheckResult Check(
      const net::TransportInfo& transport_info);

  // Returns the IP address space derived from the `transport_info` argument
  // passed to the last call to `Check()`, if any.
  //
  // Spec:
  // https://wicg.github.io/private-network-access/#response-ip-address-space
  absl::optional<mojom::IPAddressSpace> ResponseAddressSpace() const {
    return response_address_space_;
  }

  // The target IP address space applied to subsequent checks.
  //
  // Spec:
  // https://wicg.github.io/private-network-access/#request-target-ip-address-space
  mojom::IPAddressSpace TargetAddressSpace() const {
    return target_address_space_;
  }

  // Clears state from all checks this instance has performed.
  //
  // This instance will behave as if newly constructed once more. In addition,
  // resets this instance's target IP address space to `kUnknown.
  //
  // This should be called upon following a redirect.
  void ResetForRedirect();

  // Returns the client security state that applies to the current request.
  // May return nullptr.
  //
  // Contains relevant state derived from the fetch client's policy container.
  const mojom::ClientSecurityState* client_security_state() const {
    return client_security_state_.get();
  }

  // Returns an owned clone of `client_security_state()`.
  mojom::ClientSecurityStatePtr CloneClientSecurityState() const;

  // Returns the IP address space in `client_security_state()`.
  // Returns `kUnknown` if `client_security_state()` is nullptr.
  mojom::IPAddressSpace ClientAddressSpace() const;

 private:
  // Returns whether this instance has a client security state containing a
  // policy set to `kPreflightWarn`.
  bool IsPolicyPreflightWarn() const;

  // Helper for `Check()`.
  PrivateNetworkAccessCheckResult CheckInternal(
      mojom::IPAddressSpace resource_address_space);

  // The client security state copied from the request's trusted params.
  // May be nullptr.
  //
  // Should not be used directly. Use `client_security_state_` instead, which
  // points to the same struct iff this client security state should be used.
  const mojom::ClientSecurityStatePtr request_client_security_state_;

  // The security state of the client of the fetch. May be nullptr.
  const raw_ptr<const mojom::ClientSecurityState> client_security_state_;

  // Whether to block all requests to non-public IP address spaces, regardless
  // of other considerations. Set based on URL load options.
  const bool should_block_local_request_;

  // True iff |Check()| was called multiple times in between resets and the IP
  // address space of the transport was not the same each time.
  bool has_connected_to_mismatched_address_spaces_ = false;

  // The target IP address space set on the request. Ignored if `kUnknown`.
  //
  // Copied from `ResourceRequest::target_ip_address_space`.
  //
  // Invariant: always `kUnknown` if `client_security_state_` is nullptr, or
  // if `client_security_state_->private_network_request_policy` is `kAllow`.
  //
  // https://wicg.github.io/private-network-access/#request-target-ip-address-space
  mojom::IPAddressSpace target_address_space_;

  // The IP address space derived from the `transport_info` argument passed to
  // the last call to `Check()`.
  //
  // Set by `Check()`, reset by `ResetForRedirect()`.
  absl::optional<mojom::IPAddressSpace> response_address_space_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PRIVATE_NETWORK_ACCESS_CHECKER_H_
