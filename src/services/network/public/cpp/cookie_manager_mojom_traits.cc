// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_manager_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

network::mojom::CookiePriority
EnumTraits<network::mojom::CookiePriority, net::CookiePriority>::ToMojom(
    net::CookiePriority input) {
  switch (input) {
    case net::COOKIE_PRIORITY_LOW:
      return network::mojom::CookiePriority::LOW;
    case net::COOKIE_PRIORITY_MEDIUM:
      return network::mojom::CookiePriority::MEDIUM;
    case net::COOKIE_PRIORITY_HIGH:
      return network::mojom::CookiePriority::HIGH;
  }
  NOTREACHED();
  return static_cast<network::mojom::CookiePriority>(input);
}

bool EnumTraits<network::mojom::CookiePriority, net::CookiePriority>::FromMojom(
    network::mojom::CookiePriority input,
    net::CookiePriority* output) {
  switch (input) {
    case network::mojom::CookiePriority::LOW:
      *output = net::CookiePriority::COOKIE_PRIORITY_LOW;
      return true;
    case network::mojom::CookiePriority::MEDIUM:
      *output = net::CookiePriority::COOKIE_PRIORITY_MEDIUM;
      return true;
    case network::mojom::CookiePriority::HIGH:
      *output = net::CookiePriority::COOKIE_PRIORITY_HIGH;
      return true;
  }
  return false;
}

network::mojom::CookieSameSite
EnumTraits<network::mojom::CookieSameSite, net::CookieSameSite>::ToMojom(
    net::CookieSameSite input) {
  switch (input) {
    case net::CookieSameSite::UNSPECIFIED:
      return network::mojom::CookieSameSite::UNSPECIFIED;
    case net::CookieSameSite::NO_RESTRICTION:
      return network::mojom::CookieSameSite::NO_RESTRICTION;
    case net::CookieSameSite::LAX_MODE:
      return network::mojom::CookieSameSite::LAX_MODE;
    case net::CookieSameSite::STRICT_MODE:
      return network::mojom::CookieSameSite::STRICT_MODE;
    case net::CookieSameSite::EXTENDED_MODE:
      return network::mojom::CookieSameSite::EXTENDED_MODE;
    default:
      break;
  }
  NOTREACHED();
  return static_cast<network::mojom::CookieSameSite>(input);
}

bool EnumTraits<network::mojom::CookieSameSite, net::CookieSameSite>::FromMojom(
    network::mojom::CookieSameSite input,
    net::CookieSameSite* output) {
  switch (input) {
    case network::mojom::CookieSameSite::UNSPECIFIED:
      *output = net::CookieSameSite::UNSPECIFIED;
      return true;
    case network::mojom::CookieSameSite::NO_RESTRICTION:
      *output = net::CookieSameSite::NO_RESTRICTION;
      return true;
    case network::mojom::CookieSameSite::LAX_MODE:
      *output = net::CookieSameSite::LAX_MODE;
      return true;
    case network::mojom::CookieSameSite::STRICT_MODE:
      *output = net::CookieSameSite::STRICT_MODE;
      return true;
    case network::mojom::CookieSameSite::EXTENDED_MODE:
      *output = net::CookieSameSite::EXTENDED_MODE;
      return true;
    default:
      break;
  }
  return false;
}

network::mojom::CookieInclusionStatusWarningReason
EnumTraits<network::mojom::CookieInclusionStatusWarningReason,
           net::CanonicalCookie::CookieInclusionStatus::WarningReason>::
    ToMojom(net::CanonicalCookie::CookieInclusionStatus::WarningReason input) {
  switch (input) {
    case net::CanonicalCookie::CookieInclusionStatus::WarningReason::
        DO_NOT_WARN:
      return network::mojom::CookieInclusionStatusWarningReason::DO_NOT_WARN;
    case net::CanonicalCookie::CookieInclusionStatus::WarningReason::
        WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT:
      return network::mojom::CookieInclusionStatusWarningReason::
          WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT;
    case net::CanonicalCookie::CookieInclusionStatus::WarningReason::
        WARN_SAMESITE_NONE_INSECURE:
      return network::mojom::CookieInclusionStatusWarningReason::
          WARN_SAMESITE_NONE_INSECURE;
    case net::CanonicalCookie::CookieInclusionStatus::WarningReason::
        WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE:
      return network::mojom::CookieInclusionStatusWarningReason::
          WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE;
  }
  NOTREACHED();
  return network::mojom::CookieInclusionStatusWarningReason::DO_NOT_WARN;
}

bool EnumTraits<network::mojom::CookieInclusionStatusWarningReason,
                net::CanonicalCookie::CookieInclusionStatus::WarningReason>::
    FromMojom(
        network::mojom::CookieInclusionStatusWarningReason input,
        net::CanonicalCookie::CookieInclusionStatus::WarningReason* output) {
  switch (input) {
    case network::mojom::CookieInclusionStatusWarningReason::DO_NOT_WARN:
      *output = net::CanonicalCookie::CookieInclusionStatus::WarningReason::
          DO_NOT_WARN;
      return true;
    case network::mojom::CookieInclusionStatusWarningReason::
        WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT:
      *output = net::CanonicalCookie::CookieInclusionStatus::WarningReason::
          WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT;
      return true;
    case network::mojom::CookieInclusionStatusWarningReason::
        WARN_SAMESITE_NONE_INSECURE:
      *output = net::CanonicalCookie::CookieInclusionStatus::WarningReason::
          WARN_SAMESITE_NONE_INSECURE;
      return true;
    case network::mojom::CookieInclusionStatusWarningReason::
        WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE:
      *output = net::CanonicalCookie::CookieInclusionStatus::WarningReason::
          WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE;
      return true;
  }
  NOTREACHED();
  return false;
}

network::mojom::CookieSameSiteContext
EnumTraits<network::mojom::CookieSameSiteContext,
           net::CookieOptions::SameSiteCookieContext>::
    ToMojom(net::CookieOptions::SameSiteCookieContext input) {
  switch (input) {
    case net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT:
      return network::mojom::CookieSameSiteContext::SAME_SITE_STRICT;
    case net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX:
      return network::mojom::CookieSameSiteContext::SAME_SITE_LAX;
    case net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX_METHOD_UNSAFE:
      return network::mojom::CookieSameSiteContext::SAME_SITE_LAX_METHOD_UNSAFE;
    case net::CookieOptions::SameSiteCookieContext::CROSS_SITE:
      return network::mojom::CookieSameSiteContext::CROSS_SITE;
  }
  NOTREACHED();
  return network::mojom::CookieSameSiteContext::CROSS_SITE;
}

bool EnumTraits<network::mojom::CookieSameSiteContext,
                net::CookieOptions::SameSiteCookieContext>::
    FromMojom(network::mojom::CookieSameSiteContext input,
              net::CookieOptions::SameSiteCookieContext* output) {
  switch (input) {
    case network::mojom::CookieSameSiteContext::SAME_SITE_STRICT:
      *output = net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT;
      return true;
    case network::mojom::CookieSameSiteContext::SAME_SITE_LAX:
      *output = net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX;
      return true;
    case network::mojom::CookieSameSiteContext::SAME_SITE_LAX_METHOD_UNSAFE:
      *output = net::CookieOptions::SameSiteCookieContext::
          SAME_SITE_LAX_METHOD_UNSAFE;
      return true;
    case network::mojom::CookieSameSiteContext::CROSS_SITE:
      *output = net::CookieOptions::SameSiteCookieContext::CROSS_SITE;
      return true;
  }
  return false;
}

bool StructTraits<network::mojom::CookieOptionsDataView, net::CookieOptions>::
    Read(network::mojom::CookieOptionsDataView mojo_options,
         net::CookieOptions* cookie_options) {
  if (mojo_options.exclude_httponly())
    cookie_options->set_exclude_httponly();
  else
    cookie_options->set_include_httponly();

  net::CookieOptions::SameSiteCookieContext same_site_cookie_context;
  if (!mojo_options.ReadSameSiteCookieContext(&same_site_cookie_context))
    return false;
  cookie_options->set_same_site_cookie_context(same_site_cookie_context);

  if (mojo_options.update_access_time())
    cookie_options->set_update_access_time();
  else
    cookie_options->set_do_not_update_access_time();

  if (mojo_options.return_excluded_cookies())
    cookie_options->set_return_excluded_cookies();
  else
    cookie_options->unset_return_excluded_cookies();

  return true;
}

bool StructTraits<
    network::mojom::CanonicalCookieDataView,
    net::CanonicalCookie>::Read(network::mojom::CanonicalCookieDataView cookie,
                                net::CanonicalCookie* out) {
  std::string name;
  if (!cookie.ReadName(&name))
    return false;

  std::string value;
  if (!cookie.ReadValue(&value))
    return false;

  std::string domain;
  if (!cookie.ReadDomain(&domain))
    return false;

  std::string path;
  if (!cookie.ReadPath(&path))
    return false;

  base::Time creation_time;
  base::Time expiry_time;
  base::Time last_access_time;
  if (!cookie.ReadCreation(&creation_time))
    return false;

  if (!cookie.ReadExpiry(&expiry_time))
    return false;

  if (!cookie.ReadLastAccess(&last_access_time))
    return false;

  net::CookieSameSite site_restrictions;
  if (!cookie.ReadSiteRestrictions(&site_restrictions))
    return false;

  net::CookiePriority priority;
  if (!cookie.ReadPriority(&priority))
    return false;

  *out = net::CanonicalCookie(name, value, domain, path, creation_time,
                              expiry_time, last_access_time, cookie.secure(),
                              cookie.httponly(), site_restrictions, priority);
  return out->IsCanonical();
}

bool StructTraits<network::mojom::CookieInclusionStatusDataView,
                  net::CanonicalCookie::CookieInclusionStatus>::
    Read(network::mojom::CookieInclusionStatusDataView status,
         net::CanonicalCookie::CookieInclusionStatus* out) {
  *out = net::CanonicalCookie::CookieInclusionStatus();
  out->set_exclusion_reasons(status.exclusion_reasons());

  net::CanonicalCookie::CookieInclusionStatus::WarningReason warning;
  if (!status.ReadWarning(&warning))
    return false;
  out->set_warning(warning);

  return out->IsValid();
}

bool StructTraits<
    network::mojom::CookieWithStatusDataView,
    net::CookieWithStatus>::Read(network::mojom::CookieWithStatusDataView c,
                                 net::CookieWithStatus* out) {
  net::CanonicalCookie cookie;
  net::CanonicalCookie::CookieInclusionStatus status;
  if (!c.ReadCookie(&cookie))
    return false;
  if (!c.ReadStatus(&status))
    return false;

  *out = {cookie, status};

  return true;
}

bool StructTraits<network::mojom::CookieAndLineWithStatusDataView,
                  net::CookieAndLineWithStatus>::
    Read(network::mojom::CookieAndLineWithStatusDataView c,
         net::CookieAndLineWithStatus* out) {
  base::Optional<net::CanonicalCookie> cookie;
  std::string cookie_string;
  net::CanonicalCookie::CookieInclusionStatus status;
  if (!c.ReadCookie(&cookie))
    return false;
  if (!c.ReadCookieString(&cookie_string))
    return false;
  if (!c.ReadStatus(&status))
    return false;

  *out = {cookie, cookie_string, status};

  return true;
}

}  // namespace mojo
