// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_UTIL_H_
#define NET_COOKIES_COOKIE_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/site_for_cookies.h"
#include "url/origin.h"

class GURL;

namespace net {
namespace cookie_util {

// Constants for use in VLOG
const int kVlogPerCookieMonster = 1;
const int kVlogSetCookies = 7;
const int kVlogGarbageCollection = 5;

// Minimum name length for SameSite compatibility pair heuristic (see
// IsSameSiteCompatPair() below.)
const int kMinCompatPairNameLength = 3;

// This enum must match the numbering for StorageAccessResult in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// at the end.
enum class StorageAccessResult {
  ACCESS_BLOCKED = 0,
  ACCESS_ALLOWED = 1,
  ACCESS_ALLOWED_STORAGE_ACCESS_GRANT = 2,
  kMaxValue = ACCESS_ALLOWED_STORAGE_ACCESS_GRANT,
};
// Helper to fire telemetry indicating if a given request for storage was
// allowed or not by the provided |result|.
NET_EXPORT void FireStorageAccessHistogram(StorageAccessResult result);

// Returns the effective TLD+1 for a given host. This only makes sense for http
// and https schemes. For other schemes, the host will be returned unchanged
// (minus any leading period).
NET_EXPORT std::string GetEffectiveDomain(const std::string& scheme,
                                          const std::string& host);

// Determine the actual cookie domain based on the domain string passed
// (if any) and the URL from which the cookie came.
// On success returns true, and sets cookie_domain to either a
//   -host cookie domain (ex: "google.com")
//   -domain cookie domain (ex: ".google.com")
// On success, DomainIsHostOnly(url.host()) is DCHECKed. The URL's host must not
// begin with a '.' character.
NET_EXPORT bool GetCookieDomainWithString(const GURL& url,
                                          const std::string& domain_string,
                                          std::string* result);

// Returns true if a domain string represents a host-only cookie,
// i.e. it doesn't begin with a leading '.' character.
NET_EXPORT bool DomainIsHostOnly(const std::string& domain_string);

// If |cookie_domain| is nonempty and starts with a "." character, this returns
// the substring of |cookie_domain| without the leading dot. (Note only one
// leading dot is stripped, if there are multiple.) Otherwise it returns
// |cookie_domain|. This is useful for converting from CanonicalCookie's
// representation of a cookie domain to the RFC's notion of a cookie's domain.
NET_EXPORT std::string CookieDomainAsHost(const std::string& cookie_domain);

// Parses the string with the cookie expiration time (very forgivingly).
// Returns the "null" time on failure.
//
// If the expiration date is below or above the platform-specific range
// supported by Time::FromUTCExplodeded(), then this will return Time(1) or
// Time::Max(), respectively.
NET_EXPORT base::Time ParseCookieExpirationTime(const std::string& time_string);

// Get a cookie's URL from it's domain, path, and source scheme.
// The first field can be the combined domain-and-host-only-flag (e.g. the
// string returned by CanonicalCookie::Domain()) as opposed to the domain
// attribute per RFC6265bis. The GURL is constructed after stripping off any
// leading dot.
// Note: the GURL returned by this method is not guaranteed to be valid.
NET_EXPORT GURL CookieDomainAndPathToURL(const std::string& domain,
                                         const std::string& path,
                                         const std::string& source_scheme);
NET_EXPORT GURL CookieDomainAndPathToURL(const std::string& domain,
                                         const std::string& path,
                                         bool is_https);
NET_EXPORT GURL CookieDomainAndPathToURL(const std::string& domain,
                                         const std::string& path,
                                         CookieSourceScheme source_scheme);

// Convenience for converting a cookie origin (domain and https pair) to a URL.
NET_EXPORT GURL CookieOriginToURL(const std::string& domain, bool is_https);

// Returns a URL that could have been the cookie's source.
// Not guaranteed to actually be the URL that set the cookie. Not guaranteed to
// be a valid GURL. Intended as a shim for SetCanonicalCookieAsync calls, where
// a source URL is required but only a source scheme may be available.
NET_EXPORT GURL SimulatedCookieSource(const CanonicalCookie& cookie,
                                      const std::string& source_scheme);

// |domain| is the output of cookie.Domain() for some cookie. This returns true
// if a |domain| indicates that the cookie can be accessed by |host|.
// See comment on CanonicalCookie::IsDomainMatch().
NET_EXPORT bool IsDomainMatch(const std::string& domain,
                              const std::string& host);

// A ParsedRequestCookie consists of the key and value of the cookie.
using ParsedRequestCookie = std::pair<std::string, std::string>;
using ParsedRequestCookies = std::vector<ParsedRequestCookie>;

// Assumes that |header_value| is the cookie header value of a HTTP Request
// following the cookie-string schema of RFC 6265, section 4.2.1, and returns
// cookie name/value pairs. If cookie values are presented in double quotes,
// these will appear in |parsed_cookies| as well. Assumes that the cookie
// header is written by Chromium and therefore well-formed.
NET_EXPORT void ParseRequestCookieLine(const std::string& header_value,
                                       ParsedRequestCookies* parsed_cookies);

// Writes all cookies of |parsed_cookies| into a HTTP Request header value
// that belongs to the "Cookie" header. The entries of |parsed_cookies| must
// already be appropriately escaped.
NET_EXPORT std::string SerializeRequestCookieLine(
    const ParsedRequestCookies& parsed_cookies);

// Determines which of the cookies for |url| can be accessed, with respect to
// the SameSite attribute. This applies to looking up existing cookies; for
// setting new ones, see ComputeSameSiteContextForResponse and
// ComputeSameSiteContextForScriptSet.
//
// |site_for_cookies| is the currently navigated to site that should be
// considered "first-party" for cookies.
//
// |initiator| is the origin ultimately responsible for getting the request
// issued; it may be different from |site_for_cookies| in that it may be some
// other website that caused the navigation to |site_for_cookies| to occur.
//
// base::nullopt for |initiator| denotes that the navigation was initiated by
// the user directly interacting with the browser UI, e.g. entering a URL
// or selecting a bookmark.
//
// If |force_ignore_site_for_cookies| is specified, all SameSite cookies will be
// attached, i.e. this will return SAME_SITE_STRICT. This flag is set to true
// when the |site_for_cookies| is a chrome:// URL embedding a secure origin,
// among other scenarios.
// This is *not* set when the *initiator* is chrome-extension://,
// which is intentional, since it would be bad to let an extension arbitrarily
// redirect anywhere and bypass SameSite=Strict rules.
//
// See also documentation for corresponding methods on net::URLRequest.
//
// |http_method| is used to enforce the requirement that, in a context that's
// lax same-site but not strict same-site, SameSite=lax cookies be only sent
// when the method is "safe" in the RFC7231 section 4.2.1 sense.
NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForRequest(const std::string& http_method,
                                 const GURL& url,
                                 const SiteForCookies& site_for_cookies,
                                 const base::Optional<url::Origin>& initiator,
                                 bool force_ignore_site_for_cookies);

// As above, but applying for scripts. |initiator| here should be the initiator
// used when fetching the document.
// If |force_ignore_site_for_cookies| is true, this returns SAME_SITE_STRICT.
NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForScriptGet(const GURL& url,
                                   const SiteForCookies& site_for_cookies,
                                   const base::Optional<url::Origin>& initiator,
                                   bool force_ignore_site_for_cookies);

// Determines which of the cookies for |url| can be set from a network response,
// with respect to the SameSite attribute. This will only return CROSS_SITE or
// SAME_SITE_LAX (cookie sets of SameSite=strict cookies are permitted in same
// contexts that sets of SameSite=lax cookies are).
// If |force_ignore_site_for_cookies| is true, this returns SAME_SITE_LAX.
NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForResponse(const GURL& url,
                                  const SiteForCookies& site_for_cookies,
                                  const base::Optional<url::Origin>& initiator,
                                  bool force_ignore_site_for_cookies);

// Determines which of the cookies for |url| can be set from a script context,
// with respect to the SameSite attribute. This will only return CROSS_SITE or
// SAME_SITE_LAX (cookie sets of SameSite=strict cookies are permitted in same
// contexts that sets of SameSite=lax cookies are).
// If |force_ignore_site_for_cookies| is true, this returns SAME_SITE_LAX.
NET_EXPORT CookieOptions::SameSiteCookieContext
ComputeSameSiteContextForScriptSet(const GURL& url,
                                   const SiteForCookies& site_for_cookies,
                                   bool force_ignore_site_for_cookies);

// Determines which of the cookies for |url| can be accessed when fetching a
// subresources. This is either CROSS_SITE or SAME_SITE_STRICT,
// since the initiator for a subresource is the frame loading it.
NET_EXPORT CookieOptions::SameSiteCookieContext
// If |force_ignore_site_for_cookies| is true, this returns SAME_SITE_STRICT.
ComputeSameSiteContextForSubresource(const GURL& url,
                                     const SiteForCookies& site_for_cookies,
                                     bool force_ignore_site_for_cookies);

// Evaluates a heuristic to determine whether |c1| and |c2| are likely to be a
// "double cookie" pair used for SameSite=None compatibility reasons.
//
// This returns true if all of the following are true:
//  1. The cookies are not equivalent (i.e. same name, domain, and path).
//  2. One of them is SameSite=None and Secure; the other one has unspecified
//     SameSite.
//  3. Their domains are equal.
//  4. Their paths are equal.
//  5. Their values are equal.
//  6. One of them has a name that is a prefix or suffix of the other and has
//     length at least 3 characters.
//
// |options| is the CookieOptions object used to access (get/set) the cookies.
// If the CookieOptions indicate that HttpOnly cookies are not allowed, this
// will return false if either of |c1| or |c2| is HttpOnly.
NET_EXPORT bool IsSameSiteCompatPair(const CanonicalCookie& c1,
                                     const CanonicalCookie& c2,
                                     const CookieOptions& options);

// Returns whether the respective SameSite feature is enabled.
NET_EXPORT bool IsSameSiteByDefaultCookiesEnabled();
NET_EXPORT bool IsCookiesWithoutSameSiteMustBeSecureEnabled();
NET_EXPORT bool IsSchemefulSameSiteEnabled();

// Takes a callback accepting a CookieAccessResult and returns a callback
// that accepts a bool, setting the bool to true if the CookieInclusionStatus
// in CookieAccessResult was set to "include", else sending false.
//
// Can be used with SetCanonicalCookie when you don't need to know why a cookie
// was blocked, only whether it was blocked.
NET_EXPORT base::OnceCallback<void(CookieAccessResult)>
AdaptCookieAccessResultToBool(base::OnceCallback<void(bool)> callback);

// Turn a CookieAccessResultList into a CookieList by stripping out access
// results (for callers who only care about cookies).
NET_EXPORT CookieList
StripAccessResults(const CookieAccessResultList& cookie_access_result_list);

// Records port related metrics from Omnibox navigations.
NET_EXPORT void RecordCookiePortOmniboxHistograms(const GURL& url);

}  // namespace cookie_util
}  // namespace net

#endif  // NET_COOKIES_COOKIE_UTIL_H_
