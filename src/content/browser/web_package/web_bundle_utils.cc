// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_utils.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "url/url_constants.h"
#endif  // defined(OS_ANDROID)

namespace content {
namespace web_bundle_utils {
namespace {

const base::FilePath::CharType kWebBundleFileExtension[] =
    FILE_PATH_LITERAL(".wbn");

}  // namespace

bool IsSupportedFileScheme(const GURL& url) {
  if (url.SchemeIsFile())
    return true;
#if defined(OS_ANDROID)
  if (url.SchemeIs(url::kContentScheme))
    return true;
#endif  // defined(OS_ANDROID)
  return false;
}

bool CanLoadAsTrustableWebBundleFile(const GURL& url) {
  if (!GetContentClient()->browser()->CanAcceptUntrustedExchangesIfNeeded())
    return false;
  if (!IsSupportedFileScheme(url))
    return false;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTrustableWebBundleFileUrl)) {
    return false;
  }
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kTrustableWebBundleFileUrl) == url.spec();
}

bool CanLoadAsWebBundleFile(const GURL& url) {
  if (!base::FeatureList::IsEnabled(features::kWebBundles))
    return false;
  return IsSupportedFileScheme(url);
}

bool CanLoadAsWebBundle(const GURL& url, const std::string& mime_type) {
  if (!base::FeatureList::IsEnabled(features::kWebBundles))
    return false;
  // Currently loading Web Bundle file from server response is not
  // implemented yet.
  if (!IsSupportedFileScheme(url))
    return false;
  return mime_type == kWebBundleFileMimeTypeWithoutParameters;
}

bool GetWebBundleFileMimeTypeFromFile(const base::FilePath& path,
                                      std::string* mime_type) {
  DCHECK(mime_type);
  if (!base::FeatureList::IsEnabled(features::kWebBundles))
    return false;
  if (path.Extension() != kWebBundleFileExtension)
    return false;
  *mime_type = kWebBundleFileMimeTypeWithoutParameters;
  return true;
}

GURL GetSynthesizedUrlForWebBundle(const GURL& web_bundle_file_url,
                                   const GURL& url_in_bundles) {
  url::Replacements<char> replacements;

  url::Replacements<char> clear_ref;
  clear_ref.ClearRef();
  std::string query_string = url_in_bundles.ReplaceComponents(clear_ref).spec();
  url::Component new_query(0, query_string.size());
  replacements.SetQuery(query_string.c_str(), new_query);

  if (!url_in_bundles.has_ref()) {
    replacements.ClearRef();
    return web_bundle_file_url.ReplaceComponents(replacements);
  }
  url::Component new_ref(0, url_in_bundles.ref().size());
  std::string ref_string = url_in_bundles.ref();
  replacements.SetRef(ref_string.c_str(), new_ref);
  return web_bundle_file_url.ReplaceComponents(replacements);
}

}  // namespace web_bundle_utils
}  // namespace content
