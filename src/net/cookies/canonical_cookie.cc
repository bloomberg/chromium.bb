// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions of this code based on Mozilla:
//   (netwerk/cookie/src/nsCookieService.cpp)
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/cookies/canonical_cookie.h"

#include <limits>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/features.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

using base::Time;

namespace net {

namespace {

// Determine the cookie domain to use for setting the specified cookie.
bool GetCookieDomain(const GURL& url,
                     const ParsedCookie& pc,
                     std::string* result) {
  std::string domain_string;
  if (pc.HasDomain())
    domain_string = pc.Domain();
  return cookie_util::GetCookieDomainWithString(url, domain_string, result);
}

// Compares cookies using name, domain and path, so that "equivalent" cookies
// (per RFC 2965) are equal to each other.
int PartialCookieOrdering(const CanonicalCookie& a, const CanonicalCookie& b) {
  int diff = a.Name().compare(b.Name());
  if (diff != 0)
    return diff;

  diff = a.Domain().compare(b.Domain());
  if (diff != 0)
    return diff;

  return a.Path().compare(b.Path());
}

void AppendCookieLineEntry(const CanonicalCookie& cookie,
                           std::string* cookie_line) {
  if (!cookie_line->empty())
    *cookie_line += "; ";
  // In Mozilla, if you set a cookie like "AAA", it will have an empty token
  // and a value of "AAA". When it sends the cookie back, it will send "AAA",
  // so we need to avoid sending "=AAA" for a blank token value.
  if (!cookie.Name().empty())
    *cookie_line += cookie.Name() + "=";
  *cookie_line += cookie.Value();
}

// Captures Strict -> Lax context downgrade with Strict cookie
bool IsBreakingStrictToLaxDowngrade(
    CookieOptions::SameSiteCookieContext::ContextType context,
    CookieOptions::SameSiteCookieContext::ContextType schemeful_context,
    CookieEffectiveSameSite effective_same_site,
    bool is_cookie_being_set) {
  if (context ==
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT &&
      schemeful_context ==
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX &&
      effective_same_site == CookieEffectiveSameSite::STRICT_MODE) {
    // This downgrade only applies when a SameSite=Strict cookie is being sent.
    // A Strict -> Lax downgrade will not affect a Strict cookie which is being
    // set because it will be set in either context.
    return !is_cookie_being_set;
  }

  return false;
}

// Captures Strict -> Cross-site context downgrade with {Strict, Lax} cookie
// Captures Strict -> Lax Unsafe context downgrade with {Strict, Lax} cookie.
// This is treated as a cross-site downgrade due to the Lax Unsafe context
// behaving like cross-site.
bool IsBreakingStrictToCrossDowngrade(
    CookieOptions::SameSiteCookieContext::ContextType context,
    CookieOptions::SameSiteCookieContext::ContextType schemeful_context,
    CookieEffectiveSameSite effective_same_site) {
  bool breaking_schemeful_context =
      schemeful_context ==
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE ||
      schemeful_context == CookieOptions::SameSiteCookieContext::ContextType::
                               SAME_SITE_LAX_METHOD_UNSAFE;

  bool strict_lax_enforcement =
      effective_same_site == CookieEffectiveSameSite::STRICT_MODE ||
      effective_same_site == CookieEffectiveSameSite::LAX_MODE ||
      // Treat LAX_MODE_ALLOW_UNSAFE the same as LAX_MODE for the purposes of
      // our SameSite enforcement check.
      effective_same_site == CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE;

  if (context ==
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT &&
      breaking_schemeful_context && strict_lax_enforcement) {
    return true;
  }

  return false;
}

// Captures Lax -> Cross context downgrade with {Strict, Lax} cookies.
// Ignores Lax Unsafe context.
bool IsBreakingLaxToCrossDowngrade(
    CookieOptions::SameSiteCookieContext::ContextType context,
    CookieOptions::SameSiteCookieContext::ContextType schemeful_context,
    CookieEffectiveSameSite effective_same_site,
    bool is_cookie_being_set) {
  bool lax_enforcement =
      effective_same_site == CookieEffectiveSameSite::LAX_MODE ||
      // Treat LAX_MODE_ALLOW_UNSAFE the same as LAX_MODE for the purposes of
      // our SameSite enforcement check.
      effective_same_site == CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE;

  if (context ==
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX &&
      schemeful_context ==
          CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE) {
    // For SameSite=Strict cookies this downgrade only applies when it is being
    // set. A Lax -> Cross downgrade will not affect a Strict cookie which is
    // being sent because it wouldn't be sent in either context.
    return effective_same_site == CookieEffectiveSameSite::STRICT_MODE
               ? is_cookie_being_set
               : lax_enforcement;
  }

  return false;
}

void ApplySameSiteCookieWarningToStatus(
    CookieSameSite samesite,
    CookieEffectiveSameSite effective_samesite,
    bool is_secure,
    const CookieOptions::SameSiteCookieContext& same_site_context,
    CookieInclusionStatus* status,
    bool is_cookie_being_set) {
  if (samesite == CookieSameSite::UNSPECIFIED &&
      same_site_context.GetContextForCookieInclusion() <
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX) {
    status->AddWarningReason(
        CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT);
  }
  if (effective_samesite == CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE &&
      same_site_context.GetContextForCookieInclusion() ==
          CookieOptions::SameSiteCookieContext::ContextType::
              SAME_SITE_LAX_METHOD_UNSAFE) {
    // This warning is more specific so remove the previous, more general,
    // warning.
    status->RemoveWarningReason(
        CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT);
    status->AddWarningReason(
        CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE);
  }
  if (samesite == CookieSameSite::NO_RESTRICTION && !is_secure) {
    status->AddWarningReason(
        CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE);
  }

  // Add a warning if the cookie would be accessible in
  // |same_site_context|::context but not in
  // |same_site_context|::schemeful_context.
  if (IsBreakingStrictToLaxDowngrade(same_site_context.context(),
                                     same_site_context.schemeful_context(),
                                     effective_samesite, is_cookie_being_set)) {
    status->AddWarningReason(
        CookieInclusionStatus::WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE);
  } else if (IsBreakingStrictToCrossDowngrade(
                 same_site_context.context(),
                 same_site_context.schemeful_context(), effective_samesite)) {
    // Which warning to apply depends on the SameSite value.
    if (effective_samesite == CookieEffectiveSameSite::STRICT_MODE) {
      status->AddWarningReason(
          CookieInclusionStatus::WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE);
    } else {
      // LAX_MODE or LAX_MODE_ALLOW_UNSAFE.
      status->AddWarningReason(
          CookieInclusionStatus::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE);
    }

  } else if (IsBreakingLaxToCrossDowngrade(
                 same_site_context.context(),
                 same_site_context.schemeful_context(), effective_samesite,
                 is_cookie_being_set)) {
    // Which warning to apply depends on the SameSite value.
    if (effective_samesite == CookieEffectiveSameSite::STRICT_MODE) {
      status->AddWarningReason(
          CookieInclusionStatus::WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE);
    } else {
      // LAX_MODE or LAX_MODE_ALLOW_UNSAFE.
      // This warning applies to both set/send.
      status->AddWarningReason(
          CookieInclusionStatus::WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE);
    }
  }

  // Apply warning for whether inclusion was changed by considering redirects
  // for the SameSite context calculation. This does not look at the actual
  // inclusion or exclusion, but only at whether the inclusion differs between
  // considering redirects and not.
  using ContextDowngradeType = CookieOptions::SameSiteCookieContext::
      ContextMetadata::ContextDowngradeType;
  const auto& metadata = same_site_context.GetMetadataForCurrentSchemefulMode();
  bool apply_cross_site_redirect_downgrade_warning = false;
  switch (effective_samesite) {
    case CookieEffectiveSameSite::STRICT_MODE:
      // Strict contexts are all normalized to lax for cookie writes, so a
      // strict-to-{lax,cross} downgrade cannot occur for response cookies.
      apply_cross_site_redirect_downgrade_warning =
          is_cookie_being_set ? metadata.cross_site_redirect_downgrade ==
                                    ContextDowngradeType::kLaxToCross
                              : (metadata.cross_site_redirect_downgrade ==
                                     ContextDowngradeType::kStrictToLax ||
                                 metadata.cross_site_redirect_downgrade ==
                                     ContextDowngradeType::kStrictToCross);
      break;
    case CookieEffectiveSameSite::LAX_MODE:
    case CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE:
      // Note that a lax-to-cross downgrade can only happen for response
      // cookies, because a laxly same-site context only happens for a safe
      // top-level cross-site request, which cannot be downgraded due to a
      // cross-site redirect to a non-top-level or unsafe cross-site request.
      apply_cross_site_redirect_downgrade_warning =
          metadata.cross_site_redirect_downgrade ==
          (is_cookie_being_set ? ContextDowngradeType::kLaxToCross
                               : ContextDowngradeType::kStrictToCross);
      break;
    default:
      break;
  }
  if (apply_cross_site_redirect_downgrade_warning) {
    status->AddWarningReason(
        CookieInclusionStatus::
            WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION);
  }

  // If there are reasons to exclude the cookie other than SameSite, don't warn
  // about the cookie at all.
  status->MaybeClearSameSiteWarning();
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SameSiteNonePartyContextType {
  // SameSite=None was required in order for the cookie to be included.
  kSameSiteNoneRequired = 0,
  // The cookie would have been included if it were SameParty (using only the
  // top frame and resource URL).
  kSamePartyTopResource = 1,
  // The cookie would have been included if it were SameParty (using the
  // resource URL and all frame ancestors).
  kSamePartyAncestors = 2,
  // The cookie would have been included if it were SameSite=Lax.
  kSameSiteLax = 3,
  // The cookie would have been included if it were SameSite=Strict.
  kSameSiteStrict = 4,
  kMaxValue = kSameSiteStrict
};

void RecordSameSiteNoneReadContextMetric(SameSiteNonePartyContextType type) {
  UMA_HISTOGRAM_ENUMERATION("Cookie.SameSiteNone.PartyContext.Read", type);
}

void RecordSameSiteNoneWriteContextMetric(SameSiteNonePartyContextType type) {
  UMA_HISTOGRAM_ENUMERATION("Cookie.SameSiteNone.PartyContext.Write", type);
}

}  // namespace

CookieAccessParams::CookieAccessParams(CookieAccessSemantics access_semantics,
                                       bool delegate_treats_url_as_trustworthy,
                                       CookieSamePartyStatus same_party_status)
    : access_semantics(access_semantics),
      delegate_treats_url_as_trustworthy(delegate_treats_url_as_trustworthy),
      same_party_status(same_party_status) {}

CanonicalCookie::CanonicalCookie() = default;

CanonicalCookie::CanonicalCookie(const CanonicalCookie& other) = default;

CanonicalCookie::CanonicalCookie(CanonicalCookie&& other) = default;

CanonicalCookie& CanonicalCookie::operator=(const CanonicalCookie& other) =
    default;

CanonicalCookie& CanonicalCookie::operator=(CanonicalCookie&& other) = default;

CanonicalCookie::CanonicalCookie(
    std::string name,
    std::string value,
    std::string domain,
    std::string path,
    base::Time creation,
    base::Time expiration,
    base::Time last_access,
    bool secure,
    bool httponly,
    CookieSameSite same_site,
    CookiePriority priority,
    bool same_party,
    absl::optional<CookiePartitionKey> partition_key,
    CookieSourceScheme source_scheme,
    int source_port)
    : name_(std::move(name)),
      value_(std::move(value)),
      domain_(std::move(domain)),
      path_(std::move(path)),
      creation_date_(creation),
      expiry_date_(expiration),
      last_access_date_(last_access),
      secure_(secure),
      httponly_(httponly),
      same_site_(same_site),
      priority_(priority),
      same_party_(same_party),
      partition_key_(std::move(partition_key)),
      source_scheme_(source_scheme) {
  SetSourcePort(source_port);
}

CanonicalCookie::~CanonicalCookie() = default;

// static
std::string CanonicalCookie::CanonPathWithString(
    const GURL& url,
    const std::string& path_string) {
  // The path was supplied in the cookie, we'll take it.
  if (!path_string.empty() && path_string[0] == '/')
    return path_string;

  // The path was not supplied in the cookie or invalid, we will default
  // to the current URL path.
  // """Defaults to the path of the request URL that generated the
  //    Set-Cookie response, up to, but not including, the
  //    right-most /."""
  // How would this work for a cookie on /?  We will include it then.
  const std::string& url_path = url.path();

  size_t idx = url_path.find_last_of('/');

  // The cookie path was invalid or a single '/'.
  if (idx == 0 || idx == std::string::npos)
    return std::string("/");

  // Return up to the rightmost '/'.
  return url_path.substr(0, idx);
}

// static
Time CanonicalCookie::CanonExpiration(const ParsedCookie& pc,
                                      const Time& current,
                                      const Time& server_time) {
  // First, try the Max-Age attribute.
  if (pc.HasMaxAge()) {
    int64_t max_age = 0;
    // Use the output if StringToInt64 returns true ("perfect" conversion). This
    // case excludes overflow/underflow, leading/trailing whitespace, non-number
    // strings, and empty string. (ParsedCookie trims whitespace.)
    if (base::StringToInt64(pc.MaxAge(), &max_age)) {
      // RFC 6265bis algorithm for parsing Max-Age:
      // "If delta-seconds is less than or equal to zero (0), let expiry-
      // time be the earliest representable date and time. ... "
      if (max_age <= 0)
        return Time::Min();
      // "... Otherwise, let the expiry-time be the current date and time plus
      // delta-seconds seconds."
      return current + base::Seconds(max_age);
    } else {
      // If the conversion wasn't perfect, but the best-effort conversion
      // resulted in an overflow/underflow, use the min/max representable time.
      // (This is alluded to in the spec, which says the user agent MAY clip an
      // Expires attribute to a saturated time. We'll do the same for Max-Age.)
      if (max_age == std::numeric_limits<int64_t>::min())
        return Time::Min();
      if (max_age == std::numeric_limits<int64_t>::max())
        return Time::Max();
    }
  }

  // Try the Expires attribute.
  if (pc.HasExpires() && !pc.Expires().empty()) {
    // Adjust for clock skew between server and host.
    base::Time parsed_expiry =
        cookie_util::ParseCookieExpirationTime(pc.Expires());
    if (!parsed_expiry.is_null())
      return parsed_expiry + (current - server_time);
  }

  // Invalid or no expiration, session cookie.
  return Time();
}

// static
std::unique_ptr<CanonicalCookie> CanonicalCookie::Create(
    const GURL& url,
    const std::string& cookie_line,
    const base::Time& creation_time,
    absl::optional<base::Time> server_time,
    absl::optional<CookiePartitionKey> cookie_partition_key,
    CookieInclusionStatus* status) {
  // Put a pointer on the stack so the rest of the function can assign to it if
  // the default nullptr is passed in.
  CookieInclusionStatus blank_status;
  if (status == nullptr) {
    status = &blank_status;
  }
  *status = CookieInclusionStatus();

  ParsedCookie parsed_cookie(cookie_line, status);

  if (!parsed_cookie.IsValid()) {
    DVLOG(net::cookie_util::kVlogSetCookies)
        << "WARNING: Couldn't parse cookie";
    DCHECK(!status->IsInclude());
    // Don't continue, because an invalid ParsedCookie doesn't have any
    // attributes.
    // TODO(chlily): Log metrics.
    return nullptr;
  }

  std::string cookie_domain;
  if (!GetCookieDomain(url, parsed_cookie, &cookie_domain)) {
    DVLOG(net::cookie_util::kVlogSetCookies)
        << "Create() failed to get a valid cookie domain";
    status->AddExclusionReason(CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);
  }

  std::string cookie_path = CanonPathWithString(
      url, parsed_cookie.HasPath() ? parsed_cookie.Path() : std::string());

  Time cookie_server_time(creation_time);
  if (server_time.has_value() && !server_time->is_null())
    cookie_server_time = server_time.value();

  DCHECK(!creation_time.is_null());
  Time cookie_expires = CanonicalCookie::CanonExpiration(
      parsed_cookie, creation_time, cookie_server_time);

  CookiePrefix prefix = GetCookiePrefix(parsed_cookie.Name());
  bool is_cookie_prefix_valid = IsCookiePrefixValid(prefix, url, parsed_cookie);
  RecordCookiePrefixMetrics(prefix, is_cookie_prefix_valid);
  if (!is_cookie_prefix_valid) {
    DVLOG(net::cookie_util::kVlogSetCookies)
        << "Create() failed because the cookie violated prefix rules.";
    status->AddExclusionReason(CookieInclusionStatus::EXCLUDE_INVALID_PREFIX);
  }

  bool is_same_party_valid = IsCookieSamePartyValid(parsed_cookie);
  if (!is_same_party_valid) {
    status->AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_INVALID_SAMEPARTY);
  }

  // Collect metrics on whether usage of SameParty attribute is correct.
  if (parsed_cookie.IsSameParty())
    base::UmaHistogramBoolean("Cookie.IsSamePartyValid", is_same_party_valid);

  bool partition_has_nonce = CookiePartitionKey::HasNonce(cookie_partition_key);
  bool is_partitioned_valid =
      IsCookiePartitionedValid(parsed_cookie, partition_has_nonce);
  if (!is_partitioned_valid) {
    status->AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_INVALID_PARTITIONED);
  }

  // Collect metrics on whether usage of the Partitioned attribute is correct.
  // Do not include implicit nonce-based partitioned cookies in these metrics.
  if (parsed_cookie.IsPartitioned()) {
    base::UmaHistogramBoolean("Cookie.IsPartitionedValid",
                              is_partitioned_valid);
  } else if (!partition_has_nonce) {
    cookie_partition_key = absl::nullopt;
  }

  if (!status->IsInclude())
    return nullptr;

  CookieSameSiteString samesite_string = CookieSameSiteString::kUnspecified;
  CookieSameSite samesite = parsed_cookie.SameSite(&samesite_string);

  CookieSourceScheme source_scheme = url.SchemeIsCryptographic()
                                         ? CookieSourceScheme::kSecure
                                         : CookieSourceScheme::kNonSecure;
  // Get the port, this will get a default value if a port isn't provided.
  int source_port = url.EffectiveIntPort();

  std::unique_ptr<CanonicalCookie> cc = base::WrapUnique(new CanonicalCookie(
      parsed_cookie.Name(), parsed_cookie.Value(), cookie_domain, cookie_path,
      creation_time, cookie_expires, creation_time, parsed_cookie.IsSecure(),
      parsed_cookie.IsHttpOnly(), samesite, parsed_cookie.Priority(),
      parsed_cookie.IsSameParty(), cookie_partition_key, source_scheme,
      source_port));

  // TODO(chlily): Log metrics.
  if (!cc->IsCanonical()) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
    return nullptr;
  }

  RecordCookieSameSiteAttributeValueHistogram(samesite_string,
                                              parsed_cookie.IsSameParty());

  UMA_HISTOGRAM_BOOLEAN("Cookie.ControlCharacterTruncation",
                        parsed_cookie.HasTruncatedNameOrValue());

  UMA_HISTOGRAM_ENUMERATION(
      "Cookie.TruncatingCharacterInCookieString",
      parsed_cookie.GetTruncatingCharacterInCookieStringType());

  return cc;
}

// static
std::unique_ptr<CanonicalCookie> CanonicalCookie::CreateSanitizedCookie(
    const GURL& url,
    const std::string& name,
    const std::string& value,
    const std::string& domain,
    const std::string& path,
    base::Time creation_time,
    base::Time expiration_time,
    base::Time last_access_time,
    bool secure,
    bool http_only,
    CookieSameSite same_site,
    CookiePriority priority,
    bool same_party,
    absl::optional<CookiePartitionKey> partition_key,
    CookieInclusionStatus* status) {
  // Put a pointer on the stack so the rest of the function can assign to it if
  // the default nullptr is passed in.
  CookieInclusionStatus blank_status;
  if (status == nullptr) {
    status = &blank_status;
  }
  *status = CookieInclusionStatus();

  // Validate consistency of passed arguments.
  if (ParsedCookie::ParseTokenString(name) != name) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
  } else if (ParsedCookie::ParseValueString(value) != value) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
  } else if (ParsedCookie::ParseValueString(path) != path) {
    // NOTE: If `path` contains  "terminating characters" ('\r', '\n', and
    // '\0'), ';', or leading / trailing whitespace, path will be rejected,
    // but any other control characters will just get URL-encoded below.
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
  }

  if (base::FeatureList::IsEnabled(features::kExtraCookieValidityChecks)) {
    // Validate name and value against character set and size limit constraints.
    // If IsValidCookieNameValuePair identifies that `name` and/or `value` are
    // invalid, it will add an ExclusionReason to `status`.
    ParsedCookie::IsValidCookieNameValuePair(name, value, status);

  } else if (!ParsedCookie::IsValidCookieAttributeValueLegacy(name) ||
             !ParsedCookie::IsValidCookieAttributeValueLegacy(value) ||
             (name.empty() && value.empty())) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
  }

  // Validate domain against character set and size limit constraints.
  bool domain_is_valid = true;

  if ((ParsedCookie::ParseValueString(domain) != domain)) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);
    domain_is_valid = false;
  }

  if (base::FeatureList::IsEnabled(features::kExtraCookieValidityChecks)) {
    if (!ParsedCookie::CookieAttributeValueHasValidCharSet(domain)) {
      status->AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);
      domain_is_valid = false;
    }
    if (!ParsedCookie::CookieAttributeValueHasValidSize(domain)) {
      status->AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE);
      domain_is_valid = false;
    }
  }
  const std::string& domain_attribute =
      domain_is_valid ? domain : std::string();

  std::string cookie_domain;
  // This validation step must happen before GetCookieDomainWithString, so it
  // doesn't fail DCHECKs.
  if (!cookie_util::DomainIsHostOnly(url.host())) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);
  } else if (!cookie_util::GetCookieDomainWithString(url, domain_attribute,
                                                     &cookie_domain)) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);
  }

  CookieSourceScheme source_scheme = CookieSourceScheme::kNonSecure;
  // This validation step must happen before SchemeIsCryptographic, so it
  // doesn't fail DCHECKs.
  if (!url.is_valid()) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_DOMAIN);
  } else {
    source_scheme = url.SchemeIsCryptographic()
                        ? CookieSourceScheme::kSecure
                        : CookieSourceScheme::kNonSecure;
  }

  // Get the port, this will get a default value if a port isn't provided.
  int source_port = url.EffectiveIntPort();

  std::string cookie_path = CanonicalCookie::CanonPathWithString(url, path);
  // Canonicalize path again to make sure it escapes characters as needed.
  url::Component path_component(0, cookie_path.length());
  url::RawCanonOutputT<char> canon_path;
  url::Component canon_path_component;
  url::CanonicalizePath(cookie_path.data(), path_component, &canon_path,
                        &canon_path_component);
  std::string encoded_cookie_path = std::string(
      canon_path.data() + canon_path_component.begin, canon_path_component.len);

  if (!path.empty()) {
    if (cookie_path != path) {
      // The path attribute was specified and found to be invalid, so record an
      // error.
      status->AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
    } else if (base::FeatureList::IsEnabled(
                   features::kExtraCookieValidityChecks) &&
               !ParsedCookie::CookieAttributeValueHasValidSize(
                   encoded_cookie_path)) {
      // The path attribute was specified and encodes into a value that's longer
      // than the length limit, so record an error.
      status->AddExclusionReason(
          net::CookieInclusionStatus::EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE);
    }
  }

  CookiePrefix prefix = GetCookiePrefix(name);
  if (!IsCookiePrefixValid(prefix, url, secure, domain_attribute,
                           cookie_path)) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_PREFIX);
  }

  if (!IsCookieSamePartyValid(same_party, secure, same_site)) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_SAMEPARTY);
  }
  if (!IsCookiePartitionedValid(partition_key.has_value(), prefix, same_party,
                                CookiePartitionKey::HasNonce(partition_key))) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_INVALID_PARTITIONED);
  }

  if (!last_access_time.is_null() && creation_time.is_null()) {
    status->AddExclusionReason(
        net::CookieInclusionStatus::EXCLUDE_FAILURE_TO_STORE);
  }

  if (!status->IsInclude())
    return nullptr;

  std::unique_ptr<CanonicalCookie> cc = base::WrapUnique(new CanonicalCookie(
      name, value, cookie_domain, encoded_cookie_path, creation_time,
      expiration_time, last_access_time, secure, http_only, same_site, priority,
      same_party, partition_key, source_scheme, source_port));
  DCHECK(cc->IsCanonical());

  return cc;
}

// static
std::unique_ptr<CanonicalCookie> CanonicalCookie::FromStorage(
    std::string name,
    std::string value,
    std::string domain,
    std::string path,
    base::Time creation,
    base::Time expiration,
    base::Time last_access,
    bool secure,
    bool httponly,
    CookieSameSite same_site,
    CookiePriority priority,
    bool same_party,
    absl::optional<CookiePartitionKey> partition_key,
    CookieSourceScheme source_scheme,
    int source_port) {
  std::unique_ptr<CanonicalCookie> cc = base::WrapUnique(new CanonicalCookie(
      std::move(name), std::move(value), std::move(domain), std::move(path),
      creation, expiration, last_access, secure, httponly, same_site, priority,
      same_party, partition_key, source_scheme, source_port));

  if (cc->IsCanonicalForFromStorage()) {
    // This will help capture the number of times a cookie is canonical but does
    // not have a valid name+value size length
    bool valid_cookie_name_value_pair =
        ParsedCookie::IsValidCookieNameValuePair(cc->Name(), cc->Value());
    UMA_HISTOGRAM_BOOLEAN("Cookie.FromStorageWithValidLength",
                          valid_cookie_name_value_pair);
  } else {
    return nullptr;
  }
  return cc;
}

// static
std::unique_ptr<CanonicalCookie> CanonicalCookie::CreateUnsafeCookieForTesting(
    const std::string& name,
    const std::string& value,
    const std::string& domain,
    const std::string& path,
    const base::Time& creation,
    const base::Time& expiration,
    const base::Time& last_access,
    bool secure,
    bool httponly,
    CookieSameSite same_site,
    CookiePriority priority,
    bool same_party,
    absl::optional<CookiePartitionKey> partition_key,
    CookieSourceScheme source_scheme,
    int source_port) {
  return base::WrapUnique(new CanonicalCookie(
      name, value, domain, path, creation, expiration, last_access, secure,
      httponly, same_site, priority, same_party, partition_key, source_scheme,
      source_port));
}

std::string CanonicalCookie::DomainWithoutDot() const {
  return cookie_util::CookieDomainAsHost(domain_);
}

void CanonicalCookie::SetSourcePort(int port) {
  if ((port >= 0 && port <= 65535) || port == url::PORT_UNSPECIFIED) {
    // 0 would be really weird as it has a special meaning, but it's still
    // technically a valid tcp/ip port so we're going to accept it here.
    source_port_ = port;
  } else {
    source_port_ = url::PORT_INVALID;
  }
}

bool CanonicalCookie::IsEquivalentForSecureCookieMatching(
    const CanonicalCookie& secure_cookie) const {
  // Partition keys must both be equivalent.
  bool same_partition_key = PartitionKey() == secure_cookie.PartitionKey();

  // Names must be the same
  bool same_name = name_ == secure_cookie.Name();

  // They should domain-match in one direction or the other. (See RFC 6265bis
  // section 5.1.3.)
  // TODO(chlily): This does not check for the IP address case. This is bad due
  // to https://crbug.com/1069935.
  bool domain_match =
      IsSubdomainOf(DomainWithoutDot(), secure_cookie.DomainWithoutDot()) ||
      IsSubdomainOf(secure_cookie.DomainWithoutDot(), DomainWithoutDot());

  bool path_match = secure_cookie.IsOnPath(Path());

  bool equivalent_for_secure_cookie_matching =
      same_partition_key && same_name && domain_match && path_match;

  // IsEquivalent() is a stricter check than this.
  DCHECK(!IsEquivalent(secure_cookie) || equivalent_for_secure_cookie_matching);

  return equivalent_for_secure_cookie_matching;
}

bool CanonicalCookie::IsOnPath(const std::string& url_path) const {
  // A zero length would be unsafe for our trailing '/' checks, and
  // would also make no sense for our prefix match.  The code that
  // creates a CanonicalCookie should make sure the path is never zero length,
  // but we double check anyway.
  if (path_.empty())
    return false;

  // The Mozilla code broke this into three cases, based on if the cookie path
  // was longer, the same length, or shorter than the length of the url path.
  // I think the approach below is simpler.

  // Make sure the cookie path is a prefix of the url path.  If the url path is
  // shorter than the cookie path, then the cookie path can't be a prefix.
  if (!base::StartsWith(url_path, path_, base::CompareCase::SENSITIVE))
    return false;

  // |url_path| is >= |path_|, and |path_| is a prefix of |url_path|.  If they
  // are the are the same length then they are identical, otherwise need an
  // additional check:

  // In order to avoid in correctly matching a cookie path of /blah
  // with a request path of '/blahblah/', we need to make sure that either
  // the cookie path ends in a trailing '/', or that we prefix up to a '/'
  // in the url path.  Since we know that the url path length is greater
  // than the cookie path length, it's safe to index one byte past.
  if (path_.length() != url_path.length() && path_.back() != '/' &&
      url_path[path_.length()] != '/') {
    return false;
  }

  return true;
}

bool CanonicalCookie::IsDomainMatch(const std::string& host) const {
  return cookie_util::IsDomainMatch(domain_, host);
}

CookieAccessResult CanonicalCookie::IncludeForRequestURL(
    const GURL& url,
    const CookieOptions& options,
    const CookieAccessParams& params) const {
  CookieInclusionStatus status;
  // Filter out HttpOnly cookies, per options.
  if (options.exclude_httponly() && IsHttpOnly())
    status.AddExclusionReason(CookieInclusionStatus::EXCLUDE_HTTP_ONLY);
  // Secure cookies should not be included in requests for URLs with an
  // insecure scheme, unless it is a localhost url, or the CookieAccessDelegate
  // otherwise denotes them as trustworthy
  // (`delegate_treats_url_as_trustworthy`).
  bool is_allowed_to_access_secure_cookies = false;
  CookieAccessScheme cookie_access_scheme =
      cookie_util::ProvisionalAccessScheme(url);
  if (cookie_access_scheme == CookieAccessScheme::kNonCryptographic &&
      params.delegate_treats_url_as_trustworthy) {
    cookie_access_scheme = CookieAccessScheme::kTrustworthy;
  }
  switch (cookie_access_scheme) {
    case CookieAccessScheme::kNonCryptographic:
      if (IsSecure())
        status.AddExclusionReason(CookieInclusionStatus::EXCLUDE_SECURE_ONLY);
      break;
    case CookieAccessScheme::kTrustworthy:
      is_allowed_to_access_secure_cookies = true;
      if (IsSecure()) {
        status.AddWarningReason(
            CookieInclusionStatus::
                WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC);
      }
      break;
    case CookieAccessScheme::kCryptographic:
      is_allowed_to_access_secure_cookies = true;
      break;
  }
  // Don't include cookies for requests that don't apply to the cookie domain.
  if (!IsDomainMatch(url.host()))
    status.AddExclusionReason(CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH);
  // Don't include cookies for requests with a url path that does not path
  // match the cookie-path.
  if (!IsOnPath(url.path()))
    status.AddExclusionReason(CookieInclusionStatus::EXCLUDE_NOT_ON_PATH);

  // For LEGACY cookies we should always return the schemeless context,
  // otherwise let GetContextForCookieInclusion() decide.
  CookieOptions::SameSiteCookieContext::ContextType cookie_inclusion_context =
      params.access_semantics == CookieAccessSemantics::LEGACY
          ? options.same_site_cookie_context().context()
          : options.same_site_cookie_context().GetContextForCookieInclusion();

  // Don't include same-site cookies for cross-site requests.
  CookieEffectiveSameSite effective_same_site =
      GetEffectiveSameSite(params.access_semantics);
  DCHECK(effective_same_site != CookieEffectiveSameSite::UNDEFINED);
  UMA_HISTOGRAM_ENUMERATION(
      "Cookie.RequestSameSiteContext", cookie_inclusion_context,
      CookieOptions::SameSiteCookieContext::ContextType::COUNT);

  switch (effective_same_site) {
    case CookieEffectiveSameSite::STRICT_MODE:
      if (cookie_inclusion_context <
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_STRICT) {
        status.AddExclusionReason(
            CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT);
      }
      break;
    case CookieEffectiveSameSite::LAX_MODE:
      if (cookie_inclusion_context <
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX) {
        status.AddExclusionReason(
            (SameSite() == CookieSameSite::UNSPECIFIED)
                ? CookieInclusionStatus::
                      EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX
                : CookieInclusionStatus::EXCLUDE_SAMESITE_LAX);
      }
      break;
    // TODO(crbug.com/990439): Add a browsertest for this behavior.
    case CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE:
      DCHECK(SameSite() == CookieSameSite::UNSPECIFIED);
      if (cookie_inclusion_context <
          CookieOptions::SameSiteCookieContext::ContextType::
              SAME_SITE_LAX_METHOD_UNSAFE) {
        // TODO(chlily): Do we need a separate CookieInclusionStatus for this?
        status.AddExclusionReason(
            CookieInclusionStatus::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX);
      }
      break;
    default:
      break;
  }

  // Unless legacy access semantics are in effect, SameSite=None cookies without
  // the Secure attribute should be ignored. This can apply to cookies which
  // were created before "SameSite=None requires Secure" was enabled (as
  // SameSite=None insecure cookies cannot be set while the options are on).
  if (params.access_semantics != CookieAccessSemantics::LEGACY &&
      SameSite() == CookieSameSite::NO_RESTRICTION && !IsSecure()) {
    status.AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE);
  }

  switch (params.same_party_status) {
    case CookieSamePartyStatus::kEnforceSamePartyExclude:
      DCHECK(IsSameParty());
      status.AddExclusionReason(
          CookieInclusionStatus::EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT);
      [[fallthrough]];
    case CookieSamePartyStatus::kEnforceSamePartyInclude: {
      status.AddWarningReason(CookieInclusionStatus::WARN_TREATED_AS_SAMEPARTY);
      // Remove any SameSite exclusion reasons, since SameParty overrides
      // SameSite.
      DCHECK(!status.HasExclusionReason(
          CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT));
      DCHECK_NE(effective_same_site, CookieEffectiveSameSite::STRICT_MODE);
      bool included_by_samesite =
          !status.HasExclusionReason(
              CookieInclusionStatus::EXCLUDE_SAMESITE_LAX) &&
          !status.HasExclusionReason(
              CookieInclusionStatus::
                  EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX);
      if (!included_by_samesite) {
        status.RemoveExclusionReasons({
            CookieInclusionStatus::EXCLUDE_SAMESITE_LAX,
            CookieInclusionStatus::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
        });
      }

      // Update metrics.
      if (status.HasOnlyExclusionReason(
              CookieInclusionStatus::EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT) &&
          included_by_samesite) {
        status.AddWarningReason(
            CookieInclusionStatus::WARN_SAMEPARTY_EXCLUSION_OVERRULED_SAMESITE);
      }
      if (status.IsInclude()) {
        UMA_HISTOGRAM_BOOLEAN(
            "Cookie.SamePartyReadIncluded.InclusionUnderSameSite",
            included_by_samesite);
        if (!included_by_samesite) {
          status.AddWarningReason(
              CookieInclusionStatus::
                  WARN_SAMEPARTY_INCLUSION_OVERRULED_SAMESITE);
        }
      }
      break;
    }
    case CookieSamePartyStatus::kNoSamePartyEnforcement:
      // Only apply SameSite-related warnings if SameParty is not in effect.
      ApplySameSiteCookieWarningToStatus(
          SameSite(), effective_same_site, IsSecure(),
          options.same_site_cookie_context(), &status,
          false /* is_cookie_being_set */);
      break;
  }

  if (status.IsInclude()) {
    UMA_HISTOGRAM_ENUMERATION("Cookie.IncludedRequestEffectiveSameSite",
                              effective_same_site,
                              CookieEffectiveSameSite::COUNT);

    if (SameSite() == CookieSameSite::NO_RESTRICTION) {
      SamePartyContext::Type top_resource =
          options.same_party_context().top_resource_for_metrics_only();
      SamePartyContext::Type ancestors =
          options.same_party_context().ancestors_for_metrics_only();

      if (top_resource == SamePartyContext::Type::kCrossParty) {
        status.AddWarningReason(
            CookieInclusionStatus::WARN_SAMESITE_NONE_REQUIRED);
        RecordSameSiteNoneReadContextMetric(
            SameSiteNonePartyContextType::kSameSiteNoneRequired);
      } else if (ancestors == SamePartyContext::Type::kCrossParty) {
        status.AddWarningReason(
            CookieInclusionStatus::
                WARN_SAMESITE_NONE_INCLUDED_BY_SAMEPARTY_TOP_RESOURCE);
        RecordSameSiteNoneReadContextMetric(
            SameSiteNonePartyContextType::kSamePartyTopResource);
      } else if (cookie_inclusion_context <
                 CookieOptions::SameSiteCookieContext::ContextType::
                     SAME_SITE_LAX) {
        status.AddWarningReason(
            CookieInclusionStatus::
                WARN_SAMESITE_NONE_INCLUDED_BY_SAMEPARTY_ANCESTORS);
        RecordSameSiteNoneReadContextMetric(
            SameSiteNonePartyContextType::kSamePartyAncestors);
      } else if (cookie_inclusion_context <
                 CookieOptions::SameSiteCookieContext::ContextType::
                     SAME_SITE_STRICT) {
        status.AddWarningReason(
            CookieInclusionStatus::WARN_SAMESITE_NONE_INCLUDED_BY_SAMESITE_LAX);
        RecordSameSiteNoneReadContextMetric(
            SameSiteNonePartyContextType::kSameSiteLax);
      } else {
        status.AddWarningReason(
            CookieInclusionStatus::
                WARN_SAMESITE_NONE_INCLUDED_BY_SAMESITE_STRICT);
        RecordSameSiteNoneReadContextMetric(
            SameSiteNonePartyContextType::kSameSiteStrict);
      }
    }
  }

  if (status.HasWarningReason(
          CookieInclusionStatus::
              WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Cookie.CrossSiteRedirectDowngradeChangesInclusion.Read", SameSite());
  }

  return CookieAccessResult(effective_same_site, status,
                            params.access_semantics,
                            is_allowed_to_access_secure_cookies);
}

CookieAccessResult CanonicalCookie::IsSetPermittedInContext(
    const GURL& source_url,
    const CookieOptions& options,
    const CookieAccessParams& params,
    const std::vector<std::string>& cookieable_schemes) const {
  CookieAccessResult access_result;
  if (!base::Contains(cookieable_schemes, source_url.scheme())) {
    access_result.status.AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_NONCOOKIEABLE_SCHEME);
  }

  CookieAccessScheme access_scheme =
      cookie_util::ProvisionalAccessScheme(source_url);
  if (access_scheme == CookieAccessScheme::kNonCryptographic &&
      params.delegate_treats_url_as_trustworthy) {
    access_scheme = CookieAccessScheme::kTrustworthy;
  }

  switch (access_scheme) {
    case CookieAccessScheme::kNonCryptographic:
      access_result.is_allowed_to_access_secure_cookies = false;
      if (IsSecure()) {
        access_result.status.AddExclusionReason(
            CookieInclusionStatus::EXCLUDE_SECURE_ONLY);
      }
      break;

    case CookieAccessScheme::kCryptographic:
      // All cool!
      access_result.is_allowed_to_access_secure_cookies = true;
      break;

    case CookieAccessScheme::kTrustworthy:
      access_result.is_allowed_to_access_secure_cookies = true;
      if (IsSecure()) {
        // OK, but want people aware of this.
        access_result.status.AddWarningReason(
            CookieInclusionStatus::
                WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC);
      }
      break;
  }

  access_result.access_semantics = params.access_semantics;
  if (options.exclude_httponly() && IsHttpOnly()) {
    DVLOG(net::cookie_util::kVlogSetCookies)
        << "HttpOnly cookie not permitted in script context.";
    access_result.status.AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_HTTP_ONLY);
  }

  // Unless legacy access semantics are in effect, SameSite=None cookies without
  // the Secure attribute will be rejected.
  if (params.access_semantics != CookieAccessSemantics::LEGACY &&
      SameSite() == CookieSameSite::NO_RESTRICTION && !IsSecure()) {
    DVLOG(net::cookie_util::kVlogSetCookies)
        << "SetCookie() rejecting insecure cookie with SameSite=None.";
    access_result.status.AddExclusionReason(
        CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE);
  }

  // For LEGACY cookies we should always return the schemeless context,
  // otherwise let GetContextForCookieInclusion() decide.
  CookieOptions::SameSiteCookieContext::ContextType cookie_inclusion_context =
      params.access_semantics == CookieAccessSemantics::LEGACY
          ? options.same_site_cookie_context().context()
          : options.same_site_cookie_context().GetContextForCookieInclusion();

  access_result.effective_same_site =
      GetEffectiveSameSite(params.access_semantics);
  DCHECK(access_result.effective_same_site !=
         CookieEffectiveSameSite::UNDEFINED);
  switch (access_result.effective_same_site) {
    case CookieEffectiveSameSite::STRICT_MODE:
      // This intentionally checks for `< SAME_SITE_LAX`, as we allow
      // `SameSite=Strict` cookies to be set for top-level navigations that
      // qualify for receipt of `SameSite=Lax` cookies.
      if (cookie_inclusion_context <
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX) {
        DVLOG(net::cookie_util::kVlogSetCookies)
            << "Trying to set a `SameSite=Strict` cookie from a "
               "cross-site URL.";
        access_result.status.AddExclusionReason(
            CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT);
      }
      break;
    case CookieEffectiveSameSite::LAX_MODE:
    case CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE:
      if (cookie_inclusion_context <
          CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX) {
        if (SameSite() == CookieSameSite::UNSPECIFIED) {
          DVLOG(net::cookie_util::kVlogSetCookies)
              << "Cookies with no known SameSite attribute being treated as "
                 "lax; attempt to set from a cross-site URL denied.";
          access_result.status.AddExclusionReason(
              CookieInclusionStatus::
                  EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX);
        } else {
          DVLOG(net::cookie_util::kVlogSetCookies)
              << "Trying to set a `SameSite=Lax` cookie from a cross-site URL.";
          access_result.status.AddExclusionReason(
              CookieInclusionStatus::EXCLUDE_SAMESITE_LAX);
        }
      }
      break;
    default:
      break;
  }

  switch (params.same_party_status) {
    case CookieSamePartyStatus::kEnforceSamePartyExclude:
      DCHECK(IsSameParty());
      access_result.status.AddExclusionReason(
          CookieInclusionStatus::EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT);
      [[fallthrough]];
    case CookieSamePartyStatus::kEnforceSamePartyInclude: {
      DCHECK(IsSameParty());
      access_result.status.AddWarningReason(
          CookieInclusionStatus::WARN_TREATED_AS_SAMEPARTY);
      // Remove any SameSite exclusion reasons, since SameParty overrides
      // SameSite.
      DCHECK(!access_result.status.HasExclusionReason(
          CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT));
      DCHECK_NE(access_result.effective_same_site,
                CookieEffectiveSameSite::STRICT_MODE);
      bool included_by_samesite =
          !access_result.status.HasExclusionReason(
              CookieInclusionStatus::EXCLUDE_SAMESITE_LAX) &&
          !access_result.status.HasExclusionReason(
              CookieInclusionStatus::
                  EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX);
      if (!included_by_samesite) {
        access_result.status.RemoveExclusionReasons({
            CookieInclusionStatus::EXCLUDE_SAMESITE_LAX,
            CookieInclusionStatus::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
        });
      }

      // Update metrics.
      if (access_result.status.HasOnlyExclusionReason(
              CookieInclusionStatus::EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT) &&
          included_by_samesite) {
        access_result.status.AddWarningReason(
            CookieInclusionStatus::WARN_SAMEPARTY_EXCLUSION_OVERRULED_SAMESITE);
      }
      if (access_result.status.IsInclude()) {
        UMA_HISTOGRAM_BOOLEAN(
            "Cookie.SamePartySetIncluded.InclusionUnderSameSite",
            included_by_samesite);
        if (!included_by_samesite) {
          access_result.status.AddWarningReason(
              CookieInclusionStatus::
                  WARN_SAMEPARTY_INCLUSION_OVERRULED_SAMESITE);
        }
      }
      break;
    }
    case CookieSamePartyStatus::kNoSamePartyEnforcement:
      // Only apply SameSite-related warnings if SameParty is not in effect.
      ApplySameSiteCookieWarningToStatus(
          SameSite(), access_result.effective_same_site, IsSecure(),
          options.same_site_cookie_context(), &access_result.status,
          true /* is_cookie_being_set */);
      break;
  }

  if (access_result.status.IsInclude()) {
    UMA_HISTOGRAM_ENUMERATION("Cookie.IncludedResponseEffectiveSameSite",
                              access_result.effective_same_site,
                              CookieEffectiveSameSite::COUNT);

    if (SameSite() == CookieSameSite::NO_RESTRICTION) {
      SamePartyContext::Type top_resource =
          options.same_party_context().top_resource_for_metrics_only();
      SamePartyContext::Type ancestors =
          options.same_party_context().ancestors_for_metrics_only();

      if (top_resource == SamePartyContext::Type::kCrossParty) {
        access_result.status.AddWarningReason(
            CookieInclusionStatus::WARN_SAMESITE_NONE_REQUIRED);
        RecordSameSiteNoneWriteContextMetric(
            SameSiteNonePartyContextType::kSameSiteNoneRequired);
      } else if (ancestors == SamePartyContext::Type::kCrossParty) {
        access_result.status.AddWarningReason(
            CookieInclusionStatus::
                WARN_SAMESITE_NONE_INCLUDED_BY_SAMEPARTY_TOP_RESOURCE);
        RecordSameSiteNoneWriteContextMetric(
            SameSiteNonePartyContextType::kSamePartyTopResource);
      } else if (cookie_inclusion_context <
                 CookieOptions::SameSiteCookieContext::ContextType::
                     SAME_SITE_LAX) {
        access_result.status.AddWarningReason(
            CookieInclusionStatus::
                WARN_SAMESITE_NONE_INCLUDED_BY_SAMEPARTY_ANCESTORS);
        RecordSameSiteNoneWriteContextMetric(
            SameSiteNonePartyContextType::kSamePartyAncestors);
      } else {
        // NB: unlike when sending a cookie, there's no distinction between
        // SameSite=Lax and SameSite=Strict when setting a cookie.
        access_result.status.AddWarningReason(
            CookieInclusionStatus::
                WARN_SAMESITE_NONE_INCLUDED_BY_SAMESITE_STRICT);
        RecordSameSiteNoneWriteContextMetric(
            SameSiteNonePartyContextType::kSameSiteStrict);
      }
    }
  }

  if (access_result.status.HasWarningReason(
          CookieInclusionStatus::
              WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Cookie.CrossSiteRedirectDowngradeChangesInclusion.Write", SameSite());
  }

  return access_result;
}

std::string CanonicalCookie::DebugString() const {
  return base::StringPrintf(
      "name: %s value: %s domain: %s path: %s creation: %" PRId64,
      name_.c_str(), value_.c_str(), domain_.c_str(), path_.c_str(),
      static_cast<int64_t>(creation_date_.ToTimeT()));
}

bool CanonicalCookie::PartialCompare(const CanonicalCookie& other) const {
  return PartialCookieOrdering(*this, other) < 0;
}

bool CanonicalCookie::IsCanonical() const {
  // TODO(crbug.com/1244172) Eventually we should check the size of name+value,
  // assuming we collect metrics and determine that a low percentage of cookies
  // would fail this check. Note that we still don't want to enforce length
  // checks on domain or path for the reason stated above.

  return IsCanonicalForFromStorage();
}

bool CanonicalCookie::IsCanonicalForFromStorage() const {
  // Not checking domain or path against ParsedCookie as it may have
  // come purely from the URL. Also, don't call IsValidCookieNameValuePair()
  // here because we don't want to enforce the size checks on names or values
  // that may have been reconstituted from the cookie store.
  if (ParsedCookie::ParseTokenString(name_) != name_ ||
      !ParsedCookie::ValueMatchesParsedValue(value_)) {
    return false;
  }

  if (base::FeatureList::IsEnabled(features::kExtraCookieValidityChecks)) {
    if (!ParsedCookie::IsValidCookieName(name_) ||
        !ParsedCookie::IsValidCookieValue(value_)) {
      return false;
    }
  } else {
    if (!ParsedCookie::IsValidCookieAttributeValueLegacy(name_) ||
        !ParsedCookie::IsValidCookieAttributeValueLegacy(value_)) {
      return false;
    }
  }

  if (!last_access_date_.is_null() && creation_date_.is_null())
    return false;

  url::CanonHostInfo canon_host_info;
  std::string canonical_domain(CanonicalizeHost(domain_, &canon_host_info));

  // TODO(rdsmith): This specifically allows for empty domains.  The spec
  // suggests this is invalid (if a domain attribute is empty, the cookie's
  // domain is set to the canonicalized request host; see
  // https://tools.ietf.org/html/rfc6265#section-5.3).  However, it is
  // needed for Chrome extension cookies.
  // See http://crbug.com/730633 for more information.
  if (canonical_domain != domain_)
    return false;

  if (path_.empty() || path_[0] != '/')
    return false;

  CookiePrefix prefix = GetCookiePrefix(name_);
  switch (prefix) {
    case COOKIE_PREFIX_HOST:
      if (!secure_ || path_ != "/" || domain_.empty() || domain_[0] == '.')
        return false;
      break;
    case COOKIE_PREFIX_SECURE:
      if (!secure_)
        return false;
      break;
    default:
      break;
  }

  if (!IsCookieSamePartyValid(same_party_, secure_, same_site_))
    return false;

  return IsCookiePartitionedValid(IsPartitioned(), prefix, same_party_,
                                  CookiePartitionKey::HasNonce(partition_key_));
}

bool CanonicalCookie::IsEffectivelySameSiteNone(
    CookieAccessSemantics access_semantics) const {
  return GetEffectiveSameSite(access_semantics) ==
         CookieEffectiveSameSite::NO_RESTRICTION;
}

CookieEffectiveSameSite CanonicalCookie::GetEffectiveSameSiteForTesting(
    CookieAccessSemantics access_semantics) const {
  return GetEffectiveSameSite(access_semantics);
}

// static
std::string CanonicalCookie::BuildCookieLine(const CookieList& cookies) {
  std::string cookie_line;
  for (const auto& cookie : cookies) {
    AppendCookieLineEntry(cookie, &cookie_line);
  }
  return cookie_line;
}

// static
std::string CanonicalCookie::BuildCookieLine(
    const CookieAccessResultList& cookie_access_result_list) {
  std::string cookie_line;
  for (const auto& cookie_with_access_result : cookie_access_result_list) {
    const CanonicalCookie& cookie = cookie_with_access_result.cookie;
    AppendCookieLineEntry(cookie, &cookie_line);
  }
  return cookie_line;
}

// static
CanonicalCookie::CookiePrefix CanonicalCookie::GetCookiePrefix(
    const std::string& name) {
  const char kSecurePrefix[] = "__Secure-";
  const char kHostPrefix[] = "__Host-";
  if (base::StartsWith(name, kSecurePrefix, base::CompareCase::SENSITIVE))
    return CanonicalCookie::COOKIE_PREFIX_SECURE;
  if (base::StartsWith(name, kHostPrefix, base::CompareCase::SENSITIVE))
    return CanonicalCookie::COOKIE_PREFIX_HOST;
  return CanonicalCookie::COOKIE_PREFIX_NONE;
}

// static
void CanonicalCookie::RecordCookiePrefixMetrics(
    CanonicalCookie::CookiePrefix prefix,
    bool is_cookie_valid) {
  const char kCookiePrefixHistogram[] = "Cookie.CookiePrefix";
  UMA_HISTOGRAM_ENUMERATION(kCookiePrefixHistogram, prefix,
                            CanonicalCookie::COOKIE_PREFIX_LAST);
}

// Returns true if the cookie does not violate any constraints imposed
// by the cookie name's prefix, as described in
// https://tools.ietf.org/html/draft-west-cookie-prefixes
//
// static
bool CanonicalCookie::IsCookiePrefixValid(CanonicalCookie::CookiePrefix prefix,
                                          const GURL& url,
                                          const ParsedCookie& parsed_cookie) {
  return CanonicalCookie::IsCookiePrefixValid(
      prefix, url, parsed_cookie.IsSecure(),
      parsed_cookie.HasDomain() ? parsed_cookie.Domain() : "",
      parsed_cookie.HasPath() ? parsed_cookie.Path() : "");
}

bool CanonicalCookie::IsCookiePrefixValid(CanonicalCookie::CookiePrefix prefix,
                                          const GURL& url,
                                          bool secure,
                                          const std::string& domain,
                                          const std::string& path) {
  if (prefix == CanonicalCookie::COOKIE_PREFIX_SECURE)
    return secure && url.SchemeIsCryptographic();
  if (prefix == CanonicalCookie::COOKIE_PREFIX_HOST) {
    const bool domain_valid =
        domain.empty() || (url.HostIsIPAddress() && url.host() == domain);
    return secure && url.SchemeIsCryptographic() && domain_valid && path == "/";
  }
  return true;
}

CookieEffectiveSameSite CanonicalCookie::GetEffectiveSameSite(
    CookieAccessSemantics access_semantics) const {
  base::TimeDelta lax_allow_unsafe_threshold_age =
      base::FeatureList::IsEnabled(
          features::kSameSiteDefaultChecksMethodRigorously)
          ? base::TimeDelta::Min()
          : (base::FeatureList::IsEnabled(
                 features::kShortLaxAllowUnsafeThreshold)
                 ? kShortLaxAllowUnsafeMaxAge
                 : kLaxAllowUnsafeMaxAge);

  switch (SameSite()) {
    // If a cookie does not have a SameSite attribute, the effective SameSite
    // mode depends on the access semantics and whether the cookie is
    // recently-created.
    case CookieSameSite::UNSPECIFIED:
      return (access_semantics == CookieAccessSemantics::LEGACY)
                 ? CookieEffectiveSameSite::NO_RESTRICTION
                 : (IsRecentlyCreated(lax_allow_unsafe_threshold_age)
                        ? CookieEffectiveSameSite::LAX_MODE_ALLOW_UNSAFE
                        : CookieEffectiveSameSite::LAX_MODE);
    case CookieSameSite::NO_RESTRICTION:
      return CookieEffectiveSameSite::NO_RESTRICTION;
    case CookieSameSite::LAX_MODE:
      return CookieEffectiveSameSite::LAX_MODE;
    case CookieSameSite::STRICT_MODE:
      return CookieEffectiveSameSite::STRICT_MODE;
  }
}

bool CanonicalCookie::IsRecentlyCreated(base::TimeDelta age_threshold) const {
  return (base::Time::Now() - creation_date_) <= age_threshold;
}

// static
bool CanonicalCookie::IsCookieSamePartyValid(
    const ParsedCookie& parsed_cookie) {
  return IsCookieSamePartyValid(parsed_cookie.IsSameParty(),
                                parsed_cookie.IsSecure(),
                                parsed_cookie.SameSite());
}

// static
bool CanonicalCookie::IsCookieSamePartyValid(bool is_same_party,
                                             bool is_secure,
                                             CookieSameSite same_site) {
  if (!is_same_party)
    return true;
  return is_secure && (same_site != CookieSameSite::STRICT_MODE);
}

// static
bool CanonicalCookie::IsCookiePartitionedValid(
    const ParsedCookie& parsed_cookie,
    bool partition_has_nonce) {
  return IsCookiePartitionedValid(
      parsed_cookie.IsPartitioned(), GetCookiePrefix(parsed_cookie.Name()),
      parsed_cookie.IsSameParty(), partition_has_nonce);
}

// static
bool CanonicalCookie::IsCookiePartitionedValid(
    bool is_partitioned,
    CanonicalCookie::CookiePrefix prefix,
    bool is_same_party,
    bool partition_has_nonce) {
  if (!is_partitioned)
    return true;
  if (partition_has_nonce)
    return true;
  return prefix == CookiePrefix::COOKIE_PREFIX_HOST && !is_same_party;
}

CookieAndLineWithAccessResult::CookieAndLineWithAccessResult() = default;

CookieAndLineWithAccessResult::CookieAndLineWithAccessResult(
    absl::optional<CanonicalCookie> cookie,
    std::string cookie_string,
    CookieAccessResult access_result)
    : cookie(std::move(cookie)),
      cookie_string(std::move(cookie_string)),
      access_result(access_result) {}

CookieAndLineWithAccessResult::CookieAndLineWithAccessResult(
    const CookieAndLineWithAccessResult&) = default;

CookieAndLineWithAccessResult& CookieAndLineWithAccessResult::operator=(
    const CookieAndLineWithAccessResult& cookie_and_line_with_access_result) =
    default;

CookieAndLineWithAccessResult::CookieAndLineWithAccessResult(
    CookieAndLineWithAccessResult&&) = default;

CookieAndLineWithAccessResult::~CookieAndLineWithAccessResult() = default;

}  // namespace net
