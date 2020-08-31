// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_THUMBNAIL_FETCHER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_THUMBNAIL_FETCHER_H_

#include <string>
#include "base/callback.h"

namespace ntp_snippets {
class ContentSuggestionsService;
}

namespace offline_pages {
struct ClientId;

// Fetches thumbnails that represent suggested pages.
class ThumbnailFetcher {
 public:
  using ImageDataFetchedCallback =
      base::OnceCallback<void(const std::string& image_data)>;

  // Thumbnails larger than 200KB are not retained. Thumbnails are typically
  // around 10KB.
  static constexpr int64_t kMaxThumbnailSize = 200000;

  virtual ~ThumbnailFetcher() {}

  virtual void SetContentSuggestionsService(
      ntp_snippets::ContentSuggestionsService* content_suggestions) {}

  // Fetches a thumbnail for a suggestion. Calls callback when the fetch
  // completes. |image_data| is empty if the fetch failed, otherwise it will
  // be raw image data that may be decoded with image_fetcher::ImageDecoder.
  virtual void FetchSuggestionImageData(const ClientId& client_id,
                                        ImageDataFetchedCallback callback) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_THUMBNAIL_FETCHER_H_
