// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_SESSION_MOJOM_TRAITS_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_SESSION_MOJOM_TRAITS_H_

#include "services/media_session/public/mojom/media_session.mojom.h"

namespace mojo {

template <>
struct StructTraits<media_session::mojom::MediaImageDataView,
                    media_session::MediaMetadata::MediaImage> {
  static const GURL& src(
      const media_session::MediaMetadata::MediaImage& image) {
    return image.src;
  }

  static const base::string16& type(
      const media_session::MediaMetadata::MediaImage& image) {
    return image.type;
  }

  static const std::vector<gfx::Size>& sizes(
      const media_session::MediaMetadata::MediaImage& image) {
    return image.sizes;
  }

  static bool Read(media_session::mojom::MediaImageDataView data,
                   media_session::MediaMetadata::MediaImage* out);
};

template <>
struct StructTraits<media_session::mojom::MediaMetadataDataView,
                    media_session::MediaMetadata> {
  static const base::string16& title(
      const media_session::MediaMetadata& metadata) {
    return metadata.title;
  }

  static const base::string16& artist(
      const media_session::MediaMetadata& metadata) {
    return metadata.artist;
  }

  static const base::string16& album(
      const media_session::MediaMetadata& metadata) {
    return metadata.album;
  }

  static const std::vector<media_session::MediaMetadata::MediaImage>& artwork(
      const media_session::MediaMetadata& metadata) {
    return metadata.artwork;
  }

  static bool Read(media_session::mojom::MediaMetadataDataView data,
                   media_session::MediaMetadata* out);
};

}  // namespace mojo

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_SESSION_MOJOM_TRAITS_H_
