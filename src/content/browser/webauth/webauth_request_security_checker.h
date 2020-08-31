// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_WEBAUTH_REQUEST_SECURITY_CHECKER_H_
#define CONTENT_BROWSER_WEBAUTH_WEBAUTH_REQUEST_SECURITY_CHECKER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace url {
class Origin;
}

namespace content {

class RenderFrameHost;

// The following enums correspond to UMA histograms and should not be
// reassigned.
enum class RelyingPartySecurityCheckFailure {
  kOpaqueOrNonSecureOrigin = 0,
  kRelyingPartyIdInvalid = 1,
  kAppIdExtensionInvalid = 2,
  kAppIdExtensionDomainMismatch = 3,
  kIconUrlInvalid = 4,
  kCrossOriginMismatch = 5,
  kMaxValue = kCrossOriginMismatch,
};

// A centralized class for enforcing security policies that apply to
// Web Authentication requests to create credentials or get authentication
// assertions. For security reasons it is important that these checks are
// performed in the browser process, and this makes the verification code
// available to both the desktop and Android implementations of the
// |Authenticator| mojom interface.
class CONTENT_EXPORT WebAuthRequestSecurityChecker
    : public base::RefCounted<WebAuthRequestSecurityChecker> {
 public:
  enum class RequestType { kMakeCredential, kGetAssertion };

  explicit WebAuthRequestSecurityChecker(RenderFrameHost* host);
  WebAuthRequestSecurityChecker(const WebAuthRequestSecurityChecker&) = delete;

  WebAuthRequestSecurityChecker& operator=(
      const WebAuthRequestSecurityChecker&) = delete;

  static void ReportSecurityCheckFailure(
      RelyingPartySecurityCheckFailure error);
  static bool OriginIsCryptoTokenExtension(const url::Origin& origin);

  // Returns blink::mojom::AuthenticatorStatus::SUCCESS if |origin| is
  // same-origin with all ancestors in the frame tree, or else if
  // requests from cross-origin embeddings are allowed by policy and the
  // RequestType is |kGetAssertion|.
  // Returns blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR otherwise.
  // |is_cross_origin| is an output parameter that is set to true if there is
  // a cross-origin embedding, regardless of policy, and false otherwise.
  blink::mojom::AuthenticatorStatus ValidateAncestorOrigins(
      const url::Origin& origin,
      RequestType type,
      bool* is_cross_origin);

  // Returns AuthenticatorStatus::SUCCESS if the origin domain is valid under
  // the referenced definitions, and also the requested RP ID is a registrable
  // domain suffix of, or is equal to, the origin's effective domain.
  // References:
  //   https://url.spec.whatwg.org/#valid-domain-string
  //   https://html.spec.whatwg.org/multipage/origin.html#concept-origin-effective-domain
  //   https://html.spec.whatwg.org/multipage/origin.html#is-a-registrable-domain-suffix-of-or-is-equal-to
  blink::mojom::AuthenticatorStatus ValidateDomainAndRelyingPartyID(
      const url::Origin& caller_origin,
      const std::string& relying_party_id);

  // Checks whether a given URL is an a-priori authenticated URL.
  // https://w3c.github.io/webappsec-credential-management/#dom-credentialuserdata-iconurl
  blink::mojom::AuthenticatorStatus ValidateAPrioriAuthenticatedUrl(
      const GURL& url);

 protected:
  friend class RefCounted<WebAuthRequestSecurityChecker>;
  virtual ~WebAuthRequestSecurityChecker();

 private:
  // Returns whether the frame indicated by |host| is same-origin with its
  // entire ancestor chain. |origin| is the origin of the frame being checked.
  bool IsSameOriginWithAncestors(const url::Origin& origin);

  RenderFrameHost* render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_WEBAUTH_REQUEST_SECURITY_CHECKER_H_
