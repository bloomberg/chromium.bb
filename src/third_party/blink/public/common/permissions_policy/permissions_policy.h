// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_H_

#include <map>
#include <vector>

#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"
#include "url/origin.h"

namespace blink {

// Permissions Policy is a mechanism for controlling the availability of web
// platform features in a frame, including all embedded frames. It can be used
// to remove features, automatically refuse API permission requests, or modify
// the behaviour of features. (The specific changes which are made depend on the
// feature; see the specification for details).
//
// Policies can be defined in the HTTP header stream, with the
// |Permissions-Policy| HTTP header, or can be set by the |allow| attributes on
// the iframe element which embeds the document.
//
// See https://w3c.github.io/webappsec-permissions-policy/
//
// Key concepts:
//
// Features
// --------
// Features which can be controlled by policy are defined by instances of enum
// mojom::PermissionsPolicyFeature, declared in |permissions_policy.mojom|.
//
// Allowlists
// ----------
// Allowlists are collections of origins, although several special terms can be
// used when declaring them:
//   "none" indicates that no origins should be included in the allowlist.
//   "self" refers to the origin of the frame which is declaring the policy.
//   "src" refers to the origin specified by the attributes of the iframe
//   element which embeds the document. This incorporates the src, srcdoc, and
//   sandbox attributes.
//   "*" refers to all origins; any origin will match an allowlist which
//   contains it.
//
// Declarations
// ------------
// A permissions policy declaration is a mapping of a feature name to an
// allowlist. A set of declarations is a declared policy.
//
// Inherited Policy
// ----------------
// In addition to the declared policy (which may be empty), every frame has
// an inherited policy, which is determined by the context in which it is
// embedded, or by the defaults for each feature in the case of the top-level
// document.
//
// Container Policy
// ----------------
// A declared policy can be set on a specific frame by the embedding page using
// the iframe "allow" attribute, or through attributes such as "allowfullscreen"
// or "allowpaymentrequest". This is the container policy for the embedded
// frame.
//
// Defaults
// --------
// Each defined feature has a default policy, which determines whether the
// feature is available when no policy has been declared, and determines how the
// feature is inherited across origin boundaries.
//
// If the default policy is in effect for a frame, then it controls how the
// feature is inherited by any cross-origin iframes embedded by the frame. (See
// the comments in |PermissionsPolicyFeatureDefault| in
// permissions_policy_features.h for specifics.)
//
// Policy Inheritance
// ------------------
// Policies in effect for a frame are inherited by any child frames it embeds.
// Unless another policy is declared in the child, all same-origin children will
// receive the same set of enabled features as the parent frame. Whether or not
// features are inherited by cross-origin iframes without an explicit policy is
// determined by the feature's default policy. (Again, see the comments in
// |PermissionsPolicyFeatureDefault| in permissions_policy_features.h for
// details)

// This struct holds permissions policy allowlist data that needs to be
// replicated between a RenderFrame and any of its associated
// RenderFrameProxies. A list of these form a ParsedPermissionsPolicy. NOTE:
// These types are used for replication frame state between processes.
struct BLINK_COMMON_EXPORT ParsedPermissionsPolicyDeclaration {
  ParsedPermissionsPolicyDeclaration();
  explicit ParsedPermissionsPolicyDeclaration(
      mojom::PermissionsPolicyFeature feature);
  ParsedPermissionsPolicyDeclaration(mojom::PermissionsPolicyFeature feature,
                                     const std::vector<url::Origin>& values,
                                     bool matches_all_origins,
                                     bool matches_opaque_src);
  ParsedPermissionsPolicyDeclaration(
      const ParsedPermissionsPolicyDeclaration& rhs);
  ParsedPermissionsPolicyDeclaration& operator=(
      const ParsedPermissionsPolicyDeclaration& rhs);
  ~ParsedPermissionsPolicyDeclaration();

  mojom::PermissionsPolicyFeature feature;

  // An alphabetically sorted list of all the origins allowed.
  std::vector<url::Origin> allowed_origins;
  // Fallback value is used when feature is enabled for all or disabled for all.
  bool matches_all_origins{false};
  // This flag is set true for a declared policy on an <iframe sandbox>
  // container, for a feature which is supposed to be allowed in the sandboxed
  // document. Usually, the 'src' keyword in a declaration will cause the origin
  // of the iframe to be present in |origins|, but for sandboxed iframes, this
  // flag is set instead.
  bool matches_opaque_src{false};
};

using ParsedPermissionsPolicy = std::vector<ParsedPermissionsPolicyDeclaration>;

bool BLINK_COMMON_EXPORT
operator==(const ParsedPermissionsPolicyDeclaration& lhs,
           const ParsedPermissionsPolicyDeclaration& rhs);

class BLINK_COMMON_EXPORT PermissionsPolicy {
 public:
  // Represents a collection of origins which make up an allowlist in a
  // permissions policy. This collection may be set to match every origin
  // (corresponding to the "*" syntax in the policy string, in which case the
  // Contains() method will always return true.
  class BLINK_COMMON_EXPORT Allowlist final {
   public:
    Allowlist();
    Allowlist(const Allowlist& rhs);
    ~Allowlist();

    // Adds a single origin to the allowlist.
    void Add(const url::Origin& origin);

    // Adds all origins to the allowlist.
    void AddAll();

    // Sets the allowlist to match the opaque origin implied by the 'src'
    // keyword.
    void AddOpaqueSrc();

    // Returns true if the given origin has been added to the allowlist.
    bool Contains(const url::Origin& origin) const;

    // Returns true if the allowlist matches all origins.
    bool MatchesAll() const;

    // Returns true if the allowlist should match the opaque origin implied by
    // the 'src' keyword.
    bool MatchesOpaqueSrc() const;

    const std::vector<url::Origin>& AllowedOrigins() const {
      return allowed_origins_;
    }

   private:
    std::vector<url::Origin> allowed_origins_;
    bool matches_all_origins_{false};
    bool matches_opaque_src_{false};
  };

  PermissionsPolicy(const PermissionsPolicy&) = delete;
  PermissionsPolicy& operator=(const PermissionsPolicy&) = delete;
  ~PermissionsPolicy();

  static std::unique_ptr<PermissionsPolicy> CreateFromParentPolicy(
      const PermissionsPolicy* parent_policy,
      const ParsedPermissionsPolicy& container_policy,
      const url::Origin& origin);

  static std::unique_ptr<PermissionsPolicy> CopyStateFrom(
      const PermissionsPolicy*);

  // Creates a PermissionsPolicy for a fenced frame. All permissions are
  // disabled in fenced frames.
  static std::unique_ptr<PermissionsPolicy> CreateForFencedFrame(
      const url::Origin& origin);

  bool IsFeatureEnabled(mojom::PermissionsPolicyFeature feature) const;

  // Returns whether or not the given feature is enabled by this policy for a
  // specific origin.
  bool IsFeatureEnabledForOrigin(mojom::PermissionsPolicyFeature feature,
                                 const url::Origin& origin) const;

  const Allowlist GetAllowlistForDevTools(
      mojom::PermissionsPolicyFeature feature) const;

  // Returns the allowlist of a given feature by this policy.
  // TODO(crbug.com/937131): Use |PermissionsPolicy::GetAllowlistForDevTools|
  // to replace this method. This method uses legacy |default_allowlist|
  // calculation method.
  const Allowlist GetAllowlistForFeature(
      mojom::PermissionsPolicyFeature feature) const;

  // Returns the allowlist of a given feature if it already exists. Doesn't
  // build a default allow list based on the policy if not.
  absl::optional<const Allowlist> GetAllowlistForFeatureIfExists(
      mojom::PermissionsPolicyFeature feature) const;

  // Sets the declared policy from the parsed Permissions-Policy HTTP header.
  // Unrecognized features will be ignored.
  void SetHeaderPolicy(const ParsedPermissionsPolicy& parsed_header);

  // Used to update a client hint header policy set via the accept-ch meta tag.
  // It will fail if header policies not for client hints are included in
  // `parsed_header` or if allowlists_ has already been used for a check.
  // TODO(crbug.com/1278127): Replace w/ generic HTML policy modification.
  void OverwriteHeaderPolicyForClientHints(
      const ParsedPermissionsPolicy& parsed_header);

  // Returns the current state of permissions policies for |origin_|. This
  // includes the |inherited_policies_| as well as the header policies.
  PermissionsPolicyFeatureState GetFeatureState() const;

  const url::Origin& GetOriginForTest() const { return origin_; }

  // Returns the list of features which can be controlled by Permissions Policy.
  const PermissionsPolicyFeatureList& GetFeatureList() const;

  bool IsFeatureEnabledByInheritedPolicy(
      mojom::PermissionsPolicyFeature feature) const;

 private:
  friend class PermissionsPolicyTest;

  PermissionsPolicy(url::Origin origin,
                    const PermissionsPolicyFeatureList& feature_list);
  static std::unique_ptr<PermissionsPolicy> CreateFromParentPolicy(
      const PermissionsPolicy* parent_policy,
      const ParsedPermissionsPolicy& container_policy,
      const url::Origin& origin,
      const PermissionsPolicyFeatureList& features);

  static std::unique_ptr<PermissionsPolicy> CreateForFencedFrame(
      const url::Origin& origin,
      const PermissionsPolicyFeatureList& features);

  bool InheritedValueForFeature(
      const PermissionsPolicy* parent_policy,
      std::pair<mojom::PermissionsPolicyFeature,
                PermissionsPolicyFeatureDefault> feature,
      const ParsedPermissionsPolicy& container_policy) const;

  // Returns the value of the given feature on the given origin.
  bool GetFeatureValueForOrigin(mojom::PermissionsPolicyFeature feature,
                                const url::Origin& origin) const;

  // The origin of the document with which this policy is associated.
  url::Origin origin_;

  // Map of feature names to declared allowlists. Any feature which is missing
  // from this map should use the inherited policy.
  std::map<mojom::PermissionsPolicyFeature, Allowlist> allowlists_;

  // Set this to true if `allowlists_` have already been checked.
  mutable bool allowlists_checked_;

  // Records whether or not each feature was enabled for this frame by its
  // parent frame.
  PermissionsPolicyFeatureState inherited_policies_;

  const PermissionsPolicyFeatureList& feature_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_H_
