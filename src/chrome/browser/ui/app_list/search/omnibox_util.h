// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_UTIL_H_

#include "net/traffic_annotation/network_traffic_annotation.h"

namespace app_list {

// The magic number 1500 is the highest score of an omnibox result.
// See comments in autocomplete_provider.h.
constexpr double kMaxOmniboxScore = 1500.0;

// Traffic annotation details for fetching Omnibox image icons.
constexpr net::NetworkTrafficAnnotationTag kOmniboxTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cros_launcher_omnibox", R"(
        semantics {
          sender: "Chrome OS Launcher"
          description:
            "Chrome OS provides search suggestions when a user types a query "
            "into the launcher. This request downloads an image icon for a "
            "suggested result in order to provide more information."
          trigger:
            "Change of results for the query typed by the user into the "
            "launcher."
          data:
            "URL of the image to be downloaded. This URL corresponds to "
            "search suggestions for the user's query."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Search autocomplete and suggestions can be disabled in Chrome OS "
            "settings. Image icons cannot be disabled separately to this."
          policy_exception_justification:
            "No content is uploaded or saved, this request downloads a "
            "publicly available image."
        })");

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_UTIL_H_
