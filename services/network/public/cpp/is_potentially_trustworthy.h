// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_IS_POTENTIALLY_TRUSTWORTHY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_IS_POTENTIALLY_TRUSTWORTHY_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "url/gurl.h"

namespace url {
class Origin;
}  // namespace url

namespace network {

// Returns whether an origin is potentially trustworthy according to
// https://www.w3.org/TR/powerful-features/#is-origin-trustworthy.
//
// See also blink::SecurityOrigin::isPotentiallyTrustworthy.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsOriginPotentiallyTrustworthy(const url::Origin& origin);

// Returns whether a URL is potentially trustworthy according to
// https://www.w3.org/TR/powerful-features/#is-url-trustworthy.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsUrlPotentiallyTrustworthy(const GURL& url);

// Return an allowlist of origins and hostname patterns that need to be
// considered trustworthy.  The allowlist is given by the
// --unsafely-treat-insecure-origin-as-secure command-line option. See
// https://www.w3.org/TR/powerful-features/#is-origin-trustworthy.
//
// The allowlist can contain origins and wildcard hostname patterns up to
// eTLD+1. For example, the list may contain "http://foo.com",
// "http://foo.com:8000", "*.foo.com", "*.foo.*.bar.com", and
// "http://*.foo.bar.com", but not "*.co.uk", "*.com", or "test.*.com". Hostname
// patterns must contain a wildcard somewhere (so "test.com" is not a valid
// pattern) and wildcards can only replace full components ("test*.foo.com" is
// not valid).
//
// Plain origins ("http://foo.com") are canonicalized when they are inserted
// into this list by converting to url::Origin and serializing. For hostname
// patterns, each component is individually canonicalized.
//
// This function is safe to be called from any thread in production code (tests
// should see the warning in the ResetSecureOriginAllowlistForTesting comments
// below).
COMPONENT_EXPORT(NETWORK_CPP)
const std::vector<std::string>& GetSecureOriginAllowlist();

// Empties the secure origin allowlist.
//
// Thread-safety warning: Caller needs to ensure that all calls to
// GetSecureOriginAllowlist, IsAllowlistedAsSecureOrigin and
// ResetSecureOriginAllowlistForTesting are done from the same thread.
COMPONENT_EXPORT(NETWORK_CPP) void ResetSecureOriginAllowlistForTesting();

// Returns true if |origin| has a match in |allowlist|.  |allowlist| is usually
// retrieved by GetSecureOriginAllowlist above.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsAllowlistedAsSecureOrigin(const url::Origin& origin,
                                 const std::vector<std::string>& allowlist);

// Parses a comma-separated list of origins and wildcard hostname patterns.
// This separate function allows callers other than GetSecureOriginAllowlist()
// to explicitly pass an allowlist to be parsed.
COMPONENT_EXPORT(NETWORK_CPP)
std::vector<std::string> ParseSecureOriginAllowlist(
    const std::string& origins_str);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_IS_POTENTIALLY_TRUSTWORTHY_H_
