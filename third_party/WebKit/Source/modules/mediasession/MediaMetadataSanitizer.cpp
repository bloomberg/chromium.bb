// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaMetadataSanitizer.h"

#include "modules/mediasession/MediaImage.h"
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

// Maximum type length of MediaImage, which conforms to RFC 4288
// (https://tools.ietf.org/html/rfc4288).
const size_t kMaxImageTypeLength = 2 * 127 + 1;

// Maximum number of MediaImages inside the MediaMetadata.
const size_t kMaxNumberOfMediaImages = 10;

// Maximum of sizes in a MediaImage.
const size_t kMaxNumberOfImageSizes = 10;

bool checkMediaImageSrcSanity(const KURL& src) {
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

// Sanitize MediaImage and do mojo serialization. Returns null when
// |image.src()| is bad.
blink::mojom::blink::MediaImagePtr sanitizeMediaImageAndConvertToMojo(
    const MediaImage* image) {
  DCHECK(image);

  blink::mojom::blink::MediaImagePtr mojoImage;

  KURL url = KURL(ParsedURLString, image->src());
  if (!checkMediaImageSrcSanity(url))
    return mojoImage;

  mojoImage = blink::mojom::blink::MediaImage::New();
  mojoImage->src = url;
  mojoImage->type = image->type().left(kMaxImageTypeLength);
  for (const auto& webSize :
       WebIconSizesParser::parseIconSizes(image->sizes())) {
    mojoImage->sizes.append(webSize);
    if (mojoImage->sizes.size() == kMaxNumberOfImageSizes)
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

  for (const auto image : metadata->artwork()) {
    blink::mojom::blink::MediaImagePtr mojoImage =
        sanitizeMediaImageAndConvertToMojo(image.get());
    if (!mojoImage.is_null())
      mojoMetadata->artwork.append(std::move(mojoImage));
    if (mojoMetadata->artwork.size() == kMaxNumberOfMediaImages)
      break;
  }
  return mojoMetadata;
}

}  // namespace blink
