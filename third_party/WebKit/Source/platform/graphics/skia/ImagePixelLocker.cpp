// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/skia/ImagePixelLocker.h"

#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

namespace {

bool infoIsCompatible(const SkImageInfo& info, SkAlphaType alphaType)
{
    ASSERT(alphaType != kUnknown_SkAlphaType);

    // We only use/support kN32_SkColorType at this time.
    if (info.colorType() != kN32_SkColorType)
        return false;

    // kOpaque_SkAlphaType works regardless of the requested alphaType.
    return info.alphaType() == alphaType || info.alphaType() == kOpaque_SkAlphaType;
}

} // anonymous namespace

ImagePixelLocker::ImagePixelLocker(PassRefPtr<const SkImage> image, SkAlphaType alphaType)
    : m_image(image)
{
    SkImageInfo info;
    size_t imageRowBytes;

    // If the image has in-RAM pixels and their format matches, use them directly.
    // TODO(fmalita): All current clients expect packed pixel rows.  Maybe we could update them
    // to support arbitrary rowBytes, and relax the check below.
    m_pixels = m_image->peekPixels(&info, &imageRowBytes);
    if (m_pixels && infoIsCompatible(info, alphaType) && imageRowBytes == info.minRowBytes())
        return;

    // No luck, we need to read the pixels into our local buffer.
    info = SkImageInfo::MakeN32(m_image->width(), m_image->height(), alphaType);
    if (!m_pixelStorage.tryAlloc(info) || !m_image->readPixels(m_pixelStorage, 0, 0)) {
        m_pixels = nullptr;
        return;
    }

    m_pixels = m_pixelStorage.addr();
}

} // namespace blink
