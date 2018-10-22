// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/url_utils.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/common/url_schemes.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace content {

bool HasWebUIScheme(const GURL& url) {
  return url.SchemeIs(kChromeDevToolsScheme) ||
         url.SchemeIs(kChromeUIScheme);
}

bool IsSavableURL(const GURL& url) {
  for (auto& scheme : GetSavableSchemes()) {
    if (url.SchemeIs(scheme))
      return true;
  }
  return false;
}

// PlzNavigate
bool IsURLHandledByNetworkStack(const GURL& url) {
  CHECK(IsBrowserSideNavigationEnabled());

  // Javascript URLs, srcdoc, schemes that don't load data should not send a
  // request to the network stack.
  if (url.SchemeIs(url::kJavaScriptScheme) || url.is_empty() ||
      url == content::kAboutSrcDocURL) {
    return false;
  }

  for (const auto& scheme : url::GetEmptyDocumentSchemes()) {
    if (url.SchemeIs(scheme))
      return false;
  }

  // Renderer debug URLs (e.g. chrome://kill) are handled in the renderer
  // process directly and should not be sent to the network stack.
  if (IsRendererDebugURL(url))
    return false;

  // For you information, even though a "data:" url doesn't generate actual
  // network requests, it is handled by the network stack and so must return
  // true. The reason is that a few "data:" urls can't be handled locally. For
  // instance:
  // - the ones that result in downloads.
  // - the ones that are invalid. An error page must be served instead.
  // - the ones that have an unsupported MIME type.
  // - the ones that target the top-level frame on Android.

  return true;
}

bool IsURLHandledByNetworkService(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIsWSOrWSS() ||
         url.SchemeIs(url::kFtpScheme) || url.SchemeIs(url::kGopherScheme);
}

bool IsRendererDebugURL(const GURL& url) {
  if (!url.is_valid())
    return false;

  if (url.SchemeIs(url::kJavaScriptScheme))
    return true;

  if (!url.SchemeIs(kChromeUIScheme))
    return false;

  if (url == kChromeUICheckCrashURL || url == kChromeUIBadCastCrashURL ||
      url == kChromeUICrashURL || url == kChromeUIDumpURL ||
      url == kChromeUIKillURL || url == kChromeUIHangURL ||
      url == kChromeUIShorthangURL || url == kChromeUIMemoryExhaustURL) {
    return true;
  }

#if defined(ADDRESS_SANITIZER)
  if (url == kChromeUICrashHeapOverflowURL ||
      url == kChromeUICrashHeapUnderflowURL ||
      url == kChromeUICrashUseAfterFreeURL) {
    return true;
  }
#endif

#if defined(OS_WIN)
  if (url == kChromeUIHeapCorruptionCrashURL)
    return true;
#endif

#if DCHECK_IS_ON()
  if (url == kChromeUICrashDcheckURL)
    return true;
#endif

#if defined(OS_WIN) && defined(ADDRESS_SANITIZER)
  if (url == kChromeUICrashCorruptHeapBlockURL ||
      url == kChromeUICrashCorruptHeapURL) {
    return true;
  }
#endif

  return false;
}

bool IsSafeRedirectTarget(const GURL& from_url, const GURL& to_url) {
  static base::NoDestructor<std::set<std::string>> kUnsafeSchemes(
      std::set<std::string>({
          url::kAboutScheme, url::kDataScheme, url::kFileScheme,
          url::kFileSystemScheme,
      }));
  if (HasWebUIScheme(to_url))
    return false;
  if (kUnsafeSchemes->find(to_url.scheme()) == kUnsafeSchemes->end())
    return true;
  if (from_url.is_empty())
    return false;
  if (from_url.SchemeIsFile() && to_url.SchemeIsFile())
    return true;
  if (from_url.SchemeIsFileSystem() && to_url.SchemeIsFileSystem())
    return true;
  return false;
}

}  // namespace content
