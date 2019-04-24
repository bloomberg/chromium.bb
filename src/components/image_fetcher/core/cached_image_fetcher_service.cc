// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cached_image_fetcher_service.h"

#include <utility>

#include "base/time/clock.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/cached_image_fetcher.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace image_fetcher {

CachedImageFetcherService::CachedImageFetcherService(
    std::unique_ptr<ImageDecoder> image_decoder,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<ImageCache> image_cache,
    bool read_only)
    : cached_image_fetcher_(std::make_unique<CachedImageFetcher>(
          std::make_unique<ImageFetcherImpl>(std::move(image_decoder),
                                             url_loader_factory),
          image_cache,
          read_only)),
      image_cache_(image_cache) {}

CachedImageFetcherService::~CachedImageFetcherService() = default;

ImageFetcher* CachedImageFetcherService::GetCachedImageFetcher() {
  return cached_image_fetcher_.get();
}

scoped_refptr<ImageCache> CachedImageFetcherService::ImageCacheForTesting()
    const {
  return image_cache_;
}

}  // namespace image_fetcher
