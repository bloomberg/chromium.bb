// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_helpers.h"

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolation_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "crypto/sha2.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace web_app {

// The following string is used to build the directory name for
// shortcuts to chrome applications (the kind which are installed
// from a CRX).  Application shortcuts to URLs use the {host}_{path}
// for the name of this directory.  Hosts can't include an underscore.
// By starting this string with an underscore, we ensure that there
// are no naming conflicts.
const char kCrxAppPrefix[] = "_crx_";

std::string GenerateApplicationNameFromURL(const GURL& url) {
  return base::StrCat({url.host_piece(), "_", url.path_piece()});
}

std::string GenerateApplicationNameFromAppId(const AppId& app_id) {
  std::string t(kCrxAppPrefix);
  t.append(app_id);
  return t;
}

AppId GetAppIdFromApplicationName(const std::string& app_name) {
  std::string prefix(kCrxAppPrefix);
  if (app_name.substr(0, prefix.length()) != prefix)
    return std::string();
  return app_name.substr(prefix.length());
}

std::string GenerateAppIdUnhashed(
    const absl::optional<std::string>& manifest_id,
    const GURL& start_url) {
  // When manifest_id is specified, the app id is generated from
  // <start_url_origin>/<manifest_id>.
  // Note: start_url.DeprecatedGetOriginAsURL().spec() returns the origin ending
  // with slash.
  if (manifest_id.has_value()) {
    GURL app_id(start_url.DeprecatedGetOriginAsURL().spec() +
                manifest_id.value());
    DCHECK(app_id.is_valid());
    return app_id.spec();
  }
  return start_url.spec();
}

AppId GenerateAppId(const absl::optional<std::string>& manifest_id,
                    const GURL& start_url) {
  return crx_file::id_util::GenerateId(
      crypto::SHA256HashString(GenerateAppIdUnhashed(manifest_id, start_url)));
}

std::string GenerateAppIdUnhashedFromManifest(
    const blink::mojom::Manifest& manifest) {
  return GenerateAppIdUnhashed(
      manifest.id.has_value()
          ? absl::optional<std::string>(base::UTF16ToUTF8(manifest.id.value()))
          : absl::nullopt,
      manifest.start_url);
}

AppId GenerateAppIdFromManifest(const blink::mojom::Manifest& manifest) {
  return crx_file::id_util::GenerateId(
      crypto::SHA256HashString(GenerateAppIdUnhashedFromManifest(manifest)));
}

std::string GenerateRecommendedId(const GURL& start_url) {
  if (!start_url.is_valid()) {
    return base::EmptyString();
  }

  std::string full_url = start_url.spec();
  std::string origin = start_url.DeprecatedGetOriginAsURL().spec();
  DCHECK(!full_url.empty() && !origin.empty() &&
         origin.size() <= full_url.size());
  // Make recommended id starts with a leading slash so it's clear to developers
  // that it's a root-relative url path. In reality it's always root-relative
  // because the base_url is the origin.
  return full_url.substr(origin.size() - 1);
}

bool IsValidWebAppUrl(const GURL& app_url) {
  if (app_url.is_empty() || app_url.inner_url())
    return false;

  // TODO(crbug.com/1253234): Remove chrome-extension scheme and use
  // SchemeIsHTTPOrHTTPS() instead of IsValidWebAppUrl();
  return app_url.SchemeIs(url::kHttpScheme) ||
         app_url.SchemeIs(url::kHttpsScheme) ||
         app_url.SchemeIs("chrome-extension");
}

absl::optional<AppId> FindInstalledAppWithUrlInScope(Profile* profile,
                                                     const GURL& url,
                                                     bool window_only) {
  auto* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  return provider ? provider->registrar().FindInstalledAppWithUrlInScope(
                        url, window_only)
                  : absl::nullopt;
}

bool IsUrlInIsolatedAppScope(PrefService* prefs, const GURL& url) {
  // For short-term testing, also use kDirectSockets to enable isolated
  // application level. DirectSocket WPT and browser tests require application
  // isolation level.
  //
  // TODO(https://crbug.com/1206150): Figure out a better way to enable isolated
  // application level in tests.
  bool is_isolated_storage_enabled = base::FeatureList::IsEnabled(
      blink::features::kWebAppEnableIsolatedStorage);
  return base::FeatureList::IsEnabled(features::kDirectSockets) ||
         (is_isolated_storage_enabled &&
          web_app::GetStorageIsolationKey(prefs, url::Origin::Create(url)));
}

}  // namespace web_app
