// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_background_data.h"

namespace {
// The options to be added to a thumbnail image URL, specifying resolution,
// cropping, etc. Options appear on an image URL after the '=' character. This
// resolution matches the height an width of bg-sel-tile.
constexpr char kThumbnailImageOptions[] = "=w156-h117-p-k-no-nd-mv";
}  // namespace

std::string GetThumbnailImageOptionsForTesting() {
  return kThumbnailImageOptions;
}

CollectionInfo::CollectionInfo() = default;
CollectionInfo::CollectionInfo(const CollectionInfo&) = default;
CollectionInfo::CollectionInfo(CollectionInfo&&) = default;
CollectionInfo::~CollectionInfo() = default;

CollectionInfo& CollectionInfo::operator=(const CollectionInfo&) = default;
CollectionInfo& CollectionInfo::operator=(CollectionInfo&&) = default;

bool operator==(const CollectionInfo& lhs, const CollectionInfo& rhs) {
  return lhs.collection_id == rhs.collection_id &&
         lhs.collection_name == rhs.collection_name &&
         lhs.preview_image_url == rhs.preview_image_url;
}

bool operator!=(const CollectionInfo& lhs, const CollectionInfo& rhs) {
  return !(lhs == rhs);
}

CollectionInfo CollectionInfo::CreateFromProto(
    const ntp::background::Collection& collection) {
  CollectionInfo collection_info;
  collection_info.collection_id = collection.collection_id();
  collection_info.collection_name = collection.collection_name();
  // Use the first preview image as the representative one for the collection.
  if (collection.preview_size() > 0 && collection.preview(0).has_image_url()) {
    collection_info.preview_image_url =
        GURL(collection.preview(0).image_url() +
             ((collection.preview(0).image_url().find('=') == std::string::npos)
                  ? kThumbnailImageOptions
                  : std::string("")));
  }

  return collection_info;
}

CollectionImage::CollectionImage() = default;
CollectionImage::CollectionImage(const CollectionImage&) = default;
CollectionImage::CollectionImage(CollectionImage&&) = default;
CollectionImage::~CollectionImage() = default;

CollectionImage& CollectionImage::operator=(const CollectionImage&) = default;
CollectionImage& CollectionImage::operator=(CollectionImage&&) = default;

bool operator==(const CollectionImage& lhs, const CollectionImage& rhs) {
  return lhs.collection_id == rhs.collection_id &&
         lhs.asset_id == rhs.asset_id &&
         lhs.thumbnail_image_url == rhs.thumbnail_image_url &&
         lhs.image_url == rhs.image_url && lhs.attribution == rhs.attribution &&
         lhs.attribution_action_url == rhs.attribution_action_url;
}

bool operator!=(const CollectionImage& lhs, const CollectionImage& rhs) {
  return !(lhs == rhs);
}

CollectionImage CollectionImage::CreateFromProto(
    const std::string& collection_id,
    const ntp::background::Image& image,
    const std::string& default_image_options) {
  CollectionImage collection_image;
  collection_image.collection_id = collection_id;
  collection_image.asset_id = image.asset_id();
  // Without options added to the image, it is 512x512.
  collection_image.thumbnail_image_url = GURL(
      image.image_url() + ((image.image_url().find('=') == std::string::npos)
                               ? kThumbnailImageOptions
                               : std::string("")));
  // TODO(ramyan): Request resolution from service, instead of setting it here.
  collection_image.image_url = GURL(
      image.image_url() + ((image.image_url().find('=') == std::string::npos)
                               ? default_image_options
                               : std::string("")));
  for (const auto& attribution : image.attribution()) {
    collection_image.attribution.push_back(attribution.text());
  }
  collection_image.attribution_action_url = GURL(image.action_url());

  return collection_image;
}

AlbumInfo::AlbumInfo() = default;
AlbumInfo::AlbumInfo(const AlbumInfo&) = default;
AlbumInfo::AlbumInfo(AlbumInfo&&) = default;
AlbumInfo::~AlbumInfo() = default;

AlbumInfo& AlbumInfo::operator=(const AlbumInfo&) = default;
AlbumInfo& AlbumInfo::operator=(AlbumInfo&&) = default;

bool operator==(const AlbumInfo& lhs, const AlbumInfo& rhs) {
  return lhs.album_id == rhs.album_id &&
         lhs.photo_container_id == rhs.photo_container_id &&
         lhs.album_name == rhs.album_name &&
         lhs.preview_image_url == rhs.preview_image_url;
}

bool operator!=(const AlbumInfo& lhs, const AlbumInfo& rhs) {
  return !(lhs == rhs);
}

AlbumInfo AlbumInfo::CreateFromProto(
    const ntp::background::AlbumMetaData& album) {
  AlbumInfo album_info;
  album_info.album_id = album.album_id();
  album_info.photo_container_id = album.photo_container_id();
  album_info.album_name = album.album_name();
  album_info.preview_image_url = GURL(album.banner_image_url());

  return album_info;
}

AlbumPhoto::AlbumPhoto() = default;
// TODO(crbug.com/851990) Handle urls with existing image options.
AlbumPhoto::AlbumPhoto(const std::string& album_id,
                       const std::string& photo_container_id,
                       const std::string& photo_url,
                       const std::string& default_image_options)
    : album_id(album_id),
      photo_container_id(photo_container_id),
      thumbnail_photo_url(GURL(photo_url)),
      photo_url(GURL(photo_url + ((photo_url.find('=') == std::string::npos)
                                      ? default_image_options
                                      : std::string()))) {}
AlbumPhoto::AlbumPhoto(const AlbumPhoto&) = default;
AlbumPhoto::AlbumPhoto(AlbumPhoto&&) = default;
AlbumPhoto::~AlbumPhoto() = default;

AlbumPhoto& AlbumPhoto::operator=(const AlbumPhoto&) = default;
AlbumPhoto& AlbumPhoto::operator=(AlbumPhoto&&) = default;

bool operator==(const AlbumPhoto& lhs, const AlbumPhoto& rhs) {
  return lhs.thumbnail_photo_url == rhs.thumbnail_photo_url &&
         lhs.photo_url == rhs.photo_url;
}

bool operator!=(const AlbumPhoto& lhs, const AlbumPhoto& rhs) {
  return !(lhs == rhs);
}

ErrorInfo::ErrorInfo() : net_error(0), error_type(ErrorType::NONE) {}
ErrorInfo::ErrorInfo(const ErrorInfo&) = default;
ErrorInfo::ErrorInfo(ErrorInfo&&) = default;
ErrorInfo::~ErrorInfo() = default;

ErrorInfo& ErrorInfo::operator=(const ErrorInfo&) = default;
ErrorInfo& ErrorInfo::operator=(ErrorInfo&&) = default;

void ErrorInfo::ClearError() {
  error_type = ErrorType::NONE;
  net_error = 0;
}
