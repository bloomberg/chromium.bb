// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_COOKIE_SETTINGS_H_
#define SERVICES_NETWORK_COOKIE_SETTINGS_H_

#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/same_party_context.h"
#include "services/network/public/cpp/session_cookie_delete_predicate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace net {
class SiteForCookies;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace network {

// Handles cookie access and deletion logic for the network service.
class COMPONENT_EXPORT(NETWORK_SERVICE) CookieSettings
    : public content_settings::CookieSettingsBase {
 public:
  CookieSettings();

  CookieSettings(const CookieSettings&) = delete;
  CookieSettings& operator=(const CookieSettings&) = delete;

  ~CookieSettings() override;

  void set_content_settings(const ContentSettingsForOneType& content_settings) {
    content_settings_ = content_settings;
  }

  void set_block_third_party_cookies(bool block_third_party_cookies) {
    block_third_party_cookies_ = block_third_party_cookies;
  }

  bool are_third_party_cookies_blocked() const {
    return block_third_party_cookies_;
  }

  void set_secure_origin_cookies_allowed_schemes(
      const std::vector<std::string>& secure_origin_cookies_allowed_schemes) {
    secure_origin_cookies_allowed_schemes_.clear();
    secure_origin_cookies_allowed_schemes_.insert(
        secure_origin_cookies_allowed_schemes.begin(),
        secure_origin_cookies_allowed_schemes.end());
  }

  void set_matching_scheme_cookies_allowed_schemes(
      const std::vector<std::string>& matching_scheme_cookies_allowed_schemes) {
    matching_scheme_cookies_allowed_schemes_.clear();
    matching_scheme_cookies_allowed_schemes_.insert(
        matching_scheme_cookies_allowed_schemes.begin(),
        matching_scheme_cookies_allowed_schemes.end());
  }

  void set_third_party_cookies_allowed_schemes(
      const std::vector<std::string>& third_party_cookies_allowed_schemes) {
    third_party_cookies_allowed_schemes_.clear();
    third_party_cookies_allowed_schemes_.insert(
        third_party_cookies_allowed_schemes.begin(),
        third_party_cookies_allowed_schemes.end());
  }

  void set_content_settings_for_legacy_cookie_access(
      const ContentSettingsForOneType& settings) {
    settings_for_legacy_cookie_access_ = settings;
  }

  void set_storage_access_grants(const ContentSettingsForOneType& settings) {
    storage_access_grants_ = settings;
  }

  // Returns a predicate that takes the domain of a cookie and a bool whether
  // the cookie is secure and returns true if the cookie should be deleted on
  // exit.
  DeleteCookiePredicate CreateDeleteCookieOnExitPredicate() const;

  // content_settings::CookieSettingsBase:
  void GetSettingForLegacyCookieAccess(const std::string& cookie_domain,
                                       ContentSetting* setting) const override;
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies) const override;

  // Returns true iff "privacy mode" should be enabled for the URL request in
  // question, according to the user's settings.
  bool IsPrivacyModeEnabled(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin,
      net::SamePartyContext::Type same_party_context_type) const;

  // Returns true if the given cookie is accessible according to user
  // cookie-blocking settings. Assumes that the cookie is otherwise accessible
  // (i.e. that the cookie is otherwise valid with no other exclusion reasons).
  bool IsCookieAccessible(
      const net::CanonicalCookie& cookie,
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin) const;

  // Annotates `maybe_included_cookies` and `excluded_cookies` with
  // ExclusionReasons if needed, per user's cookie blocking settings, and
  // ensures that all excluded cookies from `maybe_included_cookies` are moved
  // to `excluded_cookies`.  Returns false if the CookieSettings blocks access
  // to all cookies; true otherwise. Does not change the relative ordering of
  // the cookies in `maybe_included_cookies`, since this order is important when
  // building the cookie line.
  bool AnnotateAndMoveUserBlockedCookies(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin* top_frame_origin,
      net::CookieAccessResultList& maybe_included_cookies,
      net::CookieAccessResultList& excluded_cookies) const;

 private:
  // Returns whether third-party cookie blocking should be bypassed (i.e. always
  // allow the cookie regardless of cookie content settings and third-party
  // cookie blocking settings.
  // This just checks the scheme of the |url| and |site_for_cookies|:
  //  - Allow cookies if the |site_for_cookies| is a chrome:// scheme URL, and
  //    the |url| has a secure scheme.
  //  - Allow cookies if the |site_for_cookies| and the |url| match in scheme
  //    and both have the Chrome extensions scheme.
  bool ShouldAlwaysAllowCookies(const GURL& url,
                                const GURL& first_party_url) const;

  // content_settings::CookieSettingsBase:
  ContentSetting GetCookieSettingInternal(
      const GURL& url,
      const GURL& first_party_url,
      bool is_third_party_request,
      content_settings::SettingSource* source) const override;

  struct CookieSettingWithMetadata {
    ContentSetting cookie_setting;
    bool blocked_by_third_party_setting;
  };

  // Returns the cookie setting for the given request, along with metadata
  // associated with the lookup. Namely, whether the setting is due to
  // third-party cookie blocking settings or not.
  CookieSettingWithMetadata GetCookieSettingWithMetadata(
      const GURL& url,
      const GURL& first_party_url,
      bool is_third_party_request) const;

  // An overload of the above, which determines `first_party_url` and
  // `is_third_party_request` appropriately.
  CookieSettingWithMetadata GetCookieSettingWithMetadata(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin* top_frame_origin) const;

  // Returns whether the given cookie should be allowed to be sent, according to
  // the user's settings. Assumes that the `cookie.access_result` has been
  // correctly filled in by the cookie store. Note that the cookie may be
  // "excluded" for other reasons, even if this method returns true.
  bool IsCookieAllowed(
      const CookieSettings::CookieSettingWithMetadata& setting_with_metadata,
      const net::CookieWithAccessResult& cookie) const;

  // Returns whether *some* cookie would be allowed to be sent in this context,
  // according to the user's settings. Note that cookies may still be "excluded"
  // for other reasons, even if this method returns true.
  //
  // `is_same_party` should reflect whether the context is same-party *and*
  // whether the (real or hypothetical) cookie is SameParty.
  //
  // `record_metrics` indicates whether metrics should be recorded for this
  // call. I.e., whether we are checking access to a real cookie, or a
  // hypothetical one.
  bool IsHypotheticalCookieAllowed(
      const CookieSettings::CookieSettingWithMetadata& setting_with_metadata,
      bool is_same_party,
      bool record_metrics) const;

  // Returns true if at least one content settings is session only.
  bool HasSessionOnlyOrigins() const;

  ContentSettingsForOneType content_settings_;
  bool block_third_party_cookies_ = false;
  std::set<std::string> secure_origin_cookies_allowed_schemes_;
  std::set<std::string> matching_scheme_cookies_allowed_schemes_;
  std::set<std::string> third_party_cookies_allowed_schemes_;
  ContentSettingsForOneType settings_for_legacy_cookie_access_;
  // Used to represent storage access grants provided by the StorageAccessAPI.
  // Will only be populated when the StorageAccessAPI feature is enabled
  // https://crbug.com/989663.
  ContentSettingsForOneType storage_access_grants_;
  const bool sameparty_cookies_considered_first_party_ =
      base::FeatureList::IsEnabled(
          net::features::kSamePartyCookiesConsideredFirstParty);
};

}  // namespace network

#endif  // SERVICES_NETWORK_COOKIE_SETTINGS_H_
