// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_TEST_COOKIE_ACCESS_DELEGATE_H_
#define NET_COOKIES_TEST_COOKIE_ACCESS_DELEGATE_H_

#include <map>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/same_party_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

class SchemefulSite;

// CookieAccessDelegate for testing. You can set the return value for a given
// cookie_domain (modulo any leading dot). Calling GetAccessSemantics() will
// then return the given value, or UNKNOWN if you haven't set one.
class TestCookieAccessDelegate : public CookieAccessDelegate {
 public:
  TestCookieAccessDelegate();

  TestCookieAccessDelegate(const TestCookieAccessDelegate&) = delete;
  TestCookieAccessDelegate& operator=(const TestCookieAccessDelegate&) = delete;

  ~TestCookieAccessDelegate() override;

  // CookieAccessDelegate implementation:
  CookieAccessSemantics GetAccessSemantics(
      const CanonicalCookie& cookie) const override;
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const SiteForCookies& site_for_cookies) const override;
  void ComputeFirstPartySetMetadataMaybeAsync(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      base::OnceCallback<void(FirstPartySetMetadata)> callback) const override;
  absl::optional<net::SchemefulSite> FindFirstPartySetOwner(
      const net::SchemefulSite& site) const override;
  void RetrieveFirstPartySets(
      base::OnceCallback<void(
          base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>>)>
          callback) const override;

  // Sets the expected return value for any cookie whose Domain
  // matches |cookie_domain|. Pass the value of |cookie.Domain()| and any
  // leading dot will be discarded.
  void SetExpectationForCookieDomain(const std::string& cookie_domain,
                                     CookieAccessSemantics access_semantics);

  // Sets the expected return value for ShouldAlwaysAttachSameSiteCookies.
  // Can set schemes that always attach SameSite cookies, or schemes that always
  // attach SameSite cookies if the request URL is secure.
  void SetIgnoreSameSiteRestrictionsScheme(
      const std::string& site_for_cookies_scheme,
      bool require_secure_origin);

  // Set the test delegate's First-Party Sets. The map is keyed on the set's
  // owner site. The owner site should still be included in the std::set stored
  // in the map.
  void SetFirstPartySets(
      const base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>>&
          sets);

  void set_invoke_callbacks_asynchronously(bool async) {
    invoke_callbacks_asynchronously_ = async;
  }

 private:
  // Discard any leading dot in the domain string.
  std::string GetKeyForDomainValue(const std::string& domain) const;

  std::map<std::string, CookieAccessSemantics> expectations_;
  std::map<std::string, bool> ignore_samesite_restrictions_schemes_;
  base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>>
      first_party_sets_;
  bool invoke_callbacks_asynchronously_ = false;
};

}  // namespace net

#endif  // NET_COOKIES_TEST_COOKIE_ACCESS_DELEGATE_H_
