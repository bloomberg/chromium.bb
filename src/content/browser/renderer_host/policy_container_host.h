// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_HOST_H_

#include <iosfwd>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom.h"

namespace content {

// The contents of a PolicyContainerHost.
struct CONTENT_EXPORT PolicyContainerPolicies {
  PolicyContainerPolicies();
  PolicyContainerPolicies(
      network::mojom::ReferrerPolicy referrer_policy,
      network::mojom::IPAddressSpace ip_address_space,
      bool is_web_secure_context,
      std::vector<network::mojom::ContentSecurityPolicyPtr>
          content_security_policies,
      const network::CrossOriginOpenerPolicy& cross_origin_opener_policy);
  PolicyContainerPolicies(const PolicyContainerPolicies&) = delete;
  PolicyContainerPolicies operator=(const PolicyContainerPolicies&) = delete;
  ~PolicyContainerPolicies();

  // Creates a clone of this PolicyContainerPolicies. Always returns a non-null
  // pointer.
  std::unique_ptr<PolicyContainerPolicies> Clone() const;

  // Helper function to append items to `content_security_policies`.
  void AddContentSecurityPolicies(
      std::vector<network::mojom::ContentSecurityPolicyPtr> policies);

  // The referrer policy for the associated document. If not overwritten via a
  // call to SetReferrerPolicy (for example after parsing the Referrer-Policy
  // header or a meta tag), the default referrer policy will be applied to the
  // document.
  network::mojom::ReferrerPolicy referrer_policy =
      network::mojom::ReferrerPolicy::kDefault;

  // The IPAddressSpace associated with the document. In all non-network pages
  // (srcdoc, data urls, etc.) where we don't have an IP address to work with,
  // it is inherited following the general rules of the PolicyContainerHost.
  network::mojom::IPAddressSpace ip_address_space =
      network::mojom::IPAddressSpace::kUnknown;

  // Whether the document is a secure context.
  //
  // See: https://html.spec.whatwg.org/#secure-contexts.
  //
  // See also:
  //  - |network::IsUrlPotentiallyTrustworthy()|
  //  - |network::IsOriginPotentiallyTrustworthy()|
  bool is_web_secure_context = false;

  // The content security policies of the associated document.
  std::vector<network::mojom::ContentSecurityPolicyPtr>
      content_security_policies;

  // The cross-origin-opener-policy (COOP) of the document
  // See:
  // https://html.spec.whatwg.org/multipage/origin.html#cross-origin-opener-policies
  network::CrossOriginOpenerPolicy cross_origin_opener_policy;
};

// PolicyContainerPolicies structs are comparable for equality.
CONTENT_EXPORT bool operator==(const PolicyContainerPolicies& lhs,
                               const PolicyContainerPolicies& rhs);
CONTENT_EXPORT bool operator!=(const PolicyContainerPolicies& lhs,
                               const PolicyContainerPolicies& rhs);

// Streams a human-readable string representation of |policies| to |out|.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const PolicyContainerPolicies& policies);

// PolicyContainerHost serves as a container for several security policies. It
// should be owned by a RenderFrameHost. It keep tracks of the policies assigned
// to a document. When a document creates/opens another document with a local
// scheme (about:blank, about:srcdoc, data, blob, filesystem), the
// PolicyContainerHost of the opener is cloned and a copy is attached to the new
// document, so that the same security policies are applied to it. It implements
// a mojo interface that allows updates coming from Blink.
//
// Although it is owned through a scoped_refptr, a PolicyContainerHost should
// not be shared between different owners. A RenderFrameHost gets a
// PolicyContainerHost at creation time, and it gets a new one from the
// NavigationRequest every time a NavigationRequest commits. Initially, a
// PolicyContainerHost has no associated frame token. As soon as the
// PolicyContainerHost becomes owned by a RenderFrameHost, the method
// AssociateWithFrameToken must be called. This makes it possible to retrieve
// the PolicyContainerHost via
// PolicyContainerHost::FromFrameToken. Additionally, this enables the
// PolicyContainerHost to outlive its RenderFrameHost. In fact, as long as the
// mojo receiver or a keep alive handle (as registered using
// IssueKeepAliveHandle) is alive, the PolicyContainerHost will still be
// retrievable by the corresponding frame token even if the RenderFrameHost has
// been deleted (and the scoped_refptr with it).
class CONTENT_EXPORT PolicyContainerHost
    : public base::RefCounted<PolicyContainerHost>,
      public blink::mojom::PolicyContainerHost {
 public:
  // Constructs a PolicyContainerHost containing default policies and an unbound
  // mojo receiver.
  PolicyContainerHost();

  // Constructs a PolicyContainerHost containing the given |policies|, which
  // must not be null.
  explicit PolicyContainerHost(
      std::unique_ptr<PolicyContainerPolicies> policies);

  // PolicyContainerHost instances are neither copyable nor movable.
  PolicyContainerHost(const PolicyContainerHost&) = delete;
  PolicyContainerHost& operator=(const PolicyContainerHost&) = delete;

  // Retrieve the PolicyContainerHost associated with the frame token |token|
  // (cf. AsssociateWithFrameToken).
  static PolicyContainerHost* FromFrameToken(
      const blink::LocalFrameToken& token);

  // AssociateWithFrameToken must be called as soon as this PolicyContainerHost
  // becomes owned by a RenderFrameHost. After this function is called, it
  // becomes possible to retrieve this PolicyContainerHost via
  // PolicyContainerHost::FromFrameToken. This function can be called only once.
  void AssociateWithFrameToken(const blink::LocalFrameToken& token);

  const PolicyContainerPolicies& policies() const { return *policies_; }

  network::mojom::ReferrerPolicy referrer_policy() const {
    return policies_->referrer_policy;
  }

  network::mojom::IPAddressSpace ip_address_space() const {
    return policies_->ip_address_space;
  }

  network::CrossOriginOpenerPolicy cross_origin_opener_policy() const {
    return policies_->cross_origin_opener_policy;
  }

  void AddContentSecurityPolicies(
      std::vector<network::mojom::ContentSecurityPolicyPtr>
          content_security_policies) final;

  void set_cross_origin_opener_policy(
      const network::CrossOriginOpenerPolicy& policy) {
    policies_->cross_origin_opener_policy = policy;
  }

  // Return a PolicyContainer containing copies of the policies and a pending
  // mojo remote that can be used to update policies in this object. If called a
  // second time, it resets the receiver and creates a new PolicyContainer,
  // invalidating the remote of the previous one.
  blink::mojom::PolicyContainerPtr CreatePolicyContainerForBlink();

  // Create a new PolicyContainerHost with the same policies (i.e. a deep copy),
  // but with a new, unbound mojo receiver.
  scoped_refptr<PolicyContainerHost> Clone() const;

  // Bind this PolicyContainerHost with the given mojo receiver, so that it can
  // handle mojo messages coming from the corresponding remote.
  void Bind(
      blink::mojom::PolicyContainerBindParamsPtr policy_container_bind_params);

  // Register a keep alive handle by passing the mojo receiver. The
  // PolicyContainerHost is kept alive as long as the corresponding remote
  // exists.
  // See also:
  // - PolicyContainerHost::AssociateWithFrameToken(token)
  // - PolicyContainerHost::FromFrameToken(token)
  void IssueKeepAliveHandle(
      mojo::PendingReceiver<blink::mojom::PolicyContainerHostKeepAliveHandle>
          receiver) override;

 private:
  friend class base::RefCounted<PolicyContainerHost>;
  ~PolicyContainerHost() override;

  void SetReferrerPolicy(network::mojom::ReferrerPolicy referrer_policy) final;

  // The policies of this PolicyContainerHost. This is never null.
  std::unique_ptr<PolicyContainerPolicies> policies_;

  mojo::AssociatedReceiver<blink::mojom::PolicyContainerHost>
      policy_container_host_receiver_{this};

  mojo::UniqueReceiverSet<blink::mojom::PolicyContainerHostKeepAliveHandle>
      keep_alive_handles_receiver_set_;

  absl::optional<blink::LocalFrameToken> frame_token_ = absl::nullopt;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_HOST_H_
