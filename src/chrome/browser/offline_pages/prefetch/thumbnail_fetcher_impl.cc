// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/thumbnail_fetcher_impl.h"

#include "base/bind.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/client_namespace_constants.h"

namespace offline_pages {
namespace {

void FetchCompleteAndReportUMA(
    ThumbnailFetcher::ImageDataFetchedCallback callback,
    const std::string& image_data) {
  if (image_data.empty() ||
      image_data.size() > ThumbnailFetcherImpl::kMaxThumbnailSize) {
    std::move(callback).Run(std::string());
  } else {
    std::move(callback).Run(image_data);
  }
}

}  // namespace

ThumbnailFetcherImpl::ThumbnailFetcherImpl() = default;
ThumbnailFetcherImpl::~ThumbnailFetcherImpl() = default;

void ThumbnailFetcherImpl::SetContentSuggestionsService(
    ntp_snippets::ContentSuggestionsService* content_suggestions) {
  DCHECK(!content_suggestions_);  // Called once.
  content_suggestions_ = content_suggestions;
}

void ThumbnailFetcherImpl::FetchSuggestionImageData(
    const ClientId& client_id,
    ImageDataFetchedCallback callback) {
  DCHECK(client_id.name_space == kSuggestedArticlesNamespace);
  DCHECK(content_suggestions_);

  content_suggestions_->FetchSuggestionImageData(
      ntp_snippets::ContentSuggestion::ID(
          ntp_snippets::Category::FromKnownCategory(
              ntp_snippets::KnownCategories::ARTICLES),
          client_id.id),
      base::BindOnce(FetchCompleteAndReportUMA, std::move(callback)));
}

}  // namespace offline_pages
