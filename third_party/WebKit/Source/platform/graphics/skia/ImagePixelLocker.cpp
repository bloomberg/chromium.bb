// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/skia/ImagePixelLocker.h"

#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

namespace {

bool infoIsCompatible(const SkImageInfo& info, SkAlphaType alphaType, SkColorType colorType)
{
    ASSERT(alphaType != kUnknown_SkAlphaType);

    if (info.colorType() != colorType)
        return false;

    // kOpaque_SkAlphaType works regardless of the requested alphaType.
    return info.alphaType() == alphaType || info.alphaType() == kOpaque_SkAlphaType;
}

} // anonymous namespace

ImagePixelLocker::ImagePixelLocker(PassRefPtr<const SkImage> image, SkAlphaType alphaType,
    SkColorType colorType)
    : m_image(image)
{
    SkImageInfo info;
    size_t rowBytes;

    // If the image has in-RAM pixels and their format matches, use them directly.
    // TODO(fmalita): All current clients expect packed pixel rows.  Maybe we could update them
    // to support arbitrary rowBytes, and relax the check below.
    m_pixels = m_image->peekPixels(&info, &rowBytes);
    if (m_pixels
        && infoIsCompatible(info, alphaType, colorType)
        && rowBytes == info.minRowBytes()) {
        return;
    }

    m_pixels = nullptr;

    // No luck, we need to read the pixels into our local buffer.
    info = SkImageInfo::Make(m_image->width(), m_image->height(), colorType, alphaType);
    rowBytes = info.minRowBytes();
    size_t size = info.getSafeSize(rowBytes);
    if (0 == size)
        return;

    m_pixelStorage.reset(size); // this will throw on failure

    SkPixmap pixmap(info, m_pixelStorage.get(), rowBytes);

    if (!m_image->readPixels(pixmap, 0, 0))
        return;

    m_pixels = m_pixelStorage.get();
}

} // namespace blink
