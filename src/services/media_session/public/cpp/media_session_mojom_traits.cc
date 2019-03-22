// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_session_mojom_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media_session::mojom::MediaImageDataView,
                  media_session::MediaMetadata::MediaImage>::
    Read(media_session::mojom::MediaImageDataView data,
         media_session::MediaMetadata::MediaImage* out) {
  if (!data.ReadSrc(&out->src))
    return false;
  if (!data.ReadType(&out->type))
    return false;
  if (!data.ReadSizes(&out->sizes))
    return false;

  return true;
}

// static
bool StructTraits<media_session::mojom::MediaMetadataDataView,
                  media_session::MediaMetadata>::
    Read(media_session::mojom::MediaMetadataDataView data,
         media_session::MediaMetadata* out) {
  if (!data.ReadTitle(&out->title))
    return false;
  if (!data.ReadArtist(&out->artist))
    return false;
  if (!data.ReadAlbum(&out->album))
    return false;
  if (!data.ReadArtwork(&out->artwork))
    return false;

  return true;
}

}  // namespace mojo
