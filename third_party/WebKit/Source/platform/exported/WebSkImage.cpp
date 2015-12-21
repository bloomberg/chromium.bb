// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebSkImage.h"

#include "wtf/PassRefPtr.h"

namespace blink {

WebSkImage::WebSkImage()
{
}

WebSkImage::WebSkImage(const WebSkImage& image)
{
    m_image = image.m_image;
}

WebSkImage::~WebSkImage()
{
    m_image.reset();
}

int WebSkImage::width() const
{
    return m_image->width();
}

int WebSkImage::height() const
{
    return m_image->height();
}

bool WebSkImage::readPixels(const SkImageInfo& dstInfo,
    void* dstPixels,
    size_t dstRowBytes,
    int srcX,
    int srcY) const
{
    return m_image->readPixels(dstInfo, dstPixels, dstRowBytes, srcX, srcY);
}

WebSkImage::WebSkImage(const PassRefPtr<SkImage>& image)
    : m_image(image)
{
}

WebSkImage& WebSkImage::operator=(const WTF::PassRefPtr<SkImage>& image)
{
    m_image = image;
    return *this;
}

} // namespace blink
