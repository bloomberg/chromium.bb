// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cached_image_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/image_fetcher/core/storage/image_cache.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace image_fetcher {

namespace {

void DataCallbackIfPresent(ImageDataFetcherCallback data_callback,
                           const std::string& image_data,
                           const image_fetcher::RequestMetadata& metadata) {
  if (data_callback.is_null()) {
    return;
  }
  std::move(data_callback).Run(image_data, metadata);
}

void ImageCallbackIfPresent(ImageFetcherCallback image_callback,
                            const std::string& id,
                            const gfx::Image& image,
                            const image_fetcher::RequestMetadata& metadata) {
  if (image_callback.is_null()) {
    return;
  }
  std::move(image_callback).Run(id, image, metadata);
}

bool EncodeSkBitmapToPNG(const SkBitmap& bitmap,
                         std::vector<unsigned char>* dest) {
  if (!bitmap.readyToDraw() || bitmap.isNull()) {
    return false;
  }

  return gfx::PNGCodec::Encode(
      static_cast<const unsigned char*>(bitmap.getPixels()),
      gfx::PNGCodec::FORMAT_RGBA, gfx::Size(bitmap.width(), bitmap.height()),
      static_cast<int>(bitmap.rowBytes()), /* discard_transparency */ false,
      std::vector<gfx::PNGCodec::Comment>(), dest);
}

}  // namespace

CachedImageFetcher::CachedImageFetcher(
    std::unique_ptr<ImageFetcher> image_fetcher,
    std::unique_ptr<ImageCache> image_cache)
    : image_fetcher_(std::move(image_fetcher)),
      image_cache_(std::move(image_cache)),
      weak_ptr_factory_(this) {
  DCHECK(image_fetcher_);
  DCHECK(image_cache_);
}

CachedImageFetcher::~CachedImageFetcher() = default;

void CachedImageFetcher::SetDataUseServiceName(
    DataUseServiceName data_use_service_name) {
  image_fetcher_->SetDataUseServiceName(data_use_service_name);
}

void CachedImageFetcher::SetDesiredImageFrameSize(const gfx::Size& size) {
  image_fetcher_->SetDesiredImageFrameSize(size);
  desired_image_frame_size_ = size;
}

void CachedImageFetcher::SetImageDownloadLimit(
    base::Optional<int64_t> max_download_bytes) {
  image_fetcher_->SetImageDownloadLimit(max_download_bytes);
}

ImageDecoder* CachedImageFetcher::GetImageDecoder() {
  return image_fetcher_->GetImageDecoder();
}

void CachedImageFetcher::FetchImageAndData(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  // First, try to load the image from the cache, then try the network.
  image_cache_->LoadImage(
      image_url.spec(),
      base::BindOnce(&CachedImageFetcher::OnImageFetchedFromCache,
                     weak_ptr_factory_.GetWeakPtr(), id, image_url,
                     std::move(data_callback), std::move(image_callback),
                     traffic_annotation));
}

void CachedImageFetcher::OnImageFetchedFromCache(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    std::string image_data) {
  if (image_data.empty()) {
    // Fetching from the DB failed, start a network fetch.
    FetchImageFromNetwork(id, image_url, std::move(data_callback),
                          std::move(image_callback), traffic_annotation);
  } else {
    // TODO(wylieb): On Android, do this in-process.
    GetImageDecoder()->DecodeImage(
        image_data, desired_image_frame_size_,
        base::BindRepeating(&CachedImageFetcher::OnImageDecodedFromCache,
                            weak_ptr_factory_.GetWeakPtr(), id, image_url,
                            base::Passed(std::move(data_callback)),
                            base::Passed(std::move(image_callback)),
                            traffic_annotation, image_data));
  }
}

void CachedImageFetcher::OnImageDecodedFromCache(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& image_data,
    const gfx::Image& image) {
  if (image.IsEmpty()) {
    FetchImageFromNetwork(id, image_url, std::move(data_callback),
                          std::move(image_callback), traffic_annotation);
    // Decoding error, delete image from cache.
    image_cache_->DeleteImage(image_url.spec());
  } else {
    DataCallbackIfPresent(std::move(data_callback), image_data,
                          RequestMetadata());
    ImageCallbackIfPresent(std::move(image_callback), id, image,
                           RequestMetadata());
  }
}

void CachedImageFetcher::FetchImageFromNetwork(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  // Fetch image data and the image itself. The image data will be stored in
  // the image cache, and the image will be returned to the caller.
  image_fetcher_->FetchImageAndData(
      id, image_url,
      base::BindOnce(&CachedImageFetcher::OnImageDataFetchedFromNetwork,
                     weak_ptr_factory_.GetWeakPtr(), std::move(data_callback),
                     image_url),
      base::BindOnce(&CachedImageFetcher::OnImageFetchedFromNetwork,
                     weak_ptr_factory_.GetWeakPtr(), std::move(image_callback)),
      traffic_annotation);
}

void CachedImageFetcher::OnImageFetchedFromNetwork(
    ImageFetcherCallback image_callback,
    const std::string& id,
    const gfx::Image& image,
    const RequestMetadata& request_metadata) {
  // The image has been deocded by the fetcher already, return straight to the
  // caller.
  ImageCallbackIfPresent(std::move(image_callback), id, image,
                         request_metadata);
}

void CachedImageFetcher::OnImageDataFetchedFromNetwork(
    ImageDataFetcherCallback data_callback,
    const GURL& image_url,
    const std::string& image_data,
    const RequestMetadata& request_metadata) {
  DataCallbackIfPresent(std::move(data_callback), image_data, request_metadata);
  GetImageDecoder()->DecodeImage(
      image_data, /* Decoding for cache shouldn't specify size */ gfx::Size(),
      base::BindRepeating(&CachedImageFetcher::OnImageDecodedFromNetwork,
                          weak_ptr_factory_.GetWeakPtr(), image_url));
}

void CachedImageFetcher::OnImageDecodedFromNetwork(const GURL& image_url,
                                                   const gfx::Image& image) {
  std::vector<unsigned char> encoded_data;
  if (!EncodeSkBitmapToPNG(*image.ToSkBitmap(), &encoded_data)) {
    return;
  }

  image_cache_->SaveImage(
      image_url.spec(), std::string(encoded_data.begin(), encoded_data.end()));
}

}  // namespace image_fetcher
