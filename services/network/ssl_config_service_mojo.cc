// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ssl_config_service_mojo.h"

#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/network/ssl_config_type_converter.h"

namespace network {

namespace {

// Returns true if |hostname| is a subdomain of |pattern| (including if they are
// equal).
bool IsSubdomain(const base::StringPiece hostname,
                 const base::StringPiece pattern) {
  if (hostname == pattern) {
    return true;
  }
  if (hostname.length() <= (pattern.length() + 1)) {
    return false;
  }
  if (!hostname.ends_with(pattern)) {
    return false;
  }
  return hostname[hostname.length() - pattern.length() - 1] == '.';
}

}  // namespace

SSLConfigServiceMojo::SSLConfigServiceMojo(
    mojom::SSLConfigPtr initial_config,
    mojom::SSLConfigClientRequest ssl_config_client_request)
    : binding_(this),
      client_cert_pooling_policy_(
          initial_config ? initial_config->client_cert_pooling_policy
                         : std::vector<std::string>()) {
  if (initial_config)
    ssl_config_ = mojo::ConvertTo<net::SSLConfig>(std::move(initial_config));
  if (ssl_config_client_request)
    binding_.Bind(std::move(ssl_config_client_request));
}

SSLConfigServiceMojo::~SSLConfigServiceMojo() = default;

void SSLConfigServiceMojo::OnSSLConfigUpdated(mojom::SSLConfigPtr ssl_config) {
  bool force_notification =
      client_cert_pooling_policy_ != ssl_config->client_cert_pooling_policy;
  client_cert_pooling_policy_ = ssl_config->client_cert_pooling_policy;

  net::SSLConfig old_config = ssl_config_;
  ssl_config_ = mojo::ConvertTo<net::SSLConfig>(std::move(ssl_config));
  ProcessConfigUpdate(old_config, ssl_config_, force_notification);
}

void SSLConfigServiceMojo::GetSSLConfig(net::SSLConfig* ssl_config) {
  *ssl_config = ssl_config_;
}

bool SSLConfigServiceMojo::CanShareConnectionWithClientCerts(
    const std::string& hostname) const {
  // Hostnames (and the patterns configured for this class) must be
  // canonicalized before comparison, or the comparison will fail.
  for (const std::string& pattern : client_cert_pooling_policy_) {
    if (pattern.empty()) {
      continue;
    }
    // If the pattern starts with a '.', |hostname| must match it exactly
    // (except for the leading dot) for the pattern to be a match.
    if (pattern[0] == '.') {
      if (pattern.compare(1, std::string::npos, hostname) == 0) {
        return true;
      } else {
        continue;
      }
    }
    // Patterns that don't start with a dot match subdomains as well as an exact
    // pattern match. For example, a pattern of "example.com" should match a
    // hostname of "example.com", "sub.example.com", but not "notexample.com".
    if (IsSubdomain(hostname, pattern)) {
      return true;
    }
  }
  return false;
}

}  // namespace network
