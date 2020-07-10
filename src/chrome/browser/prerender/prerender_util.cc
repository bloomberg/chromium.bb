// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_util.h"

#include "base/metrics/histogram_macros.h"
#include "components/google/core/common/google_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

namespace prerender {

namespace {

enum PrerenderSchemeCancelReason {
  PRERENDER_SCHEME_CANCEL_REASON_EXTERNAL_PROTOCOL,
  PRERENDER_SCHEME_CANCEL_REASON_DATA,
  PRERENDER_SCHEME_CANCEL_REASON_BLOB,
  PRERENDER_SCHEME_CANCEL_REASON_FILE,
  PRERENDER_SCHEME_CANCEL_REASON_FILESYSTEM,
  PRERENDER_SCHEME_CANCEL_REASON_WEBSOCKET,
  PRERENDER_SCHEME_CANCEL_REASON_FTP,
  PRERENDER_SCHEME_CANCEL_REASON_CHROME,
  PRERENDER_SCHEME_CANCEL_REASON_CHROME_EXTENSION,
  PRERENDER_SCHEME_CANCEL_REASON_ABOUT,
  PRERENDER_SCHEME_CANCEL_REASON_UNKNOWN,
  PRERENDER_SCHEME_CANCEL_REASON_MAX,
};

void ReportPrerenderSchemeCancelReason(PrerenderSchemeCancelReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "Prerender.SchemeCancelReason", reason,
      PRERENDER_SCHEME_CANCEL_REASON_MAX);
}

}  // namespace

bool IsGoogleOriginURL(const GURL& origin_url) {
  // ALLOW_NON_STANDARD_PORTS for integration tests with the embedded server.
  if (!google_util::IsGoogleDomainUrl(origin_url,
                                      google_util::DISALLOW_SUBDOMAIN,
                                      google_util::ALLOW_NON_STANDARD_PORTS)) {
    return false;
  }

  return (origin_url.path_piece() == "/") ||
         google_util::IsGoogleSearchUrl(origin_url);
}

void ReportPrerenderExternalURL() {
  ReportPrerenderSchemeCancelReason(
      PRERENDER_SCHEME_CANCEL_REASON_EXTERNAL_PROTOCOL);
}

void ReportUnsupportedPrerenderScheme(const GURL& url) {
  if (url.SchemeIs(url::kDataScheme)) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_DATA);
  } else if (url.SchemeIs(url::kBlobScheme)) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_BLOB);
  } else if (url.SchemeIsFile()) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_FILE);
  } else if (url.SchemeIsFileSystem()) {
    ReportPrerenderSchemeCancelReason(
        PRERENDER_SCHEME_CANCEL_REASON_FILESYSTEM);
  } else if (url.SchemeIs(url::kWsScheme) || url.SchemeIs(url::kWssScheme)) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_WEBSOCKET);
  } else if (url.SchemeIs(url::kFtpScheme)) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_FTP);
  } else if (url.SchemeIs(content::kChromeUIScheme)) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_CHROME);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  } else if (url.SchemeIs(extensions::kExtensionScheme)) {
    ReportPrerenderSchemeCancelReason(
        PRERENDER_SCHEME_CANCEL_REASON_CHROME_EXTENSION);
#endif
  } else if (url.SchemeIs(url::kAboutScheme)) {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_ABOUT);
  } else {
    ReportPrerenderSchemeCancelReason(PRERENDER_SCHEME_CANCEL_REASON_UNKNOWN);
  }
}

}  // namespace prerender
