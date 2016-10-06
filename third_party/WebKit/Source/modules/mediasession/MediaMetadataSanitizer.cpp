// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaMetadataSanitizer.h"

#include "modules/mediasession/MediaArtwork.h"
#include "modules/mediasession/MediaMetadata.h"
#include "public/platform/WebIconSizesParser.h"
#include "public/platform/WebSize.h"
#include "url/url_constants.h"

namespace blink {

namespace {

// Constants used by the sanitizer, must be consistent with
// content::MediaMetdataSanitizer.

// Maximum length of all strings inside MediaMetadata when it is sent over mojo.
const size_t kMaxStringLength = 4 * 1024;

// Maximum type length of MediaArtwork, which conforms to RFC 4288
// (https://tools.ietf.org/html/rfc4288).
const size_t kMaxArtworkTypeLength = 2 * 127 + 1;

// Maximum number of artwork images inside the MediaMetadata.
const size_t kMaxNumberOfArtworkImages = 10;

// Maximum of sizes in an artwork image.
const size_t kMaxNumberOfArtworkSizes = 10;

bool checkArtworkSrcSanity(const KURL& src) {
  if (!src.isValid())
    return false;
  if (!src.protocolIs(url::kHttpScheme) && !src.protocolIs(url::kHttpsScheme) &&
      !src.protocolIs(url::kDataScheme)) {
    return false;
  }
  DCHECK(src.getString().is8Bit());
  if (src.getString().length() > url::kMaxURLChars)
    return false;
  return true;
}

blink::mojom::blink::MediaImagePtr sanitizeArtworkAndConvertToMojo(
    const MediaArtwork* artwork) {
  DCHECK(artwork);

  blink::mojom::blink::MediaImagePtr mojoImage;

  KURL url = KURL(ParsedURLString, artwork->src());
  if (!checkArtworkSrcSanity(url))
    return mojoImage;

  mojoImage = blink::mojom::blink::MediaImage::New();
  mojoImage->src = url;
  mojoImage->type = artwork->type().left(kMaxArtworkTypeLength);
  for (const auto& webSize :
       WebIconSizesParser::parseIconSizes(artwork->sizes())) {
    mojoImage->sizes.append(webSize);
    if (mojoImage->sizes.size() == kMaxNumberOfArtworkSizes)
      break;
  }
  return mojoImage;
}

}  // anonymous namespace

blink::mojom::blink::MediaMetadataPtr
MediaMetadataSanitizer::sanitizeAndConvertToMojo(
    const MediaMetadata* metadata) {
  blink::mojom::blink::MediaMetadataPtr mojoMetadata;
  if (!metadata)
    return mojoMetadata;

  mojoMetadata = blink::mojom::blink::MediaMetadata::New();

  mojoMetadata->title = metadata->title().left(kMaxStringLength);
  mojoMetadata->artist = metadata->artist().left(kMaxStringLength);
  mojoMetadata->album = metadata->album().left(kMaxStringLength);

  for (const auto artwork : metadata->artwork()) {
    blink::mojom::blink::MediaImagePtr mojoImage =
        sanitizeArtworkAndConvertToMojo(artwork.get());
    if (!mojoImage.is_null())
      mojoMetadata->artwork.append(std::move(mojoImage));
    if (mojoMetadata->artwork.size() == kMaxNumberOfArtworkImages)
      break;
  }
  return mojoMetadata;
}

}  // namespace blink
