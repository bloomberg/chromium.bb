// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/cookie/canonical_cookie.h"

#include <memory>
#include <utility>

#include "base/optional.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "url/gurl.h"

// Assumptions made by static_casts used in this file.
STATIC_ASSERT_ENUM(net::CookieSameSite::UNSPECIFIED,
                   network::mojom::CookieSameSite::UNSPECIFIED);
STATIC_ASSERT_ENUM(net::CookieSameSite::NO_RESTRICTION,
                   network::mojom::CookieSameSite::NO_RESTRICTION);
STATIC_ASSERT_ENUM(net::CookieSameSite::LAX_MODE,
                   network::mojom::CookieSameSite::LAX_MODE);
STATIC_ASSERT_ENUM(net::CookieSameSite::STRICT_MODE,
                   network::mojom::CookieSameSite::STRICT_MODE);

STATIC_ASSERT_ENUM(net::CookiePriority::COOKIE_PRIORITY_LOW,
                   network::mojom::CookiePriority::LOW);
STATIC_ASSERT_ENUM(net::CookiePriority::COOKIE_PRIORITY_MEDIUM,
                   network::mojom::CookiePriority::MEDIUM);
STATIC_ASSERT_ENUM(net::CookiePriority::COOKIE_PRIORITY_HIGH,
                   network::mojom::CookiePriority::HIGH);
STATIC_ASSERT_ENUM(net::CookiePriority::COOKIE_PRIORITY_DEFAULT,
                   blink::CanonicalCookie::kDefaultPriority);

STATIC_ASSERT_ENUM(net::CookieSourceScheme::kUnset,
                   network::mojom::CookieSourceScheme::kUnset);
STATIC_ASSERT_ENUM(net::CookieSourceScheme::kNonSecure,
                   network::mojom::CookieSourceScheme::kNonSecure);
STATIC_ASSERT_ENUM(net::CookieSourceScheme::kSecure,
                   network::mojom::CookieSourceScheme::kSecure);

namespace blink {

namespace {

net::CanonicalCookie ToNetCanonicalCookie(const CanonicalCookie& cookie) {
  net::CanonicalCookie net_cookie(
      cookie.Name().Utf8(), cookie.Value().Utf8(), cookie.Domain().Utf8(),
      cookie.Path().Utf8(), cookie.CreationDate(), cookie.ExpiryDate(),
      cookie.LastAccessDate(), cookie.IsSecure(), cookie.IsHttpOnly(),
      static_cast<net::CookieSameSite>(cookie.SameSite()),
      static_cast<net::CookiePriority>(cookie.Priority()),
      static_cast<net::CookieSourceScheme>(cookie.SourceScheme()));
  DCHECK(net_cookie.IsCanonical());
  return net_cookie;
}

}  // namespace

CanonicalCookie::CanonicalCookie() = default;

CanonicalCookie::CanonicalCookie(
    WebString name,
    WebString value,
    WebString domain,
    WebString path,
    base::Time creation,
    base::Time expiration,
    base::Time last_access,
    bool is_secure,
    bool is_http_only,
    network::mojom::CookieSameSite same_site,
    network::mojom::CookiePriority priority,
    network::mojom::CookieSourceScheme source_scheme)
    : name_(std::move(name)),
      value_(std::move(value)),
      domain_(std::move(domain)),
      path_(std::move(path)),
      creation_(creation),
      expiration_(expiration),
      last_access_(last_access),
      is_secure_(is_secure),
      is_http_only_(is_http_only),
      same_site_(same_site),
      priority_(priority),
      source_scheme_(source_scheme) {
  DCHECK(ToNetCanonicalCookie(*this).IsCanonical());
}

CanonicalCookie::CanonicalCookie(const CanonicalCookie& other) = default;

CanonicalCookie& CanonicalCookie::operator=(const CanonicalCookie& other) =
    default;

CanonicalCookie::~CanonicalCookie() = default;

namespace {

// TODO(crbug.com/851889): WebURL::operator GURL() is only available if
//                         !INSIDE_BLINK. Remove ToGURL() when this changes.
GURL ToGURL(const WebURL& url) {
  return url.IsNull()
             ? GURL()
             : GURL(url.GetString().Utf8(), url.GetParsed(), url.IsValid());
}

}  // namespace

// static
base::Optional<CanonicalCookie> CanonicalCookie::Create(
    const WebURL& url,
    const WebString& cookie_line,
    base::Time creation_time) {
  std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
      ToGURL(url), cookie_line.Utf8(), creation_time,
      base::nullopt /* server_time */);
  if (!cookie)
    return base::nullopt;
  return CanonicalCookie(
      WebString::FromUTF8(cookie->Name()), WebString::FromUTF8(cookie->Value()),
      WebString::FromUTF8(cookie->Domain()),
      WebString::FromUTF8(cookie->Path()), cookie->CreationDate(),
      cookie->ExpiryDate(), cookie->LastAccessDate(), cookie->IsSecure(),
      cookie->IsHttpOnly(),
      static_cast<network::mojom::CookieSameSite>(cookie->SameSite()),
      static_cast<network::mojom::CookiePriority>(cookie->Priority()),
      static_cast<network::mojom::CookieSourceScheme>(cookie->SourceScheme()));
}

// static
base::Optional<CanonicalCookie> CanonicalCookie::Create(
    WebString name,
    WebString value,
    WebString domain,
    WebString path,
    base::Time creation,
    base::Time expiration,
    base::Time last_access,
    bool is_secure,
    bool is_http_only,
    network::mojom::CookieSameSite same_site,
    network::mojom::CookiePriority priority,
    network::mojom::CookieSourceScheme source_scheme) {
  net::CanonicalCookie net_cookie(
      name.Utf8(), value.Utf8(), domain.Utf8(), path.Utf8(), creation,
      expiration, last_access, is_secure, is_http_only,
      static_cast<net::CookieSameSite>(same_site),
      static_cast<net::CookiePriority>(priority),
      static_cast<net::CookieSourceScheme>(source_scheme));
  if (!net_cookie.IsCanonical())
    return base::nullopt;

  return CanonicalCookie(std::move(name), std::move(value), std::move(domain),
                         std::move(path), creation, expiration, last_access,
                         is_secure, is_http_only, same_site, priority,
                         source_scheme);
}

constexpr const network::mojom::CookiePriority
    CanonicalCookie::kDefaultPriority;

}  // namespace blink
