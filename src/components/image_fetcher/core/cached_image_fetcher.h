// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_CACHED_IMAGE_FETCHER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_CACHED_IMAGE_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace gfx {
class Image;
class Size;
}  // namespace gfx

namespace image_fetcher {

class ImageCache;
class ImageFetcher;
struct RequestMetadata;

// TODO(wylieb): Transcode the image once it's downloaded.
// TODO(wylieb): Consider creating a struct to encapsulate the request.
// CachedImageFetcher takes care of fetching images from the network and caching
// them.
class CachedImageFetcher : public ImageFetcher {
 public:
  CachedImageFetcher(std::unique_ptr<ImageFetcher> image_fetcher,
                     std::unique_ptr<ImageCache> image_cache);
  ~CachedImageFetcher() override;

  // ImageFetcher:
  void SetDataUseServiceName(DataUseServiceName data_use_service_name) override;
  void SetDesiredImageFrameSize(const gfx::Size& size) override;
  void SetImageDownloadLimit(
      base::Optional<int64_t> max_download_bytes) override;
  void FetchImageAndData(
      const std::string& id,
      const GURL& image_url,
      ImageDataFetcherCallback image_data_callback,
      ImageFetcherCallback image_callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  ImageDecoder* GetImageDecoder() override;

 private:
  // Cache
  void OnImageFetchedFromCache(
      const std::string& id,
      const GURL& image_url,
      ImageDataFetcherCallback image_data_callback,
      ImageFetcherCallback image_callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      std::string image_data);
  void OnImageDecodedFromCache(
      const std::string& id,
      const GURL& image_url,
      ImageDataFetcherCallback image_data_callback,
      ImageFetcherCallback image_callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const std::string& image_data,
      const gfx::Image& image);

  // Network
  void FetchImageFromNetwork(
      const std::string& id,
      const GURL& image_url,
      ImageDataFetcherCallback image_data_callback,
      ImageFetcherCallback image_callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  void OnImageFetchedFromNetwork(ImageFetcherCallback image_callback,
                                 const std::string& id,
                                 const gfx::Image& image,
                                 const RequestMetadata& request_metadata);
  void OnImageDataFetchedFromNetwork(
      ImageDataFetcherCallback image_data_callback,
      const GURL& image_url,
      const std::string& image_data,
      const RequestMetadata& request_metadata);
  void OnImageDecodedFromNetwork(const GURL& image_url,
                                 const gfx::Image& image);

  std::unique_ptr<ImageFetcher> image_fetcher_;
  std::unique_ptr<ImageCache> image_cache_;

  gfx::Size desired_image_frame_size_;

  base::WeakPtrFactory<CachedImageFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcher);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_CACHED_IMAGE_FETCHER_H_
