// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_LITE_PAGE_REDIRECT_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_LITE_PAGE_REDIRECT_H_

#include <string>

#include "url/gurl.h"

namespace previews {

// The status of an attempted Lite Page Redirect preview.
enum class ServerLitePageStatus {
  // The preview has been attempted yet or we have not received a response from
  // the server yet.
  kUnknown = 0,

  // A preview was committed.
  kSuccess = 1,

  // The server bypassed the request and the preview was not committed.
  kBypass = 2,

  // The server redirected to another site. If this is the state at commit,
  // the preview was not committed. Before commit, this indicates that we
  // attempted the preview and may attempt another one if triggered later in
  // the redirect
  // chain.
  kRedirect = 3,

  // The server responded with some error, or didn't respond at all, and the
  // original page was loaded.
  kFailure = 4,

  // This navigation met all triggering criteria, but the configured
  // variations indicate that we were in a control group, so the preview was
  // not
  // triggered or committed.
  kControl = 5,
};

// Returns the string representation of |status|.
std::string ServerLitePageStatusToString(ServerLitePageStatus status);

// Returns true if the given |url| has the same domain as the lite page previews
// server.
bool IsLitePageRedirectPreviewDomain(const GURL& url);

// Returns true if the given URL is a Lite Page Preview URL. This does more
// checking than |IsLitePageRedirectPreviewDomain| so be sure to use the right
// one.
bool IsLitePageRedirectPreviewURL(const GURL& url);

// Attempts to extract the original URL from the given Previews URL. Returns
// false if |url| is not a valid Preview URL.
bool ExtractOriginalURLFromLitePageRedirectURL(const GURL& url,
                                               std::string* original_url);

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_LITE_PAGE_REDIRECT_H_
