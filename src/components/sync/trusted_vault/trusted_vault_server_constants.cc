// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_server_constants.h"

#include "base/base64url.h"
#include "net/base/url_util.h"

namespace syncer {

namespace {

const char kQueryParameterAlternateOutputKey[] = "alt";
const char kQueryParameterAlternateOutputProto[] = "proto";

}  // namespace

const int kUnknownConstantKeyVersion = 0;

const char kSyncSecurityDomainName[] = "users/me/securitydomains/chromesync";
const char kSecurityDomainMemberNamePrefix[] = "users/me/members/";
const char kJoinSecurityDomainsURLPath[] =
    "users/me/securitydomains/chromesync:join";
const char kJoinSecurityDomainsErrorDetailTypeURL[] =
    "type.googleapis.com/"
    "google.internal.identity.securitydomain.v1.JoinSecurityDomainErrorDetail";
extern const char kGetSecurityDomainURLPathAndQuery[] =
    "users/me/securitydomains/chromesync?view=2";

std::vector<uint8_t> GetConstantTrustedVaultKey() {
  return std::vector<uint8_t>(16, 0);
}

std::string GetGetSecurityDomainMemberURLPathAndQuery(
    base::span<const uint8_t> public_key) {
  std::string encoded_public_key;
  base::Base64UrlEncode(std::string(public_key.begin(), public_key.end()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);
  return kSecurityDomainMemberNamePrefix + encoded_public_key + "?view=2";
}

GURL GetFullJoinSecurityDomainsURLForTesting(const GURL& server_url) {
  return net::AppendQueryParameter(
      /*url=*/GURL(server_url.spec() + kJoinSecurityDomainsURLPath),
      kQueryParameterAlternateOutputKey, kQueryParameterAlternateOutputProto);
}

GURL GetFullGetSecurityDomainMemberURLForTesting(
    const GURL& server_url,
    base::span<const uint8_t> public_key) {
  return net::AppendQueryParameter(
      /*url=*/GURL(server_url.spec() +
                   GetGetSecurityDomainMemberURLPathAndQuery(public_key)),
      kQueryParameterAlternateOutputKey, kQueryParameterAlternateOutputProto);
}

GURL GetFullGetSecurityDomainURLForTesting(const GURL& server_url) {
  return net::AppendQueryParameter(
      /*url=*/GURL(server_url.spec() + kGetSecurityDomainURLPathAndQuery),
      kQueryParameterAlternateOutputKey, kQueryParameterAlternateOutputProto);
}

}  // namespace syncer
