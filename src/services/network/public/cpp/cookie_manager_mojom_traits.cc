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
    case net::CookieSameSite::NO_RESTRICTION:
      return network::mojom::CookieSameSite::NO_RESTRICTION;
    case net::CookieSameSite::LAX_MODE:
      return network::mojom::CookieSameSite::LAX_MODE;
    case net::CookieSameSite::STRICT_MODE:
      return network::mojom::CookieSameSite::STRICT_MODE;
  }
  NOTREACHED();
  return static_cast<network::mojom::CookieSameSite>(input);
}

bool EnumTraits<network::mojom::CookieSameSite, net::CookieSameSite>::FromMojom(
    network::mojom::CookieSameSite input,
    net::CookieSameSite* output) {
  switch (input) {
    case network::mojom::CookieSameSite::NO_RESTRICTION:
      *output = net::CookieSameSite::NO_RESTRICTION;
      return true;
    case network::mojom::CookieSameSite::LAX_MODE:
      *output = net::CookieSameSite::LAX_MODE;
      return true;
    case network::mojom::CookieSameSite::STRICT_MODE:
      *output = net::CookieSameSite::STRICT_MODE;
      return true;
  }
  return false;
}

network::mojom::CookieSameSiteFilter
EnumTraits<network::mojom::CookieSameSiteFilter,
           net::CookieOptions::SameSiteCookieContext>::
    ToMojom(net::CookieOptions::SameSiteCookieContext input) {
  switch (input) {
    case net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT:
      return network::mojom::CookieSameSiteFilter::INCLUDE_STRICT_AND_LAX;
    case net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX:
      return network::mojom::CookieSameSiteFilter::INCLUDE_LAX;
    case net::CookieOptions::SameSiteCookieContext::CROSS_SITE:
      return network::mojom::CookieSameSiteFilter::DO_NOT_INCLUDE;
  }
  NOTREACHED();
  return static_cast<network::mojom::CookieSameSiteFilter>(input);
}

bool EnumTraits<network::mojom::CookieSameSiteFilter,
                net::CookieOptions::SameSiteCookieContext>::
    FromMojom(network::mojom::CookieSameSiteFilter input,
              net::CookieOptions::SameSiteCookieContext* output) {
  switch (input) {
    case network::mojom::CookieSameSiteFilter::INCLUDE_STRICT_AND_LAX:
      *output = net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT;
      return true;
    case network::mojom::CookieSameSiteFilter::INCLUDE_LAX:
      *output = net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX;
      return true;
    case network::mojom::CookieSameSiteFilter::DO_NOT_INCLUDE:
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
  if (!mojo_options.ReadCookieSameSiteFilter(&same_site_cookie_context))
    return false;
  cookie_options->set_same_site_cookie_context(same_site_cookie_context);

  if (mojo_options.update_access_time())
    cookie_options->set_update_access_time();
  else
    cookie_options->set_do_not_update_access_time();

  base::Optional<base::Time> optional_server_time;
  if (!mojo_options.ReadServerTime(&optional_server_time))
    return false;
  if (optional_server_time) {
    cookie_options->set_server_time(*optional_server_time);
  }
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

}  // namespace mojo
