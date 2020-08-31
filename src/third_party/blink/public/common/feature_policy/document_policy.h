// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_DOCUMENT_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_DOCUMENT_POLICY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/feature_policy/document_policy_features.h"
#include "third_party/blink/public/common/feature_policy/policy_value.h"
#include "third_party/blink/public/mojom/feature_policy/document_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/feature_policy/policy_value.mojom.h"

namespace blink {

// Document Policy is a mechanism for controlling the behaviour of web platform
// features in a document, and for requesting such changes in embedded frames.
// (The specific changes which are made depend on the feature; see the
// specification for details).
//
// Policies can be defined in the HTTP header stream, with the |Document-Policy|
// HTTP header, or can be set by the |policy| attributes on the iframe element
// which embeds the document.
//
// See
// https://github.com/w3c/webappsec-feature-policy/blob/master/document-policy-explainer.md
//
// Key concepts:
//
// Features
// --------
// Features which can be controlled by policy are defined by instances of enum
// mojom::DocumentPolicyFeature, declared in |document_policy_feature.mojom|.
//
// Declarations
// ------------
// A document policy declaration is a mapping of a feature name to a policy
// value. A set of such declarations is a declared policy. The declared policy
// is attached to a document.
//
// Required Policy
// ----------------
// In addition to the declared policy (which may be empty), every frame has
// an required policy, which is set by the embedding document (or inherited
// from its parent). Any document loaded into a frame with a required policy
// must have a declared policy which is compatible with it. Frames may add new
// requirements to their own subframes, but cannot relax any existing ones.
//
// Advertised Policy
// -----------------
// If a frame has a non-empty required policy, the requirements will be
// advertised on the outgoing HTTP request for any document to be loaded in that
// frame, in the Sec-Required-Document-Policy HTTP header.
//
// Defaults
// --------
// Each defined feature has a default policy, which determines the threshold
// value to use when no policy has been declared.

class BLINK_COMMON_EXPORT DocumentPolicy {
 public:
  using FeatureState =
      base::flat_map<mojom::DocumentPolicyFeature, PolicyValue>;

  // Mapping of feature to endpoint group.
  // https://w3c.github.io/reporting/#endpoint-group
  using FeatureEndpointMap =
      base::flat_map<mojom::DocumentPolicyFeature, std::string>;

  struct ParsedDocumentPolicy {
    FeatureState feature_state;
    FeatureEndpointMap endpoint_map;
  };

  static std::unique_ptr<DocumentPolicy> CreateWithHeaderPolicy(
      const ParsedDocumentPolicy& header_policy);

  // Returns true if the feature is unrestricted (has its default value for the
  // platform)
  bool IsFeatureEnabled(mojom::DocumentPolicyFeature feature) const;

  // Returns true if the feature is unrestricted, or is not restricted as much
  // as the given threshold value.
  bool IsFeatureEnabled(mojom::DocumentPolicyFeature feature,
                        const PolicyValue& threshold_value) const;

  // Returns true if the feature is being migrated to document policy
  // TODO(iclelland): remove this method when those features are fully
  // migrated to document policy.
  bool IsFeatureSupported(mojom::DocumentPolicyFeature feature) const;

  // Returns the value of the given feature on the given origin.
  PolicyValue GetFeatureValue(mojom::DocumentPolicyFeature feature) const;

  // Returns the endpoint the given feature should report to.
  // Returns base::nullopt if the endpoint is unspecified for given feature.
  const base::Optional<std::string> GetFeatureEndpoint(
      mojom::DocumentPolicyFeature feature) const;

  // Returns true if the incoming policy is compatible with the given required
  // policy, i.e. incoming policy is at least as strict as required policy.
  static bool IsPolicyCompatible(const FeatureState& required_policy,
                                 const FeatureState& incoming_policy);

  // Serialize document policy according to http_structured_header.
  // returns base::nullopt when http structured header serializer encounters
  // problems, e.g. double value out of the range supported.
  static base::Optional<std::string> Serialize(const FeatureState& policy);

  static base::Optional<std::string> SerializeInternal(
      const FeatureState& policy,
      const DocumentPolicyFeatureInfoMap&);

  // Merge two FeatureState map. Take stricter value when there is conflict.
  static FeatureState MergeFeatureState(const FeatureState& policy1,
                                        const FeatureState& policy2);

 private:
  friend class DocumentPolicyTest;

  DocumentPolicy(const FeatureState& header_policy,
                 const FeatureEndpointMap& endpoint_map,
                 const FeatureState& defaults);
  static std::unique_ptr<DocumentPolicy> CreateWithHeaderPolicy(
      const FeatureState& header_policy,
      const FeatureEndpointMap& endpoint_map,
      const FeatureState& defaults);

  void UpdateFeatureState(const FeatureState& feature_state);

  // Internal feature state is represented as an array to avoid overhead
  // in using container classes.
  PolicyValue internal_feature_state_
      [static_cast<size_t>(mojom::DocumentPolicyFeature::kMaxValue) + 1];

  FeatureEndpointMap endpoint_map_;

  DISALLOW_COPY_AND_ASSIGN(DocumentPolicy);
};

bool inline operator==(const DocumentPolicy::ParsedDocumentPolicy& lhs,
                       const DocumentPolicy::ParsedDocumentPolicy& rhs) {
  return std::tie(lhs.feature_state, lhs.endpoint_map) ==
         std::tie(rhs.feature_state, rhs.endpoint_map);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_DOCUMENT_POLICY_H_
