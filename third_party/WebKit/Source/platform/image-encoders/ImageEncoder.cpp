// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/image-encoders/ImageEncoder.h"

#include "platform/MIMETypeRegistry.h"
#include "platform/image-encoders/skia/JPEGImageEncoder.h"
#include "platform/image-encoders/skia/PNGImageEncoder.h"
#include "platform/image-encoders/skia/WEBPImageEncoder.h"
#include "wtf/Vector.h"
#include "wtf/text/Base64.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

template<typename T>
bool encodeImage(T& source, const String& mimeType, const double* quality, Vector<char>* output)
{
    Vector<unsigned char>* encodedImage = reinterpret_cast<Vector<unsigned char>*>(output);

    int compressionQuality = -1;
    if (quality && 0 <= *quality && *quality <= 1)
        compressionQuality = static_cast<int>(*quality * 100 + 0.5);

    if (mimeType == "image/jpeg") {
        if (compressionQuality == -1)
            compressionQuality = JPEGImageEncoder::DefaultCompressionQuality;
        return JPEGImageEncoder::encode(source, compressionQuality, encodedImage);
    }
    if (mimeType == "image/webp") {
        if (compressionQuality == -1)
            compressionQuality = WEBPImageEncoder::DefaultCompressionQuality;
        return WEBPImageEncoder::encode(source, compressionQuality, encodedImage);
    }
    ASSERT(mimeType == "image/png");
    return PNGImageEncoder::encode(source, encodedImage);
}

} // namespace

String ImageEncoder::toDataURL(const SkBitmap& bitmap, const String& mimeType, const double* quality)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    Vector<char> encodedImage;
    if (!encodeImage(bitmap, mimeType, quality, &encodedImage))
        return "data:,";

    return "data:" + mimeType + ";base64," + base64Encode(encodedImage);
}

String ImageEncoder::toDataURL(const RawImageBytes& rawImageBytes, const String& mimeType, const double* quality)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    Vector<char> encodedImage;
    if (!encodeImage(rawImageBytes, mimeType, quality, &encodedImage))
        return "data:,";

    return "data:" + mimeType + ";base64," + base64Encode(encodedImage);
}

} // namespace blink
