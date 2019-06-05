// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_MANAGER_H_
#define SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_MANAGER_H_

#include <memory>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/origin_policy/origin_policy_fetcher.h"
#include "services/network/public/mojom/origin_policy_manager.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

// The OriginPolicyManager is the entry point for all Origin Policy related
// API calls. Spec: https://wicg.github.io/origin-policy/
// A client will likely call AddBinding (or use the NetworkContext function)
// and then do mojom::OriginPolicy related operations, like retrieving a policy
// (which could potentially trigger a fetch), or adding an exception.
class COMPONENT_EXPORT(NETWORK_SERVICE) OriginPolicyManager
    : public mojom::OriginPolicyManager {
 public:
  // Represents a parsed `Sec-Origin-Policy` header.
  // Spec: https://wicg.github.io/origin-policy/#origin-policy-header
  struct OriginPolicyHeaderValues {
    // The policy version that is parsed from the `policy=` parameter.
    std::string policy_version;
    // The report group to send reports to if an error occurs. Uses the
    // reporting API. Parsed from the `report-to=` parameter.
    std::string report_to;
  };

  explicit OriginPolicyManager(mojom::URLLoaderFactoryPtr url_loader_factory);
  ~OriginPolicyManager() override;

  // Bind a request to this object.  Mojo messages
  // coming through the associated pipe will be served by this object.
  void AddBinding(mojom::OriginPolicyManagerRequest request);

  // mojom::OriginPolicy
  void RetrieveOriginPolicy(const url::Origin& origin,
                            const std::string& header_value,
                            RetrieveOriginPolicyCallback callback) override;

  // To be called by fetcher when it has finished its work.
  // This removes the fetcher which results in the fetcher being destroyed.
  void FetcherDone(OriginPolicyFetcher* fetcher);

  // Retrieves an origin's default origin policy by attempting to fetch it
  // from "<origin>/.well-known/origin-policy".
  void RetrieveDefaultOriginPolicy(const url::Origin& origin,
                                   RetrieveOriginPolicyCallback callback);

  // ForTesting methods
  mojo::BindingSet<mojom::OriginPolicyManager>& GetBindingsForTesting() {
    return bindings_;
  }

  static OriginPolicyHeaderValues
  GetRequestedPolicyAndReportGroupFromHeaderStringForTesting(
      const std::string& header_value) {
    return GetRequestedPolicyAndReportGroupFromHeaderString(header_value);
  }

 private:
  // Parses a header and returns the result. If a parsed result does not contain
  // a non-empty policy version it means the `header_value` is invalid.
  static OriginPolicyHeaderValues
  GetRequestedPolicyAndReportGroupFromHeaderString(
      const std::string& header_value);

  // Will start a fetch based on the provided origin and info.
  void StartPolicyFetch(const url::Origin& origin,
                        const OriginPolicyHeaderValues& header_info,
                        RetrieveOriginPolicyCallback callback);

  // A list of fetchers owned by this object
  std::set<std::unique_ptr<OriginPolicyFetcher>, base::UniquePtrComparator>
      origin_policy_fetchers_;

  // Used for fetching requests
  mojom::URLLoaderFactoryPtr url_loader_factory_;

  // This object's set of bindings.
  // This MUST be below origin_policy_fetchers_ to ensure it is destroyed before
  // it. Otherwise it's possible that un-invoked OnceCallbacks owned by members
  // of origin_policy_fetchers_ will be destroyed before the beinding they are
  // on is destroyed.
  mojo::BindingSet<mojom::OriginPolicyManager> bindings_;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyManager);
};

}  // namespace network

#endif  // SERVICES_NETWORK_ORIGIN_POLICY_ORIGIN_POLICY_MANAGER_H_
