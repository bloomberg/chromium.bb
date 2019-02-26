// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/thumbnail_fetch_by_url.h"

#include <utility>

#include "base/bind.h"
#include "components/image_fetcher/core/image_fetcher.h"

namespace offline_pages {

namespace {
net::NetworkTrafficAnnotationTag TrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("prefetch_thumbnail", R"(
        semantics {
          sender: "Offline Pages Prefetch"
          description:
            "Chromium fetches suggested articles for offline viewing. This"
            " network request is for a thumbnail that matches the article."
          trigger:
            "Two attempts, directly before and after the article is fetched."
          data:
            "The requested thumbnail URL."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable offline prefetch by toggling "
            "'Download articles for you' in settings under Downloads or "
            "by toggling chrome://flags#offline-prefetch."
          chrome_policy {
            NTPContentSuggestionsEnabled {
              policy_options {mode: MANDATORY}
              NTPContentSuggestionsEnabled: false
            }
          }
        })");
}

}  // namespace

void FetchThumbnailByURL(
    base::OnceCallback<void(const std::string& image_data)> callback,
    image_fetcher::ImageFetcher* fetcher,
    const GURL thumbnail_url) {
  auto forward_callback =
      [](base::OnceCallback<void(const std::string& image_data)> callback,
         const std::string& image_data,
         const image_fetcher::RequestMetadata& request_metadata) {
        std::move(callback).Run(image_data);
      };
  fetcher->FetchImageData(/*id=*/std::string(), thumbnail_url,
                          base::BindOnce(forward_callback, std::move(callback)),
                          TrafficAnnotation());
}

}  // namespace offline_pages
